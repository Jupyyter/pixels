// text/plain
// entity.cpp
#include "Entities/Entity.hpp"
#include "Entities/EntitySystem.hpp"
#include "Weapon.hpp"
#include "ParticleWorld.hpp"
#include "RigidBody.hpp"
#include "Constants.hpp"
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <queue>
#include <map>

constexpr float PI       = 3.14159265358979323846f;
constexpr float NO_GROUND = 1e9f;
constexpr float TARGET_CAST_UP   = 40.0f;
constexpr float TARGET_CAST_DOWN = 80.0f;

inline float clamp01(float t)       { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }
inline float lerp(float a, float b, float t) { t = clamp01(t); return a + t * (b - a); }
inline float length(sf::Vector2f v) { return std::hypot(v.x, v.y); }

Entity::Entity(b2WorldId physWorld, EntitySystem* sys, float x, float y, const EntityDefinition& def, bool isPlayer)
    // Initialize member variables from the definition. colHalfW/H are derived from the colliderRect's size.
    : physicsWorldId(physWorld), system(sys), defName(def.name), 
      colHalfW(def.colliderRect.size.x / 2.0f), colHalfH(def.colliderRect.size.y / 2.0f),
      colliderRadius(def.colliderRadius), hipBaseY(def.hipBaseY), armBaseY(def.armBaseY), uprightMult(def.uprightMultiplier)
{
    // Calculate the collider's center point in the sprite's local coordinates.
    // This is the single most important value for aligning physics and graphics.
    sf::Vector2f colliderCenter = {
        def.colliderRect.position.x + colHalfW,
        def.colliderRect.position.y + colHalfH
    };

    b2BodyDef bdef      = b2DefaultBodyDef();
    bdef.type           = b2_dynamicBody;
    // The physics body's center is the sprite's spawn position (x, y) plus the offset to the collider's center.
    bdef.position.x     = (x + colliderCenter.x) * P2M;
    bdef.position.y     = (y + colliderCenter.y) * P2M;
    bdef.linearDamping  = 1.0f;
    bdef.angularDamping = 10.0f + (5.0f * uprightMult);
    bodyId = b2CreateBody(physicsWorldId, &bdef);
    
    createMainCollider(10.0f, 0.0f);

    pCtrl.isPlayer = isPlayer;

    if (!def.texturePath.empty()) {
        try   { sprite.texture = std::make_shared<sf::Texture>(def.texturePath); }
        catch (...) { sprite.texture = system->getDefaultTexture(); }
        sprite.texturePath = def.texturePath;
    } else {
        sprite.texture = system->getDefaultTexture();
    }
    
    sprite.sprite.emplace(*sprite.texture);
    sprite.frameWidth  = def.frameWidth;
    sprite.frameHeight = def.frameHeight;
    // The sprite's visual origin is set to the collider's center to ensure they rotate together perfectly.
    sprite.sprite->setOrigin(colliderCenter);
    sprite.animations = def.animations;
    
    handA.offset    = {4.0f, armBaseY};
    handB.offset    = {-4.0f, armBaseY};
}

void Entity::createMainCollider(float density, float friction) {
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density           = density; 
    shapeDef.material.friction = friction; 
    shapeDef.filter.categoryBits = 0x0002;
    shapeDef.filter.maskBits = 0x0001 | 0x0002 | 0x0004 | 0x0008; 
    
    // Decrease the box polygon size by the radius amount to keep the overall size exactly the same!
    float effHalfW = std::max(0.0f, colHalfW - colliderRadius);
    float effHalfH = std::max(0.0f, colHalfH - colliderRadius);
    
    b2Polygon box = b2MakeBox(effHalfW * P2M, effHalfH * P2M);
    box.radius = colliderRadius * P2M;
    b2CreatePolygonShape(bodyId, &shapeDef, &box);
}

Entity::~Entity() {
    if (bodyId.index1 != 0 && b2Body_IsValid(bodyId)) b2DestroyBody(bodyId);
    if (b2Body_IsValid(handA.ragdollBodyId)) b2DestroyBody(handA.ragdollBodyId);
    if (b2Body_IsValid(handB.ragdollBodyId)) b2DestroyBody(handB.ragdollBodyId);
}

void Entity::triggerSwing(sf::Vector2f targetWorldPos) {
    if (pCtrl.equippedWeapon && !pCtrl.isSwinging) {
        if (pCtrl.equippedWeapon->isGun) return;
        pCtrl.isSwinging = true;
        pCtrl.swingTimer = 0.0f;
        pCtrl.swingEffectApplied = false;
        pCtrl.swingTarget = targetWorldPos;
        pCtrl.swingRandomness = ((rand() % 100) / 100.0f) * 0.4f - 0.2f;
    }
}

void Entity::updateInput(float dt, sf::Vector2f mouseWorldPos, RigidBodySystem& rbs, ParticleWorld& pw) {
    dt = std::min(dt, 0.05f);
    
    if (pCtrl.equippedWeapon && pCtrl.equippedWeapon->isDestroyed) pCtrl.equippedWeapon = nullptr; 
    if (pCtrl.uprightStunTimer > 0.0f) pCtrl.uprightStunTimer -= dt;

    b2Vec2 vel = b2Body_GetLinearVelocity(bodyId);
    b2Vec2 pos = b2Body_GetPosition(bodyId);
    sf::Vector2f bodyPos(pos.x * M2P, pos.y * M2P);
    sf::Vector2f footPos(bodyPos.x, bodyPos.y + colHalfH + 1.0f);

    float dir = 0.0f;
    bool wPressed = false;
    bool rightClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
    bool leftClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    bool fPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F);
    bool ePressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E);

    if (pCtrl.isPlayer) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) dir = -1.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) dir =  1.0f;
        wPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
        
        bool currentS = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S);
        if (currentS && !pCtrl.sPressed) pCtrl.uprightStunTimer = 0.25f; 
        pCtrl.sPressed = currentS;
    } else {
        pCtrl.sPressed = false;
        
        if (pCtrl.hasTarget && !pCtrl.path.empty() && pCtrl.pathIndex < pCtrl.path.size()) {
            PathNodeData nextNode = pCtrl.path[pCtrl.pathIndex];
            if (pCtrl.isGrounded && nextNode.pos.y > footPos.y + 20.0f) {
                if (!nextNode.isJump && !nextNode.isJumpTakeoff) pCtrl.uprightStunTimer = 0.25f;
            }
        }

        if (!pCtrl.homePosSet) {
            pCtrl.homePos = footPos;
            pCtrl.homePosSet = true;
            pCtrl.idleWaitTimer = ((std::rand() % 100) / 100.0f) * 4.0f + 2.0f;
            pCtrl.isWandering = false;
        }

        if (!pCtrl.hasTarget) {
            if (pCtrl.idleWaitTimer > 0.0f) {
                pCtrl.idleWaitTimer -= dt;
                dir = 0.0f;
                pCtrl.isWandering = false;
            } else {
                if (!pCtrl.isWandering) {
                    pCtrl.isWandering = true;
                    float offsetAmount = (50.0f + ((std::rand() % 100) / 100.0f) * 100.0f);
                    float dirX = (std::rand() % 2 == 0) ? 1.0f : -1.0f;
                    
                    if (footPos.x > pCtrl.homePos.x + 100.0f) dirX = -1.0f;
                    if (footPos.x < pCtrl.homePos.x - 100.0f) dirX = 1.0f;

                    pCtrl.wanderDir = dirX;
                    
                    if (system->isGlobalGraphBuilt()) {
                        int startNode = system->getClosestNode(footPos);
                        if (startNode != -1) {
                            std::vector<int> reachableNodes;
                            std::queue<std::pair<int, int>> q;
                            std::map<int, bool> visited;
                            q.push({startNode, 0});
                            visited[startNode] = true;
                            
                            int targetWanderEdgeDist = std::max(5, static_cast<int>(offsetAmount / 16.0f)); 
                            
                            while (!q.empty()) {
                                auto [curr, edgeDist] = q.front();
                                q.pop();
                                
                                if (edgeDist > targetWanderEdgeDist / 2 && edgeDist <= targetWanderEdgeDist * 2) {
                                    float distFromHome = std::abs(system->getGlobalNavGraph()[curr].pos.x - pCtrl.homePos.x);
                                    if (distFromHome < 250.0f) { 
                                        float signToNode = (system->getGlobalNavGraph()[curr].pos.x > footPos.x) ? 1.0f : -1.0f;
                                        if (signToNode == dirX || std::rand() % 3 == 0) {
                                            reachableNodes.push_back(curr);
                                        }
                                    }
                                }
                                
                                if (edgeDist < targetWanderEdgeDist * 2) {
                                    for (int nxt : system->getGlobalNavGraph()[curr].neighbors) {
                                        if (!visited[nxt]) {
                                            visited[nxt] = true;
                                            q.push({nxt, edgeDist + 1});
                                        }
                                    }
                                }
                            }
                            
                            if (!reachableNodes.empty()) {
                                int rNode = reachableNodes[std::rand() % reachableNodes.size()];
                                sf::Vector2f candidateDest = system->getGlobalNavGraph()[rNode].pos;
                                
                                pCtrl.targetPos = candidateDest;
                                pCtrl.path = system->findPath(footPos, candidateDest);
                                if (!pCtrl.path.empty()) {
                                    pCtrl.path.push_back({candidateDest, false, false, false, 0.0f});
                                    pCtrl.pathIndex = 0;
                                    pCtrl.pathRecalcTimer = 0.5f;
                                    pCtrl.stuckTimer = 0.0f;
                                    pCtrl.lastPos = bodyPos;
                                    pCtrl.hasTarget = true;
                                } else {
                                    pCtrl.wanderTimer = offsetAmount / (pCtrl.moveSpeed * 0.7f); 
                                    pCtrl.physicsStuckTimer = 0.0f;
                                }
                            } else {
                                pCtrl.wanderTimer = offsetAmount / (pCtrl.moveSpeed * 0.7f); 
                                pCtrl.physicsStuckTimer = 0.0f;
                            }
                        } else {
                            pCtrl.wanderTimer = offsetAmount / (pCtrl.moveSpeed * 0.7f); 
                            pCtrl.physicsStuckTimer = 0.0f;
                        }
                    } else {
                        pCtrl.wanderTimer = offsetAmount / (pCtrl.moveSpeed * 0.7f); 
                        pCtrl.physicsStuckTimer = 0.0f;
                    }
                }

                if (pCtrl.isWandering && !pCtrl.hasTarget) {
                    pCtrl.wanderTimer -= dt;
                    dir = pCtrl.wanderDir;

                    if (std::abs(vel.x) < 0.2f && pCtrl.isGrounded) {
                        pCtrl.physicsStuckTimer += dt;
                    } else {
                        pCtrl.physicsStuckTimer = 0.0f;
                    }

                    bool abortWander = false;
                    if (pCtrl.wanderTimer <= 0.0f) abortWander = true;
                    
                    if (pCtrl.physicsStuckTimer > 0.3f) {
                        wPressed = true;
                        if (pCtrl.physicsStuckTimer > 1.2f) abortWander = true;
                    }

                    float castX = footPos.x + dir * 16.0f;
                    float fY = system->groundCastY(castX, footPos.y - 12.0f, TARGET_CAST_UP + TARGET_CAST_DOWN, pw, false);
                    if (fY >= NO_GROUND || fY > footPos.y + 16.0f) {
                        abortWander = true;
                    } else if (fY < footPos.y - 8.0f) { 
                        if (pCtrl.isGrounded && fY > footPos.y - 24.0f) wPressed = true; 
                        else abortWander = true; 
                    }

                    if (abortWander) {
                        dir = 0.0f;
                        pCtrl.isWandering = false;
                        pCtrl.idleWaitTimer = ((std::rand() % 100) / 100.0f) * 4.0f + 2.0f;
                    }
                }
            }
        }
        
        if (pCtrl.hasTarget) {
            sf::Vector2f tPos = pCtrl.targetPos;
            system->addDebugLine(tPos + sf::Vector2f(-4, 0), tPos + sf::Vector2f(4, 0), sf::Color::Cyan);
            system->addDebugLine(tPos + sf::Vector2f(0, -4), tPos + sf::Vector2f(0, 4), sf::Color::Cyan);

            pCtrl.pathRecalcTimer -= dt;
            
            if (std::hypot(bodyPos.x - pCtrl.lastPos.x, bodyPos.y - pCtrl.lastPos.y) < 2.0f) pCtrl.stuckTimer += dt;
            else { pCtrl.stuckTimer = 0.0f; pCtrl.lastPos = bodyPos; }

            bool needsRecalc = false;
            if (pCtrl.stuckTimer > 0.5f && pCtrl.isGrounded) { needsRecalc = true; pCtrl.stuckTimer = 0.0f; }

            if (!pCtrl.path.empty() && pCtrl.pathIndex < pCtrl.path.size()) {
                float dy = footPos.y - pCtrl.path[pCtrl.pathIndex].pos.y;
                if (dy > 40.0f && pCtrl.isGrounded) needsRecalc = true;
            }

            if ((needsRecalc || pCtrl.pathRecalcTimer <= 0.0f) && pCtrl.isGrounded) {
                pCtrl.pathRecalcTimer = 0.5f;
                if (std::hypot(pCtrl.targetPos.x - footPos.x, pCtrl.targetPos.y - footPos.y) > 24.0f) {
                    std::vector<PathNodeData> newPath = system->findPath(footPos, pCtrl.targetPos);
                    
                    bool destinationSevered = false;
                    if (!newPath.empty()) {
                        float distToEnd = std::hypot(newPath.back().pos.x - pCtrl.targetPos.x, newPath.back().pos.y - pCtrl.targetPos.y);
                        if (distToEnd > 24.0f) destinationSevered = true; 
                    }
                    
                    if (newPath.empty() || (destinationSevered && pCtrl.isWandering)) {
                        if (pCtrl.isWandering) pCtrl.pathIndex = pCtrl.path.size();
                        else {
                            pCtrl.path = std::move(newPath);
                            if (!pCtrl.path.empty()) pCtrl.path.push_back({pCtrl.targetPos, false, false, false, 0.0f});
                            pCtrl.pathIndex = 0;
                        }
                    } else {
                        pCtrl.path = std::move(newPath);
                        if (!pCtrl.path.empty() && !destinationSevered) pCtrl.path.push_back({pCtrl.targetPos, false, false, false, 0.0f});
                        pCtrl.pathIndex = 0;
                    }
                } else {
                    pCtrl.pathIndex = pCtrl.path.size(); 
                }
            }

            if (!pCtrl.path.empty() && pCtrl.pathIndex < pCtrl.path.size()) {
                b2Body_SetAwake(bodyId, true);

                while (pCtrl.pathIndex < pCtrl.path.size()) {
                    PathNodeData nextNode = pCtrl.path[pCtrl.pathIndex];
                    float dist = std::hypot(nextNode.pos.x - footPos.x, nextNode.pos.y - footPos.y);
                    
                    bool reached = false;
                    if (nextNode.isJumpTakeoff) {
                        if (std::abs(nextNode.pos.x - footPos.x) < 4.0f && std::abs(nextNode.pos.y - footPos.y) < 16.0f) reached = true;
                    } else {
                        if (dist < 12.0f) reached = true;
                        else if (pCtrl.isGrounded && std::abs(nextNode.pos.x - footPos.x) < 12.0f && std::abs(nextNode.pos.y - footPos.y) < 24.0f) reached = true;
                    }
                    
                    if (reached) pCtrl.pathIndex++;
                    else break;
                }

                if (pCtrl.pathIndex < pCtrl.path.size()) {
                    PathNodeData nextNode = pCtrl.path[pCtrl.pathIndex];
                    
                    if (pCtrl.pathIndex > 0) system->addDebugLine(footPos, nextNode.pos, sf::Color::Yellow);
                    for (size_t i = pCtrl.pathIndex; i + 1 < pCtrl.path.size(); ++i) {
                        system->addDebugLine(pCtrl.path[i].pos, pCtrl.path[i+1].pos, sf::Color::Yellow);
                    }
                    
                    float dx = nextNode.pos.x - footPos.x; 
                    if (dx > 2.0f) dir = 1.0f;
                    else if (dx < -2.0f) dir = -1.0f;
                    
                    bool needsJump = false;
                    
                    if (nextNode.isJump || nextNode.isFall) {
                        if (pCtrl.isGrounded) {
                            if (nextNode.isJump) needsJump = true;
                        }
                    } else {
                        int lookMax = std::min(static_cast<int>(pCtrl.path.size()), pCtrl.pathIndex + 2);
                        for (int i = pCtrl.pathIndex; i < lookMax; ++i) {
                            if (pCtrl.path[i].pos.y < footPos.y - 12.0f && std::abs(pCtrl.path[i].pos.x - footPos.x) < 32.0f && !pCtrl.path[i].isJump && !pCtrl.path[i].isFall) {
                                needsJump = true;
                                break;
                            }
                        }
                        if (std::abs(dx) > 4.0f && std::abs(vel.x) < 0.5f && pCtrl.stuckTimer > 0.1f) needsJump = true;
                    }
                    if (pCtrl.isGrounded && needsJump) wPressed = true;
                } else {
                    dir = 0.0f;
                    pCtrl.hasTarget = false;
                    if (pCtrl.isWandering) {
                        pCtrl.isWandering = false;
                        pCtrl.idleWaitTimer = ((std::rand() % 100) / 100.0f) * 4.0f + 2.0f;
                    }
                }
            } else {
                dir = 0.0f;
                pCtrl.hasTarget = false;
                if (pCtrl.isWandering) {
                    pCtrl.isWandering = false;
                    pCtrl.idleWaitTimer = ((std::rand() % 100) / 100.0f) * 4.0f + 2.0f;
                }
            }
        }
    } 

    b2Rot currentRot = b2Body_GetRotation(bodyId);
    float currentBodyAng = std::atan2(currentRot.s, currentRot.c);
    float maxAngle = 45.0f * PI / 180.0f;
    bool forceRagdoll = (!pCtrl.isRagdoll && std::abs(currentBodyAng) > maxAngle);

    if ((fPressed && !pCtrl.fPressedLastFrame) || forceRagdoll) {
        toggleRagdoll(forceRagdoll ? true : !pCtrl.isRagdoll);
    }
    pCtrl.fPressedLastFrame = fPressed;

    if (pCtrl.isRagdoll) {
        pCtrl.isAiming = false; pCtrl.isSwinging = false;
        pCtrl.leftClickPressedLastFrame = leftClick; pCtrl.wPressedLastFrame = wPressed;
        pCtrl.ePressedLastFrame = ePressed;
        return; 
    }

    b2Rot rot = b2Body_GetRotation(bodyId);
    float bodyAng = std::atan2(rot.s, rot.c);
    float angVel = b2Body_GetAngularVelocity(bodyId);
    if (std::abs(bodyAng) < 0.03f && std::abs(angVel) < 0.5f) {
        if (bodyAng != 0.0f || angVel != 0.0f) {
            b2Body_SetTransform(bodyId, b2Body_GetPosition(bodyId), b2MakeRot(0.0f));
            b2Body_SetAngularVelocity(bodyId, 0.0f);
            bodyAng = 0.0f; angVel = 0.0f;
        }
    }
    bool movingAway = (bodyAng * angVel > 0.0f);
    
    // Upright Multiplier amplifies the stiffness drastically!
    float stiffness = (movingAway ? 15.0f : 250.0f) * uprightMult;
    float damping = (movingAway ? 2.0f : 31.0f) * uprightMult;
    
    if (std::abs(bodyAng) > maxAngle * 0.7f) {
        float excess = (std::abs(bodyAng) - maxAngle * 0.7f) / (maxAngle * 0.3f);
        stiffness += excess * 600.0f * uprightMult; damping += excess * 40.0f * uprightMult;
    }

    float torque = (-bodyAng * stiffness - angVel * damping) * b2Body_GetMass(bodyId);
    b2Body_ApplyTorque(bodyId, torque, true);

    if (rightClick && pCtrl.equippedWeapon && pCtrl.isPlayer) {
        pCtrl.isAiming = true;
        pCtrl.aimTarget = mouseWorldPos;
        system->addDebugLine({bodyPos.x, bodyPos.y}, pCtrl.aimTarget, sf::Color::Magenta);
        
        if (pCtrl.equippedWeapon->isGun) {
            Weapon* w = static_cast<Weapon*>(pCtrl.equippedWeapon);
            if (pCtrl.fireTimer <= 0.0f) {
                bool canShoot = w->semiAuto ? (leftClick && !pCtrl.leftClickPressedLastFrame) : leftClick;
                if (canShoot) {
                    sf::Vector2f vbp(bodyPos.x, bodyPos.y + bob.offsetY + armBaseY);
                    sf::Vector2f handPos = vbp + handB.offset + handB.recoilPos;
                    float currentWeaponAngle = weaponAngle + handB.recoilAngle;
                    
                    w->fire(handPos, pCtrl.aimTarget, currentWeaponAngle, sprite.flipX, rbs, pw);
                    pCtrl.fireTimer = w->fireRate;

                    float wFinalAngle = currentWeaponAngle + (sprite.flipX ? -w->visualAngleOffset : w->visualAngleOffset);
                    float wRad = wFinalAngle * PI / 180.0f;
                    float dX = std::cos(wRad) * (sprite.flipX ? -1.0f : 1.0f);
                    float dY = std::sin(wRad) * (sprite.flipX ? -1.0f : 1.0f);
                    sf::Vector2f shootDir(dX, dY);
                    
                    float backwardForce = w->recoilForce * 120.0f;
                    handB.recoilVel -= shootDir * backwardForce;
                    
                    sf::Vector2f perpDir(-shootDir.y, shootDir.x);
                    float randLateral = ((std::rand() % 100) / 100.0f - 0.5f) * backwardForce * 1.5f;
                    handB.recoilVel += perpDir * randLateral;
                    handB.recoilAngularVel += ((std::rand() % 100) / 100.0f - 0.5f) * w->visualRecoilAngle * 80.0f;

                    float playerKickMult = 0.5f; float effectiveMass = 0.8f; 
                    vel.x += (-shootDir.x * w->recoilForce * playerKickMult) / effectiveMass;
                    vel.y += (-shootDir.y * w->recoilForce * playerKickMult) / effectiveMass;
                }
            }
        }
    } else {
        pCtrl.isAiming = false;
    }
    pCtrl.leftClickPressedLastFrame = leftClick;
    if (pCtrl.fireTimer > 0.0f) pCtrl.fireTimer -= dt;
    
    float speedFactor = 1.0f;
    
    if (pCtrl.isPlayer && pCtrl.isGrounded && dir != 0.0f) {
        float maxDx = 0.0f;
        int maxLook = 8;
        
        float baseCastFrom = bodyPos.y + colHalfH - 8.0f;
        float minFootY = bodyPos.y + colHalfH - 8.0f; 
        float maxFootY = bodyPos.y + colHalfH + 16.0f; 
        
        bool ignorePlatforms = (pCtrl.uprightStunTimer > 0.0f) || (vel.y < -1.0f);

        for (int i = 1; i <= maxLook; ++i) {
            float testX = bodyPos.x + dir * i;
            float gY = system->groundCastY(testX, baseCastFrom, TARGET_CAST_UP + TARGET_CAST_DOWN, pw, ignorePlatforms);
            
            // Only hit the brakes if a wall is in the way. 
            // Do NOT hit the brakes for cliffs/drop-offs!
            if (gY < minFootY) break; 
            maxDx = static_cast<float>(i);
        }
        
        speedFactor = maxDx / maxLook;
        if (speedFactor > 1.0f) speedFactor = 1.0f;

        b2QueryFilter filter = b2DefaultQueryFilter();
        filter.categoryBits = 0x0002;
        filter.maskBits = 0x0001 | 0x0002 | 0x0008; 

        b2Transform xf = b2Body_GetTransform(bodyId);
        float localX = dir * (colHalfW + 1.0f) * P2M;
        b2Vec2 localTop = {localX, (-colHalfH + 2.0f) * P2M};
        b2Vec2 localBottom = {localX, (colHalfH - 2.0f) * P2M};
        b2Vec2 worldTop = b2TransformPoint(xf, localTop);
        b2Vec2 worldBottom = b2TransformPoint(xf, localBottom);
        b2Vec2 sideTrans = {worldBottom.x - worldTop.x, worldBottom.y - worldTop.y};
        
        b2RayResult sideHit = b2World_CastRayClosest(physicsWorldId, worldTop, sideTrans, filter);
        if (sideHit.hit) {
            b2BodyId sideBody = b2Shape_GetBody(sideHit.shapeId);
            if (sideBody.index1 != bodyId.index1) { 
                if (checkSideSnag(dir, sideBody)) {
                    speedFactor = 0.0f;
                    vel.x = 0.0f; 
                }
            }
        }
    }

    float desiredVelX = dir * pCtrl.moveSpeed * speedFactor;
    
    if (!pCtrl.isSwinging && !pCtrl.isAiming) {
        if (dir < 0.0f) sprite.flipX = true;
        else if (dir > 0.0f) sprite.flipX = false;
    }

    if (pCtrl.landingTimer > 0.0f) {
        pCtrl.landingTimer -= dt;
        desiredVelX *= 0.1f; 
    }

    if (pCtrl.isGrounded) vel.x = lerp(vel.x, desiredVelX, dt * 8.0f);

    if (wPressed && !pCtrl.wPressedLastFrame && pCtrl.isGrounded && pCtrl.landingTimer <= 0.0f) {
        vel.y = pCtrl.jumpForce;
        
        if (pCtrl.isPlayer) {
            // Give the player a horizontal boost to jump further forward
            if (dir != 0.0f) {
                vel.x = dir * pCtrl.moveSpeed * 1.3f;
            }
        } else if (!pCtrl.path.empty() && pCtrl.pathIndex < pCtrl.path.size()) {
            PathNodeData nextNode = pCtrl.path[pCtrl.pathIndex];
            if (nextNode.isJump && std::abs(nextNode.requiredVx) > 0.1f) {
                vel.x = nextNode.requiredVx * P2M; 
            }
        }
    }
    pCtrl.wPressedLastFrame = wPressed;

    b2Body_SetLinearVelocity(bodyId, vel);

    if (pCtrl.isPlayer) {
        if (ePressed) {
            if (!pCtrl.ePressedLastFrame) {
                if (pCtrl.equippedWeapon) {
                    pCtrl.eHoldTimer = 0.0f; // Start charging the throw
                } else {
                    RigidBody* nearest = rbs.getNearestWeapon(bodyPos, 40.0f);
                    if (nearest) {
                        nearest->clearFromWorld(pw);
                        nearest->isEquipped = true;
                        b2Body_Disable(nearest->bodyId); 
                        pCtrl.equippedWeapon = nearest;
                        pCtrl.eHoldTimer = -1.0f; // Invalid state to prevent immediate charging if held
                    }
                }
            } else {
                if (pCtrl.equippedWeapon && pCtrl.eHoldTimer >= 0.0f) {
                    pCtrl.eHoldTimer += dt;
                }
            }
        } else if (pCtrl.ePressedLastFrame) {
            if (pCtrl.equippedWeapon && pCtrl.eHoldTimer >= 0.0f) {
                pCtrl.equippedWeapon->isEquipped = false;
                b2Body_Enable(pCtrl.equippedWeapon->bodyId);
                
                // Calculate linear charge from 0 to 1 second
                float charge = std::min(pCtrl.eHoldTimer, 1.0f);
                float throwSpeed = 10.0f + (charge * 45.0f);
                float spinSpeed = 5.0f + (charge * 25.0f);
                
                // Calculate aim-like position exactly as updateArms does
                sf::Vector2f vbp(bodyPos.x, bodyPos.y + bob.offsetY + armBaseY);
                sf::Vector2f shoulderPos = vbp + sf::Vector2f(0.0f, -4.0f);
                sf::Vector2f aimDir = mouseWorldPos - shoulderPos;
                float dist = std::max(std::hypot(aimDir.x, aimDir.y), 1.0f);
                sf::Vector2f aimNorm = {aimDir.x / dist, aimDir.y / dist};
                
                bool throwFlipX = (aimDir.x < 0.0f);
                
                sf::Vector2f desiredHandBOffset = (shoulderPos - vbp) + aimNorm * 9.0f;
                if (desiredHandBOffset.y > 0.0f) desiredHandBOffset.y = 0.0f;
                
                sf::Vector2f spawnPos = vbp + desiredHandBOffset;
                b2Vec2 dropPos = { spawnPos.x * P2M, spawnPos.y * P2M };
                
                float desiredWeaponAngle = 0.0f;
                if (!throwFlipX) {
                    desiredWeaponAngle = std::atan2(aimDir.y, aimDir.x) * 180.0f / PI;
                } else {
                    desiredWeaponAngle = -std::atan2(aimDir.y, -aimDir.x) * 180.0f / PI;
                }
                
                float finalAngleDeg = desiredWeaponAngle + (throwFlipX ? -pCtrl.equippedWeapon->visualAngleOffset : pCtrl.equippedWeapon->visualAngleOffset);
                float throwAngle = finalAngleDeg * PI / 180.0f;
                
                b2Body_SetTransform(pCtrl.equippedWeapon->bodyId, dropPos, b2MakeRot(throwAngle));
                
                // Throw towards cursor, linearly scaled power and rotation
                b2Body_SetLinearVelocity(pCtrl.equippedWeapon->bodyId, { aimNorm.x * throwSpeed, aimNorm.y * throwSpeed });
                b2Body_SetAngularVelocity(pCtrl.equippedWeapon->bodyId, throwFlipX ? -spinSpeed : spinSpeed);
                
                pCtrl.equippedWeapon = nullptr;
            }
            pCtrl.eHoldTimer = 0.0f;
        }
        pCtrl.ePressedLastFrame = ePressed;
    } else {
        pCtrl.ePressedLastFrame = false;
    }
}

void Entity::updateAnimations(float dt, ParticleWorld& pw) {
    b2Vec2 b2Pos = b2Body_GetPosition(bodyId);
    sf::Vector2f bodyPos(b2Pos.x * M2P, b2Pos.y * M2P);
    b2Vec2 b2Vel = b2Body_GetLinearVelocity(bodyId);
    b2Rot rot = b2Body_GetRotation(bodyId);
    float bodyAng = std::atan2(rot.s, rot.c);

    if (pCtrl.isRagdoll) {
        auto displaceSandAndApplyForce = [&](b2BodyId bId, sf::Vector2f halfSize, sf::Vector2f center, float angle) {
            if (!b2Body_IsValid(bId)) return;
            int minX = static_cast<int>(std::floor(center.x - halfSize.x - 2.0f));
            int maxX = static_cast<int>(std::ceil(center.x + halfSize.x + 2.0f));
            int minY = static_cast<int>(std::floor(center.y - halfSize.y - 2.0f));
            int maxY = static_cast<int>(std::ceil(center.y + halfSize.y + 2.0f));
            
            float r_cs = std::cos(-angle);
            float r_sn = std::sin(-angle);
            int sandCount = 0;
            
            for (int py = minY; py <= maxY; ++py) {
                for (int px = minX; px <= maxX; ++px) {
                    float dx = px - center.x;
                    float dy = py - center.y;
                    float rx = dx * r_cs - dy * r_sn;
                    float ry = dx * r_sn + dy * r_cs;
                    
                    if (std::abs(rx) <= halfSize.x + 0.5f && std::abs(ry) <= halfSize.y + 0.5f) {
                        BaseComponent* base = pw.get<BaseComponent>(px, py);
                        if (base && base->compMask != 0 && !base->flags.isRigidBodyPart) {
                            Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                            if (logic && logic->getGroup() == MaterialGroup::MovableSolid) {
                                sandCount++;
                                bool moved = false;
                                for (int d = 1; d <= 3; ++d) {
                                    if (pw.isEmpty(px, py - d)) { pw.moveParticle(px, py, px, py - d); moved = true; break; }
                                    if (pw.isEmpty(px - d, py)) { pw.moveParticle(px, py, px - d, py); moved = true; break; }
                                    if (pw.isEmpty(px + d, py)) { pw.moveParticle(px, py, px + d, py); moved = true; break; }
                                }
                                if (!moved && std::rand() % 100 < 10) {
                                    pw.removeParticle(px, py);
                                } else if (moved) {
                                    if (auto* kin = pw.get<KinematicsComponent>(px, py)) kin->isFreeFalling = true;
                                }
                            }
                        }
                    }
                }
            }
            
            if (sandCount > 0) {
                float mass = b2Body_GetMass(bId);
                b2Vec2 v = b2Body_GetLinearVelocity(bId);
                v.x *= 0.8f; v.y *= 0.8f; 
                b2Body_ApplyForceToCenter(bId, {0.0f, -(sandCount * 25.0f) * mass}, true);
                b2Body_SetLinearVelocity(bId, v);
            }
        };
        
        displaceSandAndApplyForce(bodyId, {colHalfW, colHalfH}, bodyPos, bodyAng);
        
        auto applySandToPart = [&](b2BodyId bId, sf::Vector2f halfSize) {
            if (b2Body_IsValid(bId)) {
                b2Vec2 p = b2Body_GetPosition(bId);
                b2Rot r = b2Body_GetRotation(bId);
                displaceSandAndApplyForce(bId, halfSize, {p.x * M2P, p.y * M2P}, std::atan2(r.s, r.c));
            }
        };
        applySandToPart(handA.ragdollBodyId, {2.0f, 2.0f});
        applySandToPart(handB.ragdollBodyId, {2.0f, 2.0f});

        sf::Vector2f vbp(bodyPos.x, bodyPos.y + bob.offsetY + armBaseY);
        
        auto syncHand = [&](ProceduralHand& hand) {
            if (b2Body_IsValid(hand.ragdollBodyId)) {
                b2Vec2 p = b2Body_GetPosition(hand.ragdollBodyId);
                hand.offset = {p.x * M2P - vbp.x, p.y * M2P - vbp.y};
            }
        };
        syncHand(handA);
        syncHand(handB);
        
        if (b2Body_IsValid(handB.ragdollBodyId)) {
            b2Rot handRot = b2Body_GetRotation(handB.ragdollBodyId);
            weaponAngle = std::atan2(handRot.s, handRot.c) * 180.0f / PI + (sprite.flipX ? -90.0f : 90.0f);
        } else {
            weaponAngle = bodyAng * 180.0f / PI + (sprite.flipX ? -90.0f : 90.0f);
        }
        
        handB.recoilPos = {0.0f, 0.0f};
        handB.recoilVel = {0.0f, 0.0f};
        return; 
    }

    float f = -bob.stiffness * bob.offsetY - bob.damping * bob.velocity;
    bob.velocity += f * dt;
    bob.offsetY  += bob.velocity * dt;

    if (sprite.animations.find(sprite.currentState) != sprite.animations.end()) {
        auto& as = sprite.animations[sprite.currentState];
        
        if (sprite.currentFrameIndex >= as.frameCount) {
            sprite.currentFrameIndex = 0;
        }

        sprite.frameTimer += dt;
        if (sprite.frameTimer >= as.frameDuration) {
            sprite.frameTimer = 0.0f;
            sprite.currentFrameIndex = (sprite.currentFrameIndex + 1) % as.frameCount;
        }
        int fx = (as.startFrameX + sprite.currentFrameIndex) * sprite.frameWidth;
        int fy = as.startFrameY * sprite.frameHeight;
        
        if (sprite.flipX)
            sprite.sprite->setTextureRect(sf::IntRect({fx + sprite.frameWidth, fy}, {-sprite.frameWidth, sprite.frameHeight}));
        else
            sprite.sprite->setTextureRect(sf::IntRect({fx, fy}, {sprite.frameWidth, sprite.frameHeight}));
    }

    updateArms(dt, bodyPos, bodyAng, !pCtrl.isGrounded, b2Vel, pw);

    float recoilStiff = 120.0f;
    float recoilDamp = 8.0f;
    
    sf::Vector2f springForce = -recoilStiff * handB.recoilPos - recoilDamp * handB.recoilVel;
    sf::Vector2f swirlForce = {-handB.recoilPos.y, handB.recoilPos.x};
    springForce += swirlForce * 20.0f;
    
    handB.recoilVel += springForce * dt;
    handB.recoilPos += handB.recoilVel * dt;
    
    float maxRecoilDist = 35.0f;
    float distSq = handB.recoilPos.x * handB.recoilPos.x + handB.recoilPos.y * handB.recoilPos.y;
    if (distSq > maxRecoilDist * maxRecoilDist) {
        float dist = std::sqrt(distSq);
        handB.recoilPos.x = (handB.recoilPos.x / dist) * maxRecoilDist;
        handB.recoilPos.y = (handB.recoilPos.y / dist) * maxRecoilDist;
    }

    float playerDragMult = 0.02f;
    float mass = b2Body_GetMass(bodyId);
    float originalMass = 0.8f; 
    b2Vec2 dragPlayerForce = {-springForce.x * playerDragMult * (mass / originalMass), -springForce.y * playerDragMult * (mass / originalMass)};
    b2Body_ApplyForceToCenter(bodyId, dragPlayerForce, true);
    
    float angSpringForce = -recoilStiff * handB.recoilAngle - recoilDamp * handB.recoilAngularVel;
    handB.recoilAngularVel += angSpringForce * dt;
    handB.recoilAngle += handB.recoilAngularVel * dt;

    if (handB.recoilAngle > 90.0f) handB.recoilAngle = 90.0f;
    if (handB.recoilAngle < -90.0f) handB.recoilAngle = -90.0f;

    b2ShapeId playerShapeId = b2_nullShapeId;
    int shapeCount = b2Body_GetShapeCount(bodyId);
    if (shapeCount > 0) {
        std::vector<b2ShapeId> shapes(shapeCount);
        b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
        playerShapeId = shapes[0];
    }

    if (b2Shape_IsValid(playerShapeId) && !pCtrl.isRagdoll) {
        bool overlappingPlatform = false;
        bool overlappingSolid = false;
        
        int pxMin = static_cast<int>(std::floor(bodyPos.x - colHalfW));
        int pxMax = static_cast<int>(std::ceil(bodyPos.x + colHalfW));
        int pyMin = static_cast<int>(std::floor(bodyPos.y - colHalfH)); 
        int pyMaxLimit = static_cast<int>(std::floor(bodyPos.y + colHalfH - 0.1f)); 

        for (int py = pyMin; py <= pyMaxLimit; ++py) {
            for (int px = pxMin; px <= pxMax; ++px) {
                if (pw.isEmpty(px, py)) continue;
                BaseComponent* b = pw.get<BaseComponent>(px, py);
                if (b && b->compMask != 0) {
                    if (b->compMask & COMP_PLATFORM) {
                        overlappingPlatform = true;
                    } else {
                        Particle* logic = MaterialRegistry[static_cast<int>(b->id)];
                        if (logic && logic->getGroup() != MaterialGroup::Liquid && logic->getGroup() != MaterialGroup::Gas) {
                            overlappingSolid = true;
                        }
                    }
                }
            }
        }

        b2Filter filter = b2Shape_GetFilter(playerShapeId);
        bool movingUp = !pCtrl.isGrounded && b2Vel.y < -1.0f;
        bool dropping = pCtrl.uprightStunTimer > 0.0f;
        
        bool ignorePlatformsPhysics = movingUp || dropping || (overlappingPlatform && !overlappingSolid);
        
        if (ignorePlatformsPhysics) {
            filter.maskBits = 0x0001 | 0x0002 | 0x0008; 
        } else {
            filter.maskBits = 0x0001 | 0x0002 | 0x0004 | 0x0008; 
        }
        filter.groupIndex = -1;
        b2Shape_SetFilter(playerShapeId, filter);
    }
}

void Entity::updateArms(float dt, sf::Vector2f bodyPos, float bodyAng, bool isAirborne, b2Vec2 b2Vel, ParticleWorld& pw) {
    float speedX = std::abs(b2Vel.x * M2P);
    
    if (std::abs(b2Vel.x) > 0.1f && !isAirborne) armSwing += dt * speedX * PI / strideDistance;
    else armSwing = lerp(armSwing, std::round(armSwing / PI) * PI, dt * 10.0f);

    bool facingLeft = sprite.flipX;
    float swingVal  = std::sin(armSwing);
    sf::Vector2f vbp(bodyPos.x, bodyPos.y + bob.offsetY + armBaseY);

    if (pCtrl.isSwinging) {
        pCtrl.swingTimer += dt;
        float t = clamp01(pCtrl.swingTimer / pCtrl.swingDuration);

        sprite.flipX = (pCtrl.swingTarget.x < bodyPos.x);

        if (t >= 0.5f && !pCtrl.swingEffectApplied) {
            pCtrl.swingEffectApplied = true;
            if (pCtrl.equippedWeapon && pCtrl.equippedWeapon->isWeapon) {
                Weapon* w = static_cast<Weapon*>(pCtrl.equippedWeapon);
                w->performSwingEffect(bodyPos, pCtrl.swingTarget, pw, *pw.getRigidBodySystem());
            }

            sf::Vector2f sw_dir = pCtrl.swingTarget - bodyPos;
            float dist = std::max(length(sw_dir), 1.0f);
            sf::Vector2f norm = { sw_dir.x / dist, sw_dir.y / dist };
            
            handB.recoilVel += norm * 250.0f; 
            handB.recoilAngularVel += ((std::rand() % 100) / 100.0f - 0.5f) * 800.0f;
        }

        if (t >= 1.0f) {
            pCtrl.isSwinging = false;
        }

        sf::Vector2f sw_dir = pCtrl.swingTarget - bodyPos;
        float dist = std::max(length(sw_dir), 1.0f);
        sf::Vector2f F = { sw_dir.x / dist, sw_dir.y / dist };
        
        float sign = sprite.flipX ? -1.0f : 1.0f;
        sf::Vector2f Perp = { F.y * sign, -F.x * sign };

        float r_val = pCtrl.swingRandomness; 
        float half_width = 16.0f + r_val * 10.0f; 
        float reach      = 22.0f + r_val * 10.0f;
        float base_dist  = 2.0f;

        float s = std::cos(t * PI); 
        
        float x_local = s * half_width;
        float y_local = reach * (1.0f - s * s);
        handB.offset = F * (base_dist + y_local) + Perp * x_local;

        float a = reach / (half_width * half_width);
        sf::Vector2f N_local(2.0f * a * x_local, 1.0f);
        float n_len = std::hypot(N_local.x, N_local.y);
        N_local.x /= n_len;
        N_local.y /= n_len;
        
        sf::Vector2f N_world = Perp * N_local.x + F * N_local.y;
        weaponAngle = std::atan2(N_world.y, N_world.x) * 180.0f / PI + 90.0f;

        float frontBaseX = sprite.flipX ? -4.0f : 4.0f;
        handA.offset.x = lerp(handA.offset.x, frontBaseX, dt * 15.0f);
        handA.offset.y = lerp(handA.offset.y, 0.0f, dt * 15.0f);

    } else if (pCtrl.isAiming) {
        sf::Vector2f shoulderPos = vbp + sf::Vector2f(0.0f, -4.0f); 
        sf::Vector2f aimDir = pCtrl.aimTarget - shoulderPos;
        
        sprite.flipX = (aimDir.x < 0.0f);
        facingLeft = sprite.flipX;
        
        float dist = std::max(length(aimDir), 1.0f);
        sf::Vector2f aimNorm = {aimDir.x / dist, aimDir.y / dist};
        
        float desiredWeaponAngle = 0.0f;
        if (!facingLeft) {
            desiredWeaponAngle = std::atan2(aimDir.y, aimDir.x) * 180.0f / PI;
        } else {
            desiredWeaponAngle = -std::atan2(aimDir.y, -aimDir.x) * 180.0f / PI;
        }
        
        float armExtension = 9.0f; 
        sf::Vector2f desiredHandBOffset = (shoulderPos - vbp) + aimNorm * armExtension;

        if (desiredHandBOffset.y > 0.0f) desiredHandBOffset.y = 0.0f;

        handB.offset.x = lerp(handB.offset.x, desiredHandBOffset.x, dt * 15.0f);
        handB.offset.y = lerp(handB.offset.y, desiredHandBOffset.y, dt * 15.0f);
        
        float angleDiff = desiredWeaponAngle - weaponAngle;
        while (angleDiff > 180.0f) angleDiff -= 360.0f;
        while (angleDiff < -180.0f) angleDiff += 360.0f;
        weaponAngle += angleDiff * dt * 25.0f; 

        float facingDir   = facingLeft ? -1.0f : 1.0f;
        float swingOffset = swingVal * 5.0f * facingDir; 
        float swingHeight = -std::pow(swingVal, 2) * 3.0f;
        float frontBaseX  = facingLeft ? -4.0f : 4.0f;

        if (isAirborne) {
            handA.offset.x = lerp(handA.offset.x, frontBaseX, dt * 10.0f);
            handA.offset.y = lerp(handA.offset.y, -6.0f, dt * 10.0f);
        } else {
            handA.offset.x = lerp(handA.offset.x, frontBaseX + swingOffset, dt * 20.0f);
            handA.offset.y = lerp(handA.offset.y, swingHeight, dt * 20.0f);
        }

    } else {
        float facingDir   = facingLeft ? -1.0f : 1.0f;
        float swingOffset = swingVal * 5.0f * facingDir; 
        float swingHeight = -std::pow(swingVal, 2) * 3.0f;
        float frontBaseX  = facingLeft ? -4.0f :  4.0f;
        float backBaseX   = facingLeft ?  4.0f : -4.0f;

        if (isAirborne) {
            handA.offset.x = lerp(handA.offset.x, frontBaseX, dt * 10.0f);
            handA.offset.y = lerp(handA.offset.y, -6.0f, dt * 10.0f);
            handB.offset.x = lerp(handB.offset.x, backBaseX, dt * 10.0f);
            handB.offset.y = lerp(handB.offset.y, -6.0f, dt * 10.0f);
            
            float desiredWeaponAngle = facingLeft ? -90.0f : 90.0f; 
            
            float angleDiff = desiredWeaponAngle - weaponAngle;
            while (angleDiff > 180.0f) angleDiff -= 360.0f;
            while (angleDiff < -180.0f) angleDiff += 360.0f;
            weaponAngle += angleDiff * dt * 15.0f;
        } else {
            handA.offset.x = lerp(handA.offset.x, frontBaseX + swingOffset, dt * 20.0f);
            handA.offset.y = lerp(handA.offset.y, swingHeight, dt * 20.0f);
            handB.offset.x = lerp(handB.offset.x, backBaseX - swingOffset, dt * 20.0f);
            handB.offset.y = lerp(handB.offset.y, swingHeight, dt * 20.0f);

            float slopeB = 1.2f * swingVal;
            float angleDeg = std::atan(slopeB) * 180.0f / PI;

            float desiredWeaponAngle = facingLeft ? (-90.0f - angleDeg) : (90.0f + angleDeg);
            
            float angleDiff = desiredWeaponAngle - weaponAngle;
            while (angleDiff > 180.0f) angleDiff -= 360.0f;
            while (angleDiff < -180.0f) angleDiff += 360.0f;
            weaponAngle += angleDiff * dt * 20.0f;
        }
    }
}

void Entity::renderSprite(sf::RenderTarget& target, sf::Vector2f bodyPos, float bodyAng) {
    if (pCtrl.isSelected) {
        sf::RectangleShape outline(sf::Vector2f(colHalfW * 2.0f + 4.0f, colHalfH * 2.0f + 4.0f));
        outline.setOrigin(sf::Vector2f(colHalfW + 2.0f, colHalfH + 2.0f));
        outline.setPosition(bodyPos);
        outline.setFillColor(sf::Color::Transparent);
        outline.setOutlineColor(sf::Color::Green);
        outline.setOutlineThickness(1.0f);
        target.draw(outline);
    }

    if (!sprite.sprite.has_value()) return;

    float renderAngle = bodyAng * 180.0f / PI;
    
    // STRICTLY use bodyPos (no bob.offsetY applied to the main physics body)
    // This ensures the visual pixels never detach from the debug physics lines vertically.
    sf::Vector2f renderPos = bodyPos; 

    sf::VertexArray va(sf::PrimitiveType::Triangles);
    float rad = -renderAngle * PI / 180.0f;
    float r_cs = std::cos(rad);
    float r_sn = std::sin(rad);

    sf::IntRect texRect = sprite.sprite->getTextureRect();
    sf::Vector2f origin = sprite.sprite->getOrigin();
    
    // SFML uses negative width for flipped rects, so we extract the true top-left atlas coordinate
    int atlas_x = sprite.flipX ? (texRect.position.x - sprite.frameWidth) : texRect.position.x;
    int atlas_y = texRect.position.y;

    float radius = std::hypot(sprite.frameWidth, sprite.frameHeight) + 1.0f;
    int maxD = static_cast<int>(std::ceil(radius));

    // Handle fractional physics centers to prevent the 0.5 sub-pixel shifting
    float fractX = origin.x - std::floor(origin.x);
    float fractY = origin.y - std::floor(origin.y);

    for (int dy = -maxD; dy <= maxD; ++dy) {
        for (int dx = -maxD; dx <= maxD; ++dx) {
            
            // Screen position of the pixel
            sf::Vector2f pxPos(renderPos.x + dx - fractX, renderPos.y + dy - fractY);
            
            // Map the exact center of this screen pixel back to physical space
            sf::Vector2f worldCenter = pxPos + sf::Vector2f(0.5f, 0.5f);
            float dx_w = worldCenter.x - renderPos.x;
            float dy_w = worldCenter.y - renderPos.y;
            
            // Un-rotate the physical space to match the sprite's local rotation
            float rx = dx_w * r_cs - dy_w * r_sn;
            float ry = dx_w * r_sn + dy_w * r_cs;
            
            // Local continuous coordinates within the sprite frame
            float local_cx = rx + origin.x;
            float local_cy = ry + origin.y;
            
            // THE FIX: If the entity is flipped, mirror the visual texture exactly 
            // around the center of the Box2D collider (origin.x). 
            // This guarantees the visual bounds and physics bounds stay glued together!
            if (sprite.flipX) {
                local_cx = 2.0f * origin.x - local_cx;
            }

            int lx = static_cast<int>(std::floor(local_cx));
            int ly = static_cast<int>(std::floor(local_cy));

            // Only draw if we are inside the sprite's frame bounds
            if (lx >= 0 && lx < sprite.frameWidth && ly >= 0 && ly < sprite.frameHeight) {
                int tx = atlas_x + lx;
                int ty = atlas_y + ly;

                sf::Vector2f t_c(tx + 0.5f, ty + 0.5f);
                
                va.append(sf::Vertex{pxPos, sf::Color::White, t_c});
                va.append(sf::Vertex{pxPos + sf::Vector2f(1.f, 0.f), sf::Color::White, t_c});
                va.append(sf::Vertex{pxPos + sf::Vector2f(1.f, 1.f), sf::Color::White, t_c});
                va.append(sf::Vertex{pxPos, sf::Color::White, t_c});
                va.append(sf::Vertex{pxPos + sf::Vector2f(1.f, 1.f), sf::Color::White, t_c});
                va.append(sf::Vertex{pxPos + sf::Vector2f(0.f, 1.f), sf::Color::White, t_c});
            }
        }
    }
    
    sf::RenderStates states;
    states.texture = &sprite.sprite->getTexture();
    target.draw(va, states);
}

void Entity::renderArmsAndWeapon(sf::RenderTarget& target, sf::Vector2f bodyPos, float bodyAng) {
    sf::Vector2f vbp(bodyPos.x, bodyPos.y + bob.offsetY + armBaseY);
    system->drawPixelatedHand(target, vbp + handB.offset + handB.recoilPos, sf::Color::White);

    if (pCtrl.equippedWeapon) {
        float renderAngle = weaponAngle + handB.recoilAngle;
        sf::Vector2f renderHandPos = vbp + handB.offset + handB.recoilPos;
        pCtrl.equippedWeapon->renderPixelated(target, renderHandPos, renderAngle, sprite.flipX);
    }
}

b2BodyId Entity::createRagdollPart(float w, float h, sf::Vector2f worldPosPx, float angle, float density, bool isCircle) {
    b2BodyDef bdef = b2DefaultBodyDef();
    bdef.type = b2_dynamicBody;
    bdef.position = {worldPosPx.x * P2M, worldPosPx.y * P2M};
    bdef.rotation = b2MakeRot(angle);
    bdef.linearDamping = 0.5f;
    bdef.angularDamping = 2.0f;
    b2BodyId partId = b2CreateBody(physicsWorldId, &bdef);
    
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = density;
    shapeDef.filter.categoryBits = 0x0008;
    shapeDef.filter.maskBits = 0x0001 | 0x0002 | 0x0004;
    shapeDef.material.friction = 0.5f;
    
    if (isCircle) { b2Circle circle = {{0, 0}, w * P2M}; b2CreateCircleShape(partId, &shapeDef, &circle); } 
    else { b2Polygon box = b2MakeBox(w * P2M, h * P2M); b2CreatePolygonShape(partId, &shapeDef, &box); }
    return partId;
}

void Entity::createRevoluteJoint(b2BodyId bA, b2BodyId bB, b2Vec2 anchorWorldPx) {
    b2RevoluteJointDef jd = b2DefaultRevoluteJointDef();
    b2Vec2 lAA = b2Body_GetLocalPoint(bA, {anchorWorldPx.x * P2M, anchorWorldPx.y * P2M});
    b2Vec2 lAB = b2Body_GetLocalPoint(bB, {anchorWorldPx.x * P2M, anchorWorldPx.y * P2M});
    Box2DCompat::InitJointCompat(jd, bA, bB, lAA, lAB);
    jd.enableLimit = true; jd.lowerAngle = -PI/1.5f; jd.upperAngle = PI/1.5f;
    b2CreateRevoluteJoint(physicsWorldId, &jd);
}

void Entity::createDistanceJoint(b2BodyId bA, b2BodyId bB, b2Vec2 anchorWorldPx) {
    b2DistanceJointDef djd = b2DefaultDistanceJointDef();
    b2Vec2 lAA = b2Body_GetLocalPoint(bA, {anchorWorldPx.x * P2M, anchorWorldPx.y * P2M});
    b2Vec2 lAB = {0.0f, 0.0f};
    Box2DCompat::InitJointCompat(djd, bA, bB, lAA, lAB);
    djd.minLength = 0.0f; 
    djd.maxLength = 9.0f * P2M;
    
    b2Vec2 pA = b2Body_GetWorldPoint(bA, lAA);
    b2Vec2 pB = b2Body_GetWorldPoint(bB, lAB);
    float dist = std::hypot(pA.x - pB.x, pA.y - pB.y);
    
    djd.length = std::max(0.01f, dist);
    b2CreateDistanceJoint(physicsWorldId, &djd);
}

void Entity::toggleRagdoll(bool enable) {
    if (pCtrl.isRagdoll == enable) return; 
    pCtrl.isRagdoll = enable;
    
    if (enable) {
        b2Body_SetAngularDamping(bodyId, 2.0f);
        int shapeCount = b2Body_GetShapeCount(bodyId);
        if (shapeCount > 0) {
            std::vector<b2ShapeId> shapes(shapeCount);
            b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
            for (int i = 0; i < shapeCount; i++) b2DestroyShape(shapes[i], true);
        }
        
        createMainCollider(1.0f, 0.1f);
        
        b2Vec2 p = b2Body_GetPosition(bodyId);
        sf::Vector2f bodyPos(p.x * M2P, p.y * M2P);
        sf::Vector2f vbp = {bodyPos.x, bodyPos.y + bob.offsetY + armBaseY};

        sf::Vector2f handA_pos = vbp + handA.offset;
        handA.ragdollBodyId = createRagdollPart(2.0f, 2.0f, handA_pos, 0.0f, 0.5f, true);
        sf::Vector2f shoulderA = bodyPos + sf::Vector2f(-4.0f, armBaseY - 4.0f);
        createDistanceJoint(bodyId, handA.ragdollBodyId, {shoulderA.x, shoulderA.y});

        sf::Vector2f handB_pos = vbp + handB.offset;
        handB.ragdollBodyId = createRagdollPart(2.0f, 2.0f, handB_pos, 0.0f, 0.5f, true);
        sf::Vector2f shoulderB = bodyPos + sf::Vector2f(4.0f, armBaseY - 4.0f);
        createDistanceJoint(bodyId, handB.ragdollBodyId, {shoulderB.x, shoulderB.y});
    } else {
        b2Body_SetAngularDamping(bodyId, 10.0f + (5.0f * uprightMult));
        b2Body_SetTransform(bodyId, b2Body_GetPosition(bodyId), b2MakeRot(0.0f));
        
        int shapeCount = b2Body_GetShapeCount(bodyId);
        if (shapeCount > 0) {
            std::vector<b2ShapeId> shapes(shapeCount);
            b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
            for (int i = 0; i < shapeCount; i++) b2DestroyShape(shapes[i], true);
        }
        
        createMainCollider(10.0f, 0.0f);

        if (b2Body_IsValid(handA.ragdollBodyId)) b2DestroyBody(handA.ragdollBodyId);
        if (b2Body_IsValid(handB.ragdollBodyId)) b2DestroyBody(handB.ragdollBodyId);
        
        handA.ragdollBodyId = b2_nullBodyId; handB.ragdollBodyId = b2_nullBodyId;
        bob.offsetY = 0.0f; bob.velocity = 0.0f;
    }
}
void Entity::save(std::ostream& out) const {
    b2Vec2 linVel = b2Body_GetLinearVelocity(bodyId);
    b2Rot rot = b2Body_GetRotation(bodyId);
    float angle = std::atan2(rot.s, rot.c);
    float angVel = b2Body_GetAngularVelocity(bodyId);
    
    out.write((const char*)&linVel, sizeof(b2Vec2));
    out.write((const char*)&angle, sizeof(float));
    out.write((const char*)&angVel, sizeof(float));
    
    out.write((const char*)&sprite.flipX, sizeof(bool));
    out.write((const char*)&sprite.currentFrameIndex, sizeof(int));
    out.write((const char*)&sprite.frameTimer, sizeof(float));
    size_t cLen = sprite.currentState.size();
    out.write((const char*)&cLen, sizeof(size_t));
    if (cLen > 0) out.write(sprite.currentState.c_str(), cLen);

    out.write((const char*)&pCtrl.isPlayer, sizeof(bool));
    out.write((const char*)&pCtrl.hasTarget, sizeof(bool));
    out.write((const char*)&pCtrl.targetPos, sizeof(sf::Vector2f));
    
    size_t pathSz = pCtrl.path.size();
    out.write((const char*)&pathSz, sizeof(size_t));
    if (pathSz > 0) out.write((const char*)pCtrl.path.data(), pathSz * sizeof(PathNodeData));
    
    out.write((const char*)&pCtrl.pathIndex, sizeof(int));
    out.write((const char*)&pCtrl.pathRecalcTimer, sizeof(float));
    out.write((const char*)&pCtrl.stuckTimer, sizeof(float));
    out.write((const char*)&pCtrl.lastPos, sizeof(sf::Vector2f));
    out.write((const char*)&pCtrl.homePos, sizeof(sf::Vector2f));
    out.write((const char*)&pCtrl.homePosSet, sizeof(bool));
    out.write((const char*)&pCtrl.idleWaitTimer, sizeof(float));
    out.write((const char*)&pCtrl.isWandering, sizeof(bool));
    out.write((const char*)&pCtrl.wanderDir, sizeof(float));
    out.write((const char*)&pCtrl.wanderTimer, sizeof(float));
    out.write((const char*)&pCtrl.physicsStuckTimer, sizeof(float));
    out.write((const char*)&pCtrl.isGrounded, sizeof(bool));
    
    out.write((const char*)&uprightMult, sizeof(float));

    auto saveHand = [&](const ProceduralHand& hand) {
        out.write((const char*)&hand.offset, sizeof(sf::Vector2f));
        out.write((const char*)&hand.recoilPos, sizeof(sf::Vector2f));
        out.write((const char*)&hand.recoilVel, sizeof(sf::Vector2f));
        out.write((const char*)&hand.recoilAngle, sizeof(float));
        out.write((const char*)&hand.recoilAngularVel, sizeof(float));
    };

    saveHand(handA);
    saveHand(handB);
    out.write((const char*)&bob, sizeof(BodyBobState));
    out.write((const char*)&armSwing, sizeof(float));
    out.write((const char*)&weaponAngle, sizeof(float));
}

void Entity::load(std::istream& in) {
    b2Vec2 linVel; float angle, angVel;
    in.read((char*)&linVel, sizeof(b2Vec2));
    in.read((char*)&angle, sizeof(float));
    in.read((char*)&angVel, sizeof(float));

    b2Body_SetTransform(bodyId, b2Body_GetPosition(bodyId), b2MakeRot(angle));
    b2Body_SetLinearVelocity(bodyId, linVel);
    b2Body_SetAngularVelocity(bodyId, angVel);

    in.read((char*)&sprite.flipX, sizeof(bool));
    in.read((char*)&sprite.currentFrameIndex, sizeof(int));
    in.read((char*)&sprite.frameTimer, sizeof(float));
    size_t cLen = 0; in.read((char*)&cLen, sizeof(size_t));
    if (cLen > 0) { sprite.currentState.resize(cLen); in.read(&sprite.currentState[0], cLen); }

    in.read((char*)&pCtrl.isPlayer, sizeof(bool));
    in.read((char*)&pCtrl.hasTarget, sizeof(bool));
    in.read((char*)&pCtrl.targetPos, sizeof(sf::Vector2f));
    
    size_t pathSz = 0; in.read((char*)&pathSz, sizeof(size_t));
    if (pathSz > 0) {
        pCtrl.path.resize(pathSz);
        in.read((char*)pCtrl.path.data(), pathSz * sizeof(PathNodeData));
    }
    
    in.read((char*)&pCtrl.pathIndex, sizeof(int));
    in.read((char*)&pCtrl.pathRecalcTimer, sizeof(float));
    in.read((char*)&pCtrl.stuckTimer, sizeof(float));
    in.read((char*)&pCtrl.lastPos, sizeof(sf::Vector2f));
    in.read((char*)&pCtrl.homePos, sizeof(sf::Vector2f));
    in.read((char*)&pCtrl.homePosSet, sizeof(bool));
    in.read((char*)&pCtrl.idleWaitTimer, sizeof(float));
    in.read((char*)&pCtrl.isWandering, sizeof(bool));
    in.read((char*)&pCtrl.wanderDir, sizeof(float));
    in.read((char*)&pCtrl.wanderTimer, sizeof(float));
    in.read((char*)&pCtrl.physicsStuckTimer, sizeof(float));
    in.read((char*)&pCtrl.isGrounded, sizeof(bool));
    pCtrl.isRagdoll = false;
    
    in.read((char*)&uprightMult, sizeof(float));

    auto readHand = [&](ProceduralHand& hand) {
        in.read((char*)&hand.offset, sizeof(sf::Vector2f));
        in.read((char*)&hand.recoilPos, sizeof(sf::Vector2f));
        in.read((char*)&hand.recoilVel, sizeof(sf::Vector2f));
        in.read((char*)&hand.recoilAngle, sizeof(float));
        in.read((char*)&hand.recoilAngularVel, sizeof(float));
        hand.ragdollBodyId = b2_nullBodyId;
    };
    readHand(handA); readHand(handB);
    in.read((char*)&bob, sizeof(BodyBobState));
    in.read((char*)&armSwing, sizeof(float));
    in.read((char*)&weaponAngle, sizeof(float));
}