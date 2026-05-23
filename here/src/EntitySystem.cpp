#include "EntitySystem.hpp"
#include "Weapon.hpp"
#include "Constants.hpp"
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <queue>
#include <map>
#include <SFML/Window/Mouse.hpp>

constexpr float PI       = 3.14159265358979323846f;
constexpr float NO_GROUND = 1e9f;
constexpr float FOOT_CAST_UP   = 24.0f;
constexpr float FOOT_CAST_DOWN = 24.0f;
constexpr float TARGET_CAST_UP   = 40.0f;
constexpr float TARGET_CAST_DOWN = 80.0f;

inline float lerp(float a, float b, float t) { return a + t * (b - a); }
inline sf::Vector2f lerpV(sf::Vector2f a, sf::Vector2f b, float t) {
    return { lerp(a.x, b.x, t), lerp(a.y, b.y, t) };
}
inline float length(sf::Vector2f v) { return std::hypot(v.x, v.y); }
inline float clamp01(float t)       { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }

// --- NAVMESH DATA ---
namespace {
    struct EdgeData {
        int action; // 0 = walk, 1 = jump, 2 = fall
        float dir;
        std::vector<sf::Vector2f> trajectory;
        float requiredVx = 0.0f;
    };
    std::map<std::pair<int, int>, EdgeData> s_EdgeData;
}

EntitySystem::EntitySystem(b2WorldId physWorld) : physicsWorldId(physWorld) {
    sf::Image dummy;
    dummy.resize(sf::Vector2u(32, 32), sf::Color(100, 100, 100));
    defaultPlayerTexture = std::make_shared<sf::Texture>(dummy);
}
EntitySystem::~EntitySystem() { registry.clear(); }

entt::entity EntitySystem::spawnEntity(float x, float y, const std::string& texturePath, bool isPlayer) {
    auto entity = registry.create();

    b2BodyDef bdef      = b2DefaultBodyDef();
    bdef.type           = b2_dynamicBody;
    bdef.position.x     = x * P2M;
    bdef.position.y     = y * P2M;
    bdef.linearDamping  = 1.0f;
    bdef.angularDamping = 10.0f;

    b2BodyId bodyId = b2CreateBody(physicsWorldId, &bdef);
    
    // Smooth capsule shape so entities glide over pixel bumps instead of snagging
    b2Capsule capsule = {{-0.0f, -5.5f * P2M}, {0.0f, 5.5f * P2M}, 2.5f * P2M};
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density           = 10.0f; 
    shapeDef.material.friction = 0.1f;
    b2CreateCapsuleShape(bodyId, &shapeDef, &capsule);

    registry.emplace<PhysicsComponent>(entity, bodyId);
    auto& pCtrl = registry.emplace<PlayerControllerComponent>(entity);
    pCtrl.isPlayer = isPlayer;

    SpriteSheetComponent spriteComp;
    if (!texturePath.empty()) {
        try   { spriteComp.texture = std::make_shared<sf::Texture>(texturePath); }
        catch (...) { spriteComp.texture = defaultPlayerTexture; }
    } else {
        spriteComp.texture = defaultPlayerTexture;
    }
    spriteComp.sprite.emplace(*spriteComp.texture);
    spriteComp.frameWidth  = 32;
    spriteComp.frameHeight = 32;
    spriteComp.sprite->setOrigin({15.5f, 25.0f});
    
    spriteComp.animations["Idle"] = {0, 1, 0.1f};
    spriteComp.animations["Walk"] = {0, 1, 0.1f};
    spriteComp.animations["Jump"] = {0, 1, 0.1f};
    spriteComp.currentState = "Idle";
    registry.emplace<SpriteSheetComponent>(entity, spriteComp);

    ProceduralAnimationComponent anim;
    anim.legA.footWorld  = {x - 3.0f, y + 17.0f};
    anim.legB.footWorld  = {x + 3.0f, y + 17.0f};
    anim.legA.footStart  = anim.legA.footTarget = anim.legA.footWorld;
    anim.legB.footStart  = anim.legB.footTarget = anim.legB.footWorld;
    anim.legA.plantedX   = anim.legA.footWorld.x;
    anim.legA.plantedY   = anim.legA.footWorld.y;
    anim.legA.isPlanted  = true;
    anim.legB.plantedX   = anim.legB.footWorld.x;
    anim.legB.plantedY   = anim.legB.footWorld.y;
    anim.legB.isPlanted  = true;
    
    anim.handA.offset    = {4.0f, 0.0f};
    anim.handB.offset    = {-4.0f, 0.0f};
    anim.armSwing        = 0.0f;
    registry.emplace<ProceduralAnimationComponent>(entity, anim);

    return entity;
}

void EntitySystem::triggerSwing(sf::Vector2f targetWorldPos) {
    auto view = registry.view<PlayerControllerComponent>();
    for (auto [entity, player] : view.each()) {
        if (player.equippedWeapon && !player.isSwinging) {
            if (player.equippedWeapon->isGun) continue;
            player.isSwinging = true;
            player.swingTimer = 0.0f;
            player.swingEffectApplied = false;
            player.swingTarget = targetWorldPos;
            player.swingRandomness = ((rand() % 100) / 100.0f) * 0.4f - 0.2f;
        }
    }
}

void EntitySystem::updateInput(float dt, sf::Vector2f mouseWorldPos, RigidBodySystem& rbs, ParticleWorld& pw) {
    static bool rightClickLastGlobal = false;
    bool currentRightClickGlobal = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
    bool orderGiven = currentRightClickGlobal && !rightClickLastGlobal;
    rightClickLastGlobal = currentRightClickGlobal;

    if (orderGiven && !globalGraphBuilt) {
        buildGlobalNavGraph(pw);
    }

    debugLines.clear();
    auto view = registry.view<PlayerControllerComponent, PhysicsComponent, SpriteSheetComponent, ProceduralAnimationComponent>();
    
    view.each([&](auto, auto& player, auto& phys, auto& sprite, auto& anim) {
        b2Vec2 vel = b2Body_GetLinearVelocity(phys.bodyId);
        b2Vec2 pos = b2Body_GetPosition(phys.bodyId);
        sf::Vector2f bodyPos(pos.x * M2P, pos.y * M2P);

        float dir = 0.0f;
        bool wPressed = false;
        bool rightClick = false;
        bool leftClick = false;
        bool fPressed = false;
        bool ePressed = false;

        if (player.isPlayer) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) dir = -1.0f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) dir =  1.0f;
            wPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
            rightClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
            leftClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
            fPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F);
            ePressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E);
        } else {
            // --- AI NAVIGATION EXECUTION ---
            if (orderGiven) {
                player.hasTarget = true;
                player.targetPos = resolveTargetPos(mouseWorldPos, pw);
                
                buildGlobalNavGraph(pw); // Map update on click
                
                player.path = findPath(bodyPos, player.targetPos);
                if (!player.path.empty()) {
                    player.path.push_back({player.targetPos, false, false, false, 0.0f}); // Exact target pixel injection
                }
                player.pathIndex = 0;
                player.pathRecalcTimer = 0.5f;
            }
            
            if (player.hasTarget) {
                sf::Vector2f tPos = player.targetPos;
                debugLines.push_back({tPos + sf::Vector2f(-4, 0), tPos + sf::Vector2f(4, 0), sf::Color::Cyan});
                debugLines.push_back({tPos + sf::Vector2f(0, -4), tPos + sf::Vector2f(0, 4), sf::Color::Cyan});

                sf::Vector2f footPos(bodyPos.x, bodyPos.y + 17.0f);

                // Auto Recalculation Checkers
                player.pathRecalcTimer -= dt;
                
                if (std::hypot(bodyPos.x - player.lastPos.x, bodyPos.y - player.lastPos.y) < 2.0f) {
                    player.stuckTimer += dt;
                } else {
                    player.stuckTimer = 0.0f;
                    player.lastPos = bodyPos;
                }

                bool needsRecalc = false;
                if (player.stuckTimer > 0.5f && player.isGrounded) {
                    needsRecalc = true;
                    player.stuckTimer = 0.0f;
                }

                if (!player.path.empty() && player.pathIndex < player.path.size()) {
                    float dy = footPos.y - player.path[player.pathIndex].pos.y;
                    if (dy > 40.0f && player.isGrounded) needsRecalc = true; // Fell down detected
                }

                // Dynamic Recalculation Execution
                if ((needsRecalc || player.pathRecalcTimer <= 0.0f) && player.isGrounded) {
                    player.pathRecalcTimer = 0.5f;
                    if (std::hypot(player.targetPos.x - bodyPos.x, player.targetPos.y - bodyPos.y) > 24.0f) {
                        player.path = findPath(bodyPos, player.targetPos);
                        if (!player.path.empty()) {
                            player.path.push_back({player.targetPos, false, false, false, 0.0f});
                        }
                        player.pathIndex = 0;
                    } else {
                        player.pathIndex = player.path.size(); // Safely Reached Target
                    }
                }

                if (!player.path.empty() && player.pathIndex < player.path.size()) {
                    
                    b2Body_SetAwake(phys.bodyId, true);

                    // Consume nodes we have reached using foot level evaluation!
                    while (player.pathIndex < player.path.size()) {
                        PathNodeData nextNode = player.path[player.pathIndex];
                        float dist = std::hypot(nextNode.pos.x - footPos.x, nextNode.pos.y - footPos.y);
                        
                        bool reached = false;
                        if (nextNode.isJumpTakeoff) {
                            // Takeoffs strictly require precision arrival
                            if (std::abs(nextNode.pos.x - footPos.x) < 4.0f && std::abs(nextNode.pos.y - footPos.y) < 16.0f) {
                                reached = true;
                            }
                        } else {
                            // General Walking Pass-Through
                            if (dist < 12.0f) {
                                reached = true;
                            } else if (player.isGrounded && std::abs(nextNode.pos.x - footPos.x) < 12.0f && std::abs(nextNode.pos.y - footPos.y) < 24.0f) {
                                reached = true;
                            }
                        }
                        
                        if (reached) player.pathIndex++;
                        else break;
                    }

                    if (player.pathIndex < player.path.size()) {
                        PathNodeData nextNode = player.path[player.pathIndex];
                        
                        // Draw Debug Path
                        if (player.pathIndex > 0) debugLines.push_back({footPos, nextNode.pos, sf::Color::Yellow});
                        for (size_t i = player.pathIndex; i + 1 < player.path.size(); ++i) {
                            debugLines.push_back({player.path[i].pos, player.path[i+1].pos, sf::Color::Yellow});
                        }
                        
                        float dx = nextNode.pos.x - footPos.x; 
                        if (dx > 2.0f) {
                            dir = 1.0f;
                        } else if (dx < -2.0f) {
                            dir = -1.0f;
                        }
                        
                        bool needsJump = false;
                        
                        if (nextNode.isJump || nextNode.isFall) {
                            if (player.isGrounded) {
                                if (nextNode.isJump) needsJump = true;
                                // If it's Fall, don't trigger wPressed. Will naturally walk-off ledge.
                            }
                        } else {
                            // Lookahead for walk nodes that are slightly higher (stairs or bumps)
                            int lookMax = std::min(static_cast<int>(player.path.size()), player.pathIndex + 2);
                            for (int i = player.pathIndex; i < lookMax; ++i) {
                                if (player.path[i].pos.y < footPos.y - 12.0f && std::abs(player.path[i].pos.x - footPos.x) < 32.0f && !player.path[i].isJump && !player.path[i].isFall) {
                                    needsJump = true;
                                    break;
                                }
                            }
                            // Bump Recovery
                            if (std::abs(dx) > 4.0f && std::abs(vel.x) < 0.5f && player.stuckTimer > 0.1f) {
                                needsJump = true;
                            }
                        }
                        
                        if (player.isGrounded && needsJump) {
                            wPressed = true;
                        }
                    } else {
                        dir = 0.0f;
                        player.hasTarget = false;
                    }
                } else {
                    dir = 0.0f;
                    player.hasTarget = false;
                }
            }
        }

        // --- RAGDOLL TOGGLE ---
        b2Rot currentRot = b2Body_GetRotation(phys.bodyId);
        float currentBodyAng = std::atan2(currentRot.s, currentRot.c);
        float maxAngle = 45.0f * PI / 180.0f;
        bool forceRagdoll = (!player.isRagdoll && std::abs(currentBodyAng) > maxAngle);

        if ((fPressed && !player.fPressedLastFrame) || forceRagdoll) {
            if (forceRagdoll) {
                player.isRagdoll = true;
            } else {
                player.isRagdoll = !player.isRagdoll;
            }

            if (player.isRagdoll) {
                b2Body_SetAngularDamping(phys.bodyId, 2.0f);
                int shapeCount = b2Body_GetShapeCount(phys.bodyId);
                if (shapeCount > 0) {
                    std::vector<b2ShapeId> shapes(shapeCount);
                    b2Body_GetShapes(phys.bodyId, shapes.data(), shapeCount);
                    for (int i = 0; i < shapeCount; i++) b2DestroyShape(shapes[i], true);
                }
                b2Polygon box = b2MakeBox(2.5f * P2M, 8.0f * P2M); 
                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.density = 1.0f;
                shapeDef.material.friction = 0.1f;
                b2CreatePolygonShape(phys.bodyId, &shapeDef, &box);
                
                auto createRagdollPart = [&](float w, float h, sf::Vector2f worldPosPx, float angle, float density, uint32_t category, uint32_t mask, bool isCircle = false) -> b2BodyId {
                    b2BodyDef bdef = b2DefaultBodyDef();
                    bdef.type = b2_dynamicBody;
                    bdef.position = {worldPosPx.x * P2M, worldPosPx.y * P2M};
                    bdef.rotation = b2MakeRot(angle);
                    bdef.linearDamping = 0.5f;
                    bdef.angularDamping = 2.0f;
                    b2BodyId partId = b2CreateBody(physicsWorldId, &bdef);
                    b2ShapeDef shapeDef = b2DefaultShapeDef();
                    shapeDef.density = density;
                    shapeDef.filter.categoryBits = category;
                    shapeDef.filter.maskBits = mask;
                    shapeDef.material.friction = 0.5f;
                    if (isCircle) { b2Circle circle = {{0, 0}, w * P2M}; b2CreateCircleShape(partId, &shapeDef, &circle); } 
                    else { b2Polygon box = b2MakeBox(w * P2M, h * P2M); b2CreatePolygonShape(partId, &shapeDef, &box); }
                    return partId;
                };

                auto createRevoluteJoint = [&](b2BodyId bA, b2BodyId bB, b2Vec2 anchorWorldPx) {
                    b2RevoluteJointDef jd = b2DefaultRevoluteJointDef();
                    jd.bodyIdA = bA; jd.bodyIdB = bB;
                    jd.localAnchorA = b2Body_GetLocalPoint(bA, {anchorWorldPx.x * P2M, anchorWorldPx.y * P2M});
                    jd.localAnchorB = b2Body_GetLocalPoint(bB, {anchorWorldPx.x * P2M, anchorWorldPx.y * P2M});
                    jd.enableLimit = true; jd.lowerAngle = -PI/1.5f; jd.upperAngle = PI/1.5f;
                    jd.collideConnected = false; 
                    b2CreateRevoluteJoint(physicsWorldId, &jd);
                };
                
                auto createDistanceJoint = [&](b2BodyId bA, b2BodyId bB, b2Vec2 anchorWorldPx) {
                    b2DistanceJointDef djd = b2DefaultDistanceJointDef();
                    djd.bodyIdA = bA; djd.bodyIdB = bB;
                    djd.localAnchorA = b2Body_GetLocalPoint(bA, {anchorWorldPx.x * P2M, anchorWorldPx.y * P2M});
                    djd.localAnchorB = b2Vec2_zero; djd.minLength = 0.0f; djd.maxLength = 9.0f * P2M;
                    djd.collideConnected = false; b2CreateDistanceJoint(physicsWorldId, &djd);
                };

                sf::Vector2f vbp = {bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY};
                
                auto initLeg = [&](ProceduralLeg& leg, float density) {
                    sf::Vector2f hip = vbp + leg.hipOffset;
                    sf::Vector2f foot = leg.footWorld;
                    sf::Vector2f dir = foot - hip;
                    float angle = std::atan2(dir.y, dir.x) - PI/2.0f;
                    sf::Vector2f center = hip + dir * 0.5f;
                    leg.ragdollBodyId = createRagdollPart(1.5f, 4.5f, center, angle, density, 1, 1);
                    createRevoluteJoint(phys.bodyId, leg.ragdollBodyId, {hip.x, hip.y});
                };
                initLeg(anim.legA, 1.0f);
                initLeg(anim.legB, 1.0f);

                sf::Vector2f handA_pos = vbp + anim.handA.offset;
                anim.handA.ragdollBodyId = createRagdollPart(2.0f, 2.0f, handA_pos, 0.0f, 0.5f, 2, 1, true);
                sf::Vector2f shoulderA = bodyPos + sf::Vector2f(-4.0f, 4.0f);
                createDistanceJoint(phys.bodyId, anim.handA.ragdollBodyId, {shoulderA.x, shoulderA.y});

                sf::Vector2f handB_pos = vbp + anim.handB.offset;
                anim.handB.ragdollBodyId = createRagdollPart(2.0f, 2.0f, handB_pos, 0.0f, 0.5f, 2, 1, true);
                sf::Vector2f shoulderB = bodyPos + sf::Vector2f(4.0f, 4.0f);
                createDistanceJoint(phys.bodyId, anim.handB.ragdollBodyId, {shoulderB.x, shoulderB.y});
                
            } else {
                b2Body_SetAngularDamping(phys.bodyId, 10.0f);
                b2Body_SetTransform(phys.bodyId, b2Body_GetPosition(phys.bodyId), b2MakeRot(0.0f));
                
                int shapeCount = b2Body_GetShapeCount(phys.bodyId);
                if (shapeCount > 0) {
                    std::vector<b2ShapeId> shapes(shapeCount);
                    b2Body_GetShapes(phys.bodyId, shapes.data(), shapeCount);
                    for (int i = 0; i < shapeCount; i++) b2DestroyShape(shapes[i], true);
                }
                b2Capsule capsule = {{-0.0f, -5.5f * P2M}, {0.0f, 5.5f * P2M}, 2.5f * P2M};
                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.density = 10.0f;
                shapeDef.material.friction = 0.1f;
                b2CreateCapsuleShape(phys.bodyId, &shapeDef, &capsule);

                if (b2Body_IsValid(anim.legA.ragdollBodyId)) b2DestroyBody(anim.legA.ragdollBodyId);
                if (b2Body_IsValid(anim.legB.ragdollBodyId)) b2DestroyBody(anim.legB.ragdollBodyId);
                if (b2Body_IsValid(anim.handA.ragdollBodyId)) b2DestroyBody(anim.handA.ragdollBodyId);
                if (b2Body_IsValid(anim.handB.ragdollBodyId)) b2DestroyBody(anim.handB.ragdollBodyId);
                
                anim.legA.ragdollBodyId = b2_nullBodyId; anim.legB.ragdollBodyId = b2_nullBodyId;
                anim.handA.ragdollBodyId = b2_nullBodyId; anim.handB.ragdollBodyId = b2_nullBodyId;
                anim.bob.offsetY = 0.0f; anim.bob.velocity = 0.0f;
            }
        }
        player.fPressedLastFrame = fPressed;

        if (player.isRagdoll) {
            player.isAiming = false; player.isSwinging = false;
            player.leftClickPressedLastFrame = leftClick; player.wPressedLastFrame = wPressed;
            player.ePressedLastFrame = ePressed;
            return; 
        }

        // --- UPRIGHT SPRING (PD Controller) ---
        b2Rot rot = b2Body_GetRotation(phys.bodyId);
        float bodyAng = std::atan2(rot.s, rot.c);
        float angVel = b2Body_GetAngularVelocity(phys.bodyId);
        if (std::abs(bodyAng) < 0.03f && std::abs(angVel) < 0.5f) {
            if (bodyAng != 0.0f || angVel != 0.0f) {
                b2Body_SetTransform(phys.bodyId, b2Body_GetPosition(phys.bodyId), b2MakeRot(0.0f));
                b2Body_SetAngularVelocity(phys.bodyId, 0.0f);
                bodyAng = 0.0f; angVel = 0.0f;
            }
        }
        bool movingAway = (bodyAng * angVel > 0.0f);
        float stiffness = movingAway ? 15.0f : 250.0f;
        float damping = movingAway ? 2.0f : 31.0f;
        
        if (std::abs(bodyAng) > maxAngle * 0.7f) {
            float excess = (std::abs(bodyAng) - maxAngle * 0.7f) / (maxAngle * 0.3f);
            stiffness += excess * 600.0f; damping += excess * 40.0f;
        }

        float torque = (-bodyAng * stiffness - angVel * damping) * b2Body_GetMass(phys.bodyId);
        b2Body_ApplyTorque(phys.bodyId, torque, true);

        // --- AIMING & SHOOTING (Player Only) ---
        if (rightClick && player.equippedWeapon && player.isPlayer) {
            player.isAiming = true;
            player.aimTarget = mouseWorldPos;
            debugLines.push_back({{bodyPos.x, bodyPos.y}, player.aimTarget, sf::Color::Magenta});
            
            if (player.equippedWeapon->isGun) {
                Weapon* w = static_cast<Weapon*>(player.equippedWeapon);
                if (player.fireTimer <= 0.0f) {
                    bool canShoot = w->semiAuto ? (leftClick && !player.leftClickPressedLastFrame) : leftClick;
                    if (canShoot) {
                        sf::Vector2f vbp(bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY);
                        sf::Vector2f handPos = vbp + anim.handB.offset + anim.handB.recoilPos;
                        float currentWeaponAngle = anim.weaponAngle + anim.handB.recoilAngle;
                        
                        w->fire(handPos, player.aimTarget, currentWeaponAngle, sprite.flipX, rbs, pw);
                        player.fireTimer = w->fireRate;

                        float wFinalAngle = currentWeaponAngle + (sprite.flipX ? -w->visualAngleOffset : w->visualAngleOffset);
                        float wRad = wFinalAngle * PI / 180.0f;
                        float dX = std::cos(wRad) * (sprite.flipX ? -1.0f : 1.0f);
                        float dY = std::sin(wRad) * (sprite.flipX ? -1.0f : 1.0f);
                        sf::Vector2f shootDir(dX, dY);
                        
                        float backwardForce = w->recoilForce * 120.0f;
                        anim.handB.recoilVel -= shootDir * backwardForce;
                        
                        sf::Vector2f perpDir(-shootDir.y, shootDir.x);
                        float randLateral = ((std::rand() % 100) / 100.0f - 0.5f) * backwardForce * 1.5f;
                        anim.handB.recoilVel += perpDir * randLateral;
                        anim.handB.recoilAngularVel += ((std::rand() % 100) / 100.0f - 0.5f) * w->visualRecoilAngle * 80.0f;

                        float playerKickMult = 0.5f; float effectiveMass = 0.8f; 
                        vel.x += (-shootDir.x * w->recoilForce * playerKickMult) / effectiveMass;
                        vel.y += (-shootDir.y * w->recoilForce * playerKickMult) / effectiveMass;
                    }
                }
            }
        } else {
            player.isAiming = false;
        }
        player.leftClickPressedLastFrame = leftClick;
        if (player.fireTimer > 0.0f) player.fireTimer -= dt;
        
        // --- WALKING AND TERRAIN LOOKAHEAD ---
        float speedFactor = 1.0f;
        
        // ONLY APPLY STRICT PROCEDURAL STOPPING TO THE PLAYER
        if (player.isPlayer && player.isGrounded && dir != 0.0f) {
            float maxDx = 0.0f;
            int maxLook = static_cast<int>(std::ceil(anim.stepLookahead));
            float baseCastFrom = bodyPos.y + 8;
            float minFootY = bodyPos.y + 12.0f; 
            float maxFootY = bodyPos.y + 22.0f; 

            for (int i = 1; i <= maxLook; ++i) {
                float testX = bodyPos.x + dir * i;
                float gY = groundCastY(testX, baseCastFrom, TARGET_CAST_UP + TARGET_CAST_DOWN, pw);
                if (gY < minFootY || gY > maxFootY) break;
                maxDx = static_cast<float>(i);
            }
            
            speedFactor = maxDx / anim.stepLookahead;
            if (speedFactor > 1.0f) speedFactor = 1.0f;

            b2QueryFilter filter = b2DefaultQueryFilter();
            b2Transform xf = b2Body_GetTransform(phys.bodyId);
            float localX = dir * 3.5f * P2M;
            b2Vec2 localTop = {localX, -8.0f * P2M};
            b2Vec2 localBottom = {localX, 8.0f * P2M};
            b2Vec2 worldTop = b2TransformPoint(xf, localTop);
            b2Vec2 worldBottom = b2TransformPoint(xf, localBottom);
            b2Vec2 sideTrans = {worldBottom.x - worldTop.x, worldBottom.y - worldTop.y};
            
            b2RayResult sideHit = b2World_CastRayClosest(physicsWorldId, worldTop, sideTrans, filter);
            if (sideHit.hit) {
                b2BodyId sideBody = b2Shape_GetBody(sideHit.shapeId);
                if (sideBody.index1 != phys.bodyId.index1) { 
                    auto checkLeg = [&](const ProceduralLeg& leg) -> bool {
                        b2Vec2 origin = {(leg.footWorld.x - 0.5f) * P2M, (leg.footWorld.y + 0.5f) * P2M};
                        b2Vec2 trans = {1.0f * P2M, 0.0f}; 
                        b2RayResult hit = b2World_CastRayClosest(physicsWorldId, origin, trans, filter);
                        if (hit.hit) {
                            b2BodyId hitBody = b2Shape_GetBody(hit.shapeId);
                            return (hitBody.index1 == sideBody.index1 && hitBody.generation == sideBody.generation);
                        }
                        return false;
                    };
                    if (checkLeg(anim.legA) || checkLeg(anim.legB)) {
                        speedFactor = 0.0f;
                        vel.x = 0.0f; 
                    }
                }
            }
        }

        float desiredVelX = dir * player.moveSpeed * speedFactor;
        
        if (!player.isSwinging && !player.isAiming) {
            if (dir < 0.0f) sprite.flipX = true;
            else if (dir > 0.0f) sprite.flipX = false;
        }

        if (player.landingTimer > 0.0f) {
            player.landingTimer -= dt;
            desiredVelX *= 0.1f; 
        }

        if (player.isGrounded) vel.x = lerp(vel.x, desiredVelX, dt * 8.0f);

        if (wPressed && !player.wPressedLastFrame && player.isGrounded && player.landingTimer <= 0.0f) {
            vel.y = player.jumpForce;
            
            // AI Explicit Target Velocity Injection
            if (!player.isPlayer && !player.path.empty() && player.pathIndex < player.path.size()) {
                PathNodeData nextNode = player.path[player.pathIndex];
                if (nextNode.isJump && std::abs(nextNode.requiredVx) > 0.1f) {
                    vel.x = nextNode.requiredVx * P2M; // Perfectly sets exact simulated inertia!
                }
            }
        }
        player.wPressedLastFrame = wPressed;

        if      (!player.isGrounded)         sprite.currentState = "Jump";
        else if (player.landingTimer > 0.0f) sprite.currentState = "Idle";
        else if (std::abs(vel.x) > 1.0f)     sprite.currentState = "Walk";
        else                                 sprite.currentState = "Idle";

        b2Body_SetLinearVelocity(phys.bodyId, vel);

        if (ePressed && player.isPlayer) {
            if (!player.ePressedLastFrame) {
                player.ePressedLastFrame = true;
                if (player.equippedWeapon) {
                    player.equippedWeapon->isEquipped = false;
                    b2Body_Enable(player.equippedWeapon->bodyId);
                    
                    b2Vec2 dropPos = { (bodyPos.x + (sprite.flipX ? -15.f : 15.f)) * P2M, (bodyPos.y - 10.f) * P2M };
                    b2Body_SetTransform(player.equippedWeapon->bodyId, dropPos, b2MakeRot(0.0f));
                    b2Body_SetLinearVelocity(player.equippedWeapon->bodyId, { sprite.flipX ? -8.f : 8.f, -5.0f });
                    b2Body_SetAngularVelocity(player.equippedWeapon->bodyId, sprite.flipX ? -5.0f : 5.0f);
                    
                    player.equippedWeapon = nullptr;
                } else {
                    RigidBody* nearest = rbs.getNearestWeapon(bodyPos, 40.0f);
                    if (nearest) {
                        nearest->clearFromWorld(pw);
                        nearest->isEquipped = true;
                        b2Body_Disable(nearest->bodyId); 
                        player.equippedWeapon = nearest;
                    }
                }
            }
        } else {
            player.ePressedLastFrame = false;
        }
    });
}

float EntitySystem::groundCastY(float worldX, float castFromY, float maxDown, ParticleWorld& pw) {
    int px     = static_cast<int>(std::round(worldX));
    int startY = static_cast<int>(std::floor(castFromY));

    for (int i = 0; i <= static_cast<int>(maxDown); ++i) {
        int py = startY + i;
        if (!pw.isEmpty(px, py)) {
            BaseComponent* base = pw.get<BaseComponent>(px, py);
            if (base && base->compMask != 0) {
                Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                if (logic) {
                    MaterialGroup group = logic->getGroup();
                    if (group != MaterialGroup::Liquid && group != MaterialGroup::Gas) {
                        debugLines.push_back({{worldX, castFromY}, {worldX, static_cast<float>(py - 1)}, sf::Color::Green});
                        return static_cast<float>(py - 1);
                    }
                }
            }
        }
    }
    debugLines.push_back({{worldX, castFromY}, {worldX, castFromY + maxDown}, sf::Color::Red});
    return NO_GROUND;
}

void EntitySystem::updateProceduralAnimations(float dt, ParticleWorld& pw) {
    auto view = registry.view<PlayerControllerComponent, PhysicsComponent, ProceduralAnimationComponent, SpriteSheetComponent>();

    view.each([&](auto entity, auto& player, auto& phys, auto& anim, auto& spriteComp) {

        b2Vec2 b2Pos = b2Body_GetPosition(phys.bodyId);
        sf::Vector2f bodyPos(b2Pos.x * M2P, b2Pos.y * M2P);
        b2Vec2 b2Vel = b2Body_GetLinearVelocity(phys.bodyId);

        b2Rot rot = b2Body_GetRotation(phys.bodyId);
        float bodyAng = std::atan2(rot.s, rot.c);

        if (player.isRagdoll) {
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
                    b2Vec2 vel = b2Body_GetLinearVelocity(bId);
                    vel.x *= 0.8f; vel.y *= 0.8f; 
                    b2Body_ApplyForceToCenter(bId, {0.0f, -(sandCount * 25.0f) * mass}, true);
                    b2Body_SetLinearVelocity(bId, vel);
                }
            };
            
            displaceSandAndApplyForce(phys.bodyId, {2.5f, 8.0f}, bodyPos, bodyAng);
            
            auto applySandToPart = [&](b2BodyId bId, sf::Vector2f halfSize) {
                if (b2Body_IsValid(bId)) {
                    b2Vec2 p = b2Body_GetPosition(bId);
                    b2Rot r = b2Body_GetRotation(bId);
                    displaceSandAndApplyForce(bId, halfSize, {p.x * M2P, p.y * M2P}, std::atan2(r.s, r.c));
                }
            };
            
            applySandToPart(anim.legA.ragdollBodyId, {1.5f, 4.5f});
            applySandToPart(anim.legB.ragdollBodyId, {1.5f, 4.5f});
            applySandToPart(anim.handA.ragdollBodyId, {2.0f, 2.0f});
            applySandToPart(anim.handB.ragdollBodyId, {2.0f, 2.0f});
            
            sf::Vector2f vbp(bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY);
            
            auto syncHand = [&](ProceduralHand& hand) {
                if (b2Body_IsValid(hand.ragdollBodyId)) {
                    b2Vec2 p = b2Body_GetPosition(hand.ragdollBodyId);
                    hand.offset = {p.x * M2P - vbp.x, p.y * M2P - vbp.y};
                }
            };
            syncHand(anim.handA);
            syncHand(anim.handB);
            
            auto syncLeg = [&](ProceduralLeg& leg, b2BodyId bId) {
                if (b2Body_IsValid(bId)) {
                    b2Vec2 p = b2Body_GetPosition(bId);
                    b2Rot r = b2Body_GetRotation(bId);
                    float a = std::atan2(r.s, r.c);
                    
                    sf::Vector2f center(p.x * M2P, p.y * M2P);
                    float dirX = -std::sin(a);
                    float dirY = std::cos(a);
                    
                    leg.footWorld = {center.x + dirX * 4.5f, center.y + dirY * 4.5f};
                    
                    sf::Vector2f hip = {center.x - dirX * 4.5f, center.y - dirY * 4.5f};
                    leg.hipOffset = {hip.x - vbp.x, hip.y - vbp.y};
                }
            };
            syncLeg(anim.legA, anim.legA.ragdollBodyId);
            syncLeg(anim.legB, anim.legB.ragdollBodyId);

            if (b2Body_IsValid(anim.handB.ragdollBodyId)) {
                b2Rot handRot = b2Body_GetRotation(anim.handB.ragdollBodyId);
                anim.weaponAngle = std::atan2(handRot.s, handRot.c) * 180.0f / PI + (spriteComp.flipX ? -90.0f : 90.0f);
            } else {
                anim.weaponAngle = bodyAng * 180.0f / PI + (spriteComp.flipX ? -90.0f : 90.0f);
            }
            
            anim.handB.recoilPos = {0.0f, 0.0f};
            anim.handB.recoilVel = {0.0f, 0.0f};
            
            return; 
        }

        {
            float f = -anim.bob.stiffness * anim.bob.offsetY - anim.bob.damping * anim.bob.velocity;
            anim.bob.velocity += f * dt;
            anim.bob.offsetY  += anim.bob.velocity * dt;
        }

        sf::Vector2f vbp(bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY);
        float hipY = vbp.y;
        float minAllowedFootY = vbp.y + 4.0f; 

        auto castForTarget = [&](float worldX) -> float {
            return groundCastY(worldX, hipY, 24.0f, pw);
        };
        
        auto getSafeTarget = [&](float lookOffset) -> sf::Vector2f {
            float castDir = lookOffset > 0 ? 1.0f : (lookOffset < 0 ? -1.0f : 0.0f);
            float maxDist = std::abs(lookOffset);
            float bestX = bodyPos.x;
            
            float bestY = groundCastY(bodyPos.x, hipY, 24.0f, pw);
            if (bestY >= NO_GROUND) bestY = bodyPos.y + 17.0f;

            if (maxDist < 0.1f) return {bestX, std::max(bestY, minAllowedFootY)};

            float minFootY = bodyPos.y + 12.0f;
            float maxFootY = bodyPos.y + 22.0f; 

            for (int i = 1; i <= static_cast<int>(std::ceil(maxDist)); ++i) {
                float testX = bodyPos.x + castDir * i;
                float ty = groundCastY(testX, hipY, 24.0f, pw);
                
                if (ty < minFootY) break;
                if (ty > maxFootY) break;
                
                bestX = testX;
                bestY = ty;
            }
            return {bestX, std::max(bestY, minAllowedFootY)};
        };

        float centerGroundY = castForTarget(bodyPos.x);
        float distToGround = (centerGroundY >= NO_GROUND) ? 1000.0f : (centerGroundY - bodyPos.y);

        bool wasGrounded = player.isGrounded;
        float airThreshold = (!wasGrounded && b2Vel.y > 0.0f) ? 18.0f : 28.0f;
        
        bool isAirborne  = (b2Vel.y < -15.0f) || (distToGround > airThreshold);

        if (isAirborne && b2Vel.y > 0.0f) {
            player.lastFallVelocity = b2Vel.y;
        }

        player.isGrounded = !isAirborne;

        if (!wasGrounded && player.isGrounded) {
            if (player.lastFallVelocity > 25.0f) {
                player.landingTimer = std::min(0.55f, (player.lastFallVelocity - 25.0f) * 0.030f);
            } else {
                player.landingTimer = 0.0f;
            }
            player.lastFallVelocity = 0.0f;
        }

        bool wantsToWalk = std::abs(b2Vel.x) > 0.1f && !isAirborne && (player.landingTimer <= 0.0f);

        auto castNearFoot = [&](float worldX, float footY) -> float {
            return groundCastY(worldX, hipY, 24.0f, pw);
        };

        auto isWallAhead = [&](float dX) -> bool {
            if (std::abs(dX) < 0.01f) return false;
            int dirSign = dX > 0 ? 1 : -1;
            int startX = static_cast<int>(std::round(bodyPos.x + dirSign * 4.0f));
            int endX   = static_cast<int>(std::round(bodyPos.x + dirSign * 7.0f));
            int pyAnkle = static_cast<int>(std::round(bodyPos.y + 12.0f));
            int pyKnee  = static_cast<int>(std::round(bodyPos.y +  6.0f));

            auto checkSolid = [&](int px, int py) {
                if (pw.isEmpty(px, py)) return false;
                BaseComponent* base = pw.get<BaseComponent>(px, py);
                if (base) {
                    Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                    if (logic) {
                        MaterialGroup group = logic->getGroup();
                        if (group == MaterialGroup::Liquid || group == MaterialGroup::Gas) {
                            return false;
                        }
                    }
                }
                return true;
            };

            for (int px = startX; px != endX + dirSign; px += dirSign) {
                if (checkSolid(px, pyAnkle) && checkSolid(px, pyKnee)) return true;
            }
            return false;
        };

        if (wantsToWalk && isWallAhead(b2Vel.x)) {
            wantsToWalk = false;
            if (spriteComp.currentState != "Idle") {
                spriteComp.currentState = "Idle";
                spriteComp.currentFrameIndex = 0;
            }
        }

        {
            auto& as = spriteComp.animations[spriteComp.currentState];
            spriteComp.frameTimer += dt;
            if (spriteComp.frameTimer >= as.frameDuration) {
                spriteComp.frameTimer = 0.0f;
                spriteComp.currentFrameIndex = (spriteComp.currentFrameIndex + 1) % as.frameCount;
            }
            int fx = (as.startFrame + spriteComp.currentFrameIndex) * spriteComp.frameWidth;
            if (spriteComp.flipX)
                spriteComp.sprite->setTextureRect(sf::IntRect({fx + spriteComp.frameWidth, 0}, {-spriteComp.frameWidth, spriteComp.frameHeight}));
            else
                spriteComp.sprite->setTextureRect(sf::IntRect({fx, 0}, {spriteComp.frameWidth, spriteComp.frameHeight}));
        }

        if (isAirborne) {
            anim.steppingLeg    = -1;
            anim.isStopping     = false;
            anim.legA.isPlanted = false;
            anim.legB.isPlanted = false;

            anim.legA.hipOffset = {-2.0f, 0.0f}; 
            anim.legB.hipOffset = { 2.0f, 0.0f}; 

            bool facingLeft = spriteComp.flipX;
            ProceduralLeg* frontLeg = facingLeft ? &anim.legA : &anim.legB;
            ProceduralLeg* backLeg  = facingLeft ? &anim.legB : &anim.legA;

            float vx = b2Vel.x;
            float vy = b2Vel.y;

            float angleVy = vy;
            if (angleVy > -15.0f && angleVy <= 0.0f) angleVy = -15.0f;
            else if (angleVy > 0.0f && angleVy < 15.0f) angleVy = 15.0f;

            float signY = (angleVy < 0.0f) ? -1.0f : 1.0f;
            float d_x = vx * signY;
            float d_y = std::abs(angleVy);
            if (d_x == 0.0f && d_y == 0.0f) d_y = 1.0f;

            float baseAngle = std::atan2(d_x, d_y); 

            float maxAngleAir = 60.0f * PI / 180.0f;
            if (baseAngle > maxAngleAir)  baseAngle = maxAngleAir;
            if (baseAngle < -maxAngleAir) baseAngle = -maxAngleAir;

            float frontAngle, backAngle;
            if (vy < 0.0f) {
                backAngle  = baseAngle;
                frontAngle = -baseAngle;
            } else {
                frontAngle = baseAngle;
                backAngle  = -baseAngle;
            }

            float mappedVy = vy;
            if (mappedVy < -15.0f) mappedVy = -15.0f;
            if (mappedVy >  15.0f) mappedVy =  15.0f;

            float t = (mappedVy + 15.0f) / 30.0f; 
            
            float frontLen = 1.0f + 8.0f * t;
            float backLen  = 9.0f - 8.0f * t;
            
            sf::Vector2f frontDir(std::sin(frontAngle), std::cos(frontAngle));
            sf::Vector2f backDir(std::sin(backAngle),   std::cos(backAngle));

            frontLeg->footWorld = vbp + frontLeg->hipOffset + frontDir * frontLen;
            backLeg->footWorld  = vbp + backLeg->hipOffset  + backDir  * backLen;

            auto preventGroundPenetration = [&](ProceduralLeg* leg) {
                float gY = castNearFoot(leg->footWorld.x, leg->footWorld.y);
                if (gY < NO_GROUND && leg->footWorld.y > gY) {
                    leg->footWorld.y = std::max(gY, minAllowedFootY); 
                }
            };
            preventGroundPenetration(frontLeg);
            preventGroundPenetration(backLeg);

            anim.legA.footTarget = anim.legA.footWorld;
            anim.legB.footTarget = anim.legB.footWorld;

            goto end_hips;
        }

        // GROUNDED
        {
            bool  aIsLeft  = anim.legA.footWorld.x < anim.legB.footWorld.x;
            float targetXA = bodyPos.x + (aIsLeft ? -3.0f :  3.0f);
            float targetXB = bodyPos.x + (aIsLeft ?  3.0f : -3.0f);

            auto plantLeg = [&](ProceduralLeg& leg) {
                float sy = castNearFoot(leg.footWorld.x, leg.footWorld.y);
                if (sy < NO_GROUND) leg.footWorld.y = std::max(sy, minAllowedFootY);
                leg.plantedX  = leg.footWorld.x;
                leg.plantedY  = leg.footWorld.y;
                leg.isPlanted = true;
            };

            auto enforcePlant =[](ProceduralLeg& leg) {
                leg.footWorld.x = leg.plantedX;
                leg.footWorld.y = leg.plantedY;
            };

            if (wantsToWalk) {
                anim.isStopping = false;

                if (anim.steppingLeg == -1) {
                    bool movingRight = b2Vel.x > 0.0f;
                    anim.steppingLeg = movingRight ? (aIsLeft ? 0 : 1) : (aIsLeft ? 1 : 0);
                    anim.stepProgress = 0.0f;

                    ProceduralLeg* sl = (anim.steppingLeg == 0) ? &anim.legA : &anim.legB;
                    ProceduralLeg* pl = (anim.steppingLeg == 0) ? &anim.legB : &anim.legA;

                    sl->footStart = sl->footWorld;
                    sl->isPlanted = false;

                    float look = movingRight ? anim.stepLookahead : -anim.stepLookahead;
                    sl->footTarget = getSafeTarget(look);

                    if (!pl->isPlanted) plantLeg(*pl);
                }

                float speedX    = std::abs(b2Vel.x * M2P);
                float speedRate = (speedX * dt) / anim.strideDistance;

                bool keyHeld = std::abs(b2Vel.x) > 0.5f; 
                float minRate = keyHeld ? (anim.minStepRate * 2.5f * dt) : (anim.minStepRate * dt);

                anim.stepProgress += std::max(speedRate, minRate);

            } else { 
                handle_stopped:
                if (anim.steppingLeg != -1) {
                    anim.isStopping   = true;
                    anim.stepProgress += dt * 4.5f;
                } else {
                    float distA = std::abs(anim.legA.footWorld.x - targetXA);
                    float distB = std::abs(anim.legB.footWorld.x - targetXB);

                    if (distA > 3.0f || distB > 3.0f) {
                        anim.isStopping   = true;
                        anim.steppingLeg  = (distA >= distB) ? 0 : 1;
                        anim.stepProgress = 0.0f;

                        ProceduralLeg* sl = (anim.steppingLeg == 0) ? &anim.legA : &anim.legB;
                        ProceduralLeg* pl = (anim.steppingLeg == 0) ? &anim.legB : &anim.legA;

                        sl->footStart  = sl->footWorld;
                        sl->isPlanted  = false;
                        float tx = (anim.steppingLeg == 0) ? targetXA : targetXB;
                        
                        sl->footTarget = getSafeTarget(tx - bodyPos.x);

                        if (!pl->isPlanted) plantLeg(*pl);
                    } else {
                        auto snapAndLock = [&](ProceduralLeg& leg) {
                            if (leg.isPlanted) enforcePlant(leg);
                            else               plantLeg(leg);
                        };
                        snapAndLock(anim.legA);
                        snapAndLock(anim.legB);
                        goto update_hips;
                    }
                }
            }

            {
                ProceduralLeg* stepLeg = (anim.steppingLeg == 0) ? &anim.legA : &anim.legB;
                ProceduralLeg* plant   = (anim.steppingLeg == 0) ? &anim.legB : &anim.legA;

                if (plant->isPlanted) enforcePlant(*plant);

                float idealLook;
                if (anim.isStopping) {
                    float tx = (anim.steppingLeg == 0) ? targetXA : targetXB;
                    idealLook = tx - bodyPos.x;
                } else {
                    idealLook = b2Vel.x > 0 ? anim.stepLookahead : -anim.stepLookahead;
                }
                
                sf::Vector2f safeIdeal = getSafeTarget(idealLook);

                stepLeg->footTarget.x = lerp(stepLeg->footTarget.x, safeIdeal.x, dt * 20.0f);
                stepLeg->footTarget.y = lerp(stepLeg->footTarget.y, safeIdeal.y, dt * 20.0f);

                float terrainDelta = std::abs(stepLeg->footTarget.y - stepLeg->footStart.y);
                float dynamicArc   = anim.stepArcHeight + std::min(terrainDelta * 0.5f, 8.0f);

                if (anim.stepProgress >= 1.0f) {
                    float landedY = castNearFoot(stepLeg->footTarget.x, stepLeg->footTarget.y);
                    if (landedY >= NO_GROUND) landedY = stepLeg->footTarget.y;

                    stepLeg->footWorld.x = stepLeg->footTarget.x;
                    stepLeg->footWorld.y = std::max(landedY, minAllowedFootY);
                    stepLeg->plantedX    = stepLeg->footWorld.x;
                    stepLeg->plantedY    = stepLeg->footWorld.y;
                    stepLeg->isPlanted   = true;

                    anim.bob.velocity += 8.0f;

                    if (anim.isStopping) {
                        float otherX    = (anim.steppingLeg == 0) ? targetXB : targetXA;
                        float otherDist = std::abs(plant->footWorld.x - otherX);

                        if (otherDist > 1.5f) {
                            anim.steppingLeg  = (anim.steppingLeg == 0) ? 1 : 0;
                            anim.stepProgress = 0.0f;

                            ProceduralLeg* nl = (anim.steppingLeg == 0) ? &anim.legA : &anim.legB;
                            ProceduralLeg* pl = (anim.steppingLeg == 0) ? &anim.legB : &anim.legA;

                            nl->footStart  = nl->footWorld;
                            nl->isPlanted  = false;
                            
                            float tx = (anim.steppingLeg == 0) ? targetXA : targetXB;
                            nl->footTarget = getSafeTarget(tx - bodyPos.x);

                            if (!pl->isPlanted) plantLeg(*pl);
                        } else {
                            anim.steppingLeg = -1;
                            anim.isStopping  = false;
                            if (!plant->isPlanted) plantLeg(*plant);
                        }
                    } else {
                        anim.stepProgress -= 1.0f;
                        anim.steppingLeg   = (anim.steppingLeg == 0) ? 1 : 0;

                        ProceduralLeg* nl = (anim.steppingLeg == 0) ? &anim.legA : &anim.legB;
                        nl->footStart  = nl->footWorld;
                        nl->isPlanted  = false;

                        float look2 = b2Vel.x > 0 ? anim.stepLookahead : -anim.stepLookahead;
                        nl->footTarget = getSafeTarget(look2);
                    }

                } else {
                    float u   = clamp01(anim.stepProgress);
                    float arc = 4.0f * u * (1.0f - u) * dynamicArc;

                    stepLeg->footWorld.x = lerp(stepLeg->footStart.x, stepLeg->footTarget.x, u);
                    stepLeg->footWorld.y = lerp(stepLeg->footStart.y, stepLeg->footTarget.y, u) - arc;

                    float midY = castNearFoot(stepLeg->footWorld.x, stepLeg->footWorld.y);
                    if (midY < NO_GROUND && stepLeg->footWorld.y > midY) {
                        stepLeg->footWorld.y = std::max(midY, minAllowedFootY);
                    }
                    
                    if (stepLeg->footWorld.y < minAllowedFootY) {
                        stepLeg->footWorld.y = minAllowedFootY;
                    }
                }
            }
        }

        update_hips:
        {
            auto updateHip = [&](ProceduralLeg& leg) {
                float relX      = leg.footWorld.x - vbp.x;
                leg.hipOffset.x = std::max(-2.0f, std::min(2.0f, relX * 0.8f));
                leg.hipOffset.y = 0.0f;
            };
            updateHip(anim.legA);
            updateHip(anim.legB);
        }
        end_hips:

        // ============================================================
        // ARMS / HANDS / SWING MECHANICS
        // ============================================================
        {
            float speedX = std::abs(b2Vel.x * M2P);
            
            if (wantsToWalk && !isAirborne) anim.armSwing += dt * speedX * PI / anim.strideDistance;
            else                            anim.armSwing = lerp(anim.armSwing, std::round(anim.armSwing / PI) * PI, dt * 10.0f);

            bool facingLeft = spriteComp.flipX;
            float swingVal  = std::sin(anim.armSwing);

            if (player.isSwinging) {
                player.swingTimer += dt;
                float t = clamp01(player.swingTimer / player.swingDuration);

                spriteComp.flipX = (player.swingTarget.x < bodyPos.x);

                if (t >= 0.5f && !player.swingEffectApplied) {
                    player.swingEffectApplied = true;
                    if (player.equippedWeapon && player.equippedWeapon->isWeapon) {
                        Weapon* w = static_cast<Weapon*>(player.equippedWeapon);
                        w->performSwingEffect(bodyPos, player.swingTarget, pw, *pw.getRigidBodySystem());
                    }

                    sf::Vector2f sw_dir = player.swingTarget - bodyPos;
                    float dist = std::max(length(sw_dir), 1.0f);
                    sf::Vector2f norm = { sw_dir.x / dist, sw_dir.y / dist };
                    
                    anim.handB.recoilVel += norm * 250.0f; 
                    anim.handB.recoilAngularVel += ((std::rand() % 100) / 100.0f - 0.5f) * 800.0f;
                }

                if (t >= 1.0f) {
                    player.isSwinging = false;
                }

                sf::Vector2f sw_dir = player.swingTarget - bodyPos;
                float dist = std::max(length(sw_dir), 1.0f);
                sf::Vector2f F = { sw_dir.x / dist, sw_dir.y / dist };
                
                float sign = spriteComp.flipX ? -1.0f : 1.0f;
                sf::Vector2f Perp = { F.y * sign, -F.x * sign };

                float r_val = player.swingRandomness; 
                float half_width = 16.0f + r_val * 10.0f; 
                float reach      = 22.0f + r_val * 10.0f;
                float base_dist  = 2.0f;

                float s = std::cos(t * PI); 
                
                float x_local = s * half_width;
                float y_local = reach * (1.0f - s * s);
                anim.handB.offset = F * (base_dist + y_local) + Perp * x_local;

                float a = reach / (half_width * half_width);
                sf::Vector2f N_local(2.0f * a * x_local, 1.0f);
                float n_len = std::hypot(N_local.x, N_local.y);
                N_local.x /= n_len;
                N_local.y /= n_len;
                
                sf::Vector2f N_world = Perp * N_local.x + F * N_local.y;
                anim.weaponAngle = std::atan2(N_world.y, N_world.x) * 180.0f / PI + 90.0f;

                float frontBaseX = spriteComp.flipX ? -4.0f : 4.0f;
                anim.handA.offset.x = lerp(anim.handA.offset.x, frontBaseX, dt * 15.0f);
                anim.handA.offset.y = lerp(anim.handA.offset.y, 0.0f, dt * 15.0f);

            } else if (player.isAiming) {
                sf::Vector2f shoulderPos = vbp + sf::Vector2f(0.0f, -4.0f); 
                sf::Vector2f aimDir = player.aimTarget - shoulderPos;
                
                spriteComp.flipX = (aimDir.x < 0.0f);
                facingLeft = spriteComp.flipX;
                
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

                if (desiredHandBOffset.y > 0.0f) {
                    desiredHandBOffset.y = 0.0f;
                }

                anim.handB.offset.x = lerp(anim.handB.offset.x, desiredHandBOffset.x, dt * 15.0f);
                anim.handB.offset.y = lerp(anim.handB.offset.y, desiredHandBOffset.y, dt * 15.0f);
                
                float angleDiff = desiredWeaponAngle - anim.weaponAngle;
                while (angleDiff > 180.0f) angleDiff -= 360.0f;
                while (angleDiff < -180.0f) angleDiff += 360.0f;
                anim.weaponAngle += angleDiff * dt * 25.0f; 

                float facingDir   = facingLeft ? -1.0f : 1.0f;
                float swingOffset = swingVal * 5.0f * facingDir; 
                float swingHeight = -std::pow(swingVal, 2) * 3.0f;
                float frontBaseX  = facingLeft ? -4.0f : 4.0f;

                if (isAirborne) {
                    anim.handA.offset.x = lerp(anim.handA.offset.x, frontBaseX, dt * 10.0f);
                    anim.handA.offset.y = lerp(anim.handA.offset.y, -6.0f, dt * 10.0f);
                } else {
                    anim.handA.offset.x = lerp(anim.handA.offset.x, frontBaseX + swingOffset, dt * 20.0f);
                    anim.handA.offset.y = lerp(anim.handA.offset.y, swingHeight, dt * 20.0f);
                }

            } else {
                float facingDir   = facingLeft ? -1.0f : 1.0f;
                float swingOffset = swingVal * 5.0f * facingDir; 
                float swingHeight = -std::pow(swingVal, 2) * 3.0f;
                float frontBaseX  = facingLeft ? -4.0f :  4.0f;
                float backBaseX   = facingLeft ?  4.0f : -4.0f;

                if (isAirborne) {
                    anim.handA.offset.x = lerp(anim.handA.offset.x, frontBaseX, dt * 10.0f);
                    anim.handA.offset.y = lerp(anim.handA.offset.y, -6.0f, dt * 10.0f);
                    anim.handB.offset.x = lerp(anim.handB.offset.x, backBaseX, dt * 10.0f);
                    anim.handB.offset.y = lerp(anim.handB.offset.y, -6.0f, dt * 10.0f);
                    
                    float desiredWeaponAngle = facingLeft ? -90.0f : 90.0f; 
                    
                    float angleDiff = desiredWeaponAngle - anim.weaponAngle;
                    while (angleDiff > 180.0f) angleDiff -= 360.0f;
                    while (angleDiff < -180.0f) angleDiff += 360.0f;
                    anim.weaponAngle += angleDiff * dt * 15.0f;
                } else {
                    anim.handA.offset.x = lerp(anim.handA.offset.x, frontBaseX + swingOffset, dt * 20.0f);
                    anim.handA.offset.y = lerp(anim.handA.offset.y, swingHeight, dt * 20.0f);
                    anim.handB.offset.x = lerp(anim.handB.offset.x, backBaseX - swingOffset, dt * 20.0f);
                    anim.handB.offset.y = lerp(anim.handB.offset.y, swingHeight, dt * 20.0f);

                    float slopeB = 1.2f * swingVal;
                    float angleDeg = std::atan(slopeB) * 180.0f / PI;

                    float desiredWeaponAngle = facingLeft ? (-90.0f - angleDeg) : (90.0f + angleDeg);
                    
                    float angleDiff = desiredWeaponAngle - anim.weaponAngle;
                    while (angleDiff > 180.0f) angleDiff -= 360.0f;
                    while (angleDiff < -180.0f) angleDiff += 360.0f;
                    anim.weaponAngle += angleDiff * dt * 20.0f;
                }
            }
        }

        float recoilStiff = 120.0f;
        float recoilDamp = 8.0f;
        
        sf::Vector2f springForce = -recoilStiff * anim.handB.recoilPos - recoilDamp * anim.handB.recoilVel;
        sf::Vector2f swirlForce = {-anim.handB.recoilPos.y, anim.handB.recoilPos.x};
        springForce += swirlForce * 20.0f;
        
        anim.handB.recoilVel += springForce * dt;
        anim.handB.recoilPos += anim.handB.recoilVel * dt;
        
        float maxRecoilDist = 35.0f;
        float distSq = anim.handB.recoilPos.x * anim.handB.recoilPos.x + anim.handB.recoilPos.y * anim.handB.recoilPos.y;
        if (distSq > maxRecoilDist * maxRecoilDist) {
            float dist = std::sqrt(distSq);
            anim.handB.recoilPos.x = (anim.handB.recoilPos.x / dist) * maxRecoilDist;
            anim.handB.recoilPos.y = (anim.handB.recoilPos.y / dist) * maxRecoilDist;
        }

        float playerDragMult = 0.02f;
        float mass = b2Body_GetMass(phys.bodyId);
        float originalMass = 0.8f; 
        b2Vec2 dragPlayerForce = {-springForce.x * playerDragMult * (mass / originalMass), -springForce.y * playerDragMult * (mass / originalMass)};
        b2Body_ApplyForceToCenter(phys.bodyId, dragPlayerForce, true);
        
        float angSpringForce = -recoilStiff * anim.handB.recoilAngle - recoilDamp * anim.handB.recoilAngularVel;
        anim.handB.recoilAngularVel += angSpringForce * dt;
        anim.handB.recoilAngle += anim.handB.recoilAngularVel * dt;

        if (anim.handB.recoilAngle > 90.0f) anim.handB.recoilAngle = 90.0f;
        if (anim.handB.recoilAngle < -90.0f) anim.handB.recoilAngle = -90.0f;

        if (!isAirborne) {
            float avgFootY     = (anim.legA.footWorld.y + anim.legB.footWorld.y) * 0.5f;
            float desiredBodyY = avgFootY - 17.0f;
            
            float d_dirX = (b2Vel.x > 0.1f) ? 1.0f : ((b2Vel.x < -0.1f) ? -1.0f : 0.0f);
            float targetDownhill = 0.0f;
            
            if (d_dirX != 0.0f) {
                float gHere = castForTarget(bodyPos.x);
                float gAhead = castForTarget(bodyPos.x + d_dirX * anim.stepLookahead);
                if (gHere < NO_GROUND && gAhead < NO_GROUND) {
                    float diff = gAhead - gHere; 
                    if (diff > 0.0f) { 
                        targetDownhill = std::min(3.0f, diff * 0.8f);
                    }
                }
            }
            
            anim.downhillOffset = lerp(anim.downhillOffset, targetDownhill, dt * 10.0f);
            desiredBodyY += anim.downhillOffset;
            
            float bodyError = desiredBodyY - bodyPos.y;
            float currentMinVy, currentMaxVy, vertStiffness;
            
            if (player.landingTimer > 0.0f) {
                currentMinVy  = -80.0f;
                currentMaxVy  = 80.0f;
                vertStiffness = 18.0f;
            } else {
                currentMinVy  = -250.0f; 
                currentMaxVy  = 150.0f;
                vertStiffness = 40.0f; 
            }
            
            float desiredVy_P = std::max(currentMinVy, std::min(currentMaxVy, bodyError * vertStiffness)); 
            
            b2Vec2 v = b2Body_GetLinearVelocity(phys.bodyId);
            v.y = lerp(v.y, desiredVy_P * P2M, dt * 25.0f); 
            b2Body_SetLinearVelocity(phys.bodyId, v);
        }
    });
}

// =========================================================
// AI GLOBAL PATHFINDING IMPLEMENTATIONS
// =========================================================

bool EntitySystem::isSolid(int cx, int cy, ParticleWorld& pw) {
    if (pw.isEmpty(cx, cy)) return false;
    BaseComponent* b = pw.get<BaseComponent>(cx, cy);
    if (b && b->compMask != 0 && !b->flags.isRigidBodyPart) {
        Particle* p = MaterialRegistry[static_cast<int>(b->id)];
        if (p && p->getGroup() != MaterialGroup::Liquid && p->getGroup() != MaterialGroup::Gas) return true;
    }
    return false;
}

sf::Vector2f EntitySystem::resolveTargetPos(sf::Vector2f clickPos, ParticleWorld& pw) {
    int x = static_cast<int>(clickPos.x);
    int w = static_cast<int>(WORLD_WIDTH);
    int h = static_cast<int>(WORLD_HEIGHT);
    
    if (x < 0) x = 0;
    if (x >= w) x = w - 1;

    int y = static_cast<int>(clickPos.y);
    if (y < 0) y = 0;
    if (y >= h) y = h - 1;
    
    if (isSolid(x, y, pw)) {
        for (int cy = y; cy > 0; --cy) {
            if (!isSolid(x, cy - 1, pw) && isSolid(x, cy, pw)) {
                return sf::Vector2f(x, cy - 1);
            }
        }
    } else {
        for (int cy = y; cy < h - 1; ++cy) {
            if (!isSolid(x, cy, pw) && isSolid(x, cy + 1, pw)) {
                return sf::Vector2f(x, cy);
            }
        }
    }
    return sf::Vector2f(x, y); 
}

void EntitySystem::buildGlobalNavGraph(ParticleWorld& pw) {
    globalNavGraph.clear();
    s_EdgeData.clear();
    
    int width = static_cast<int>(WORLD_WIDTH);
    int height = static_cast<int>(WORLD_HEIGHT);
    
    const int STEP = 16;
    const int CLEARANCE_H = 32; 
    const int CLEARANCE_W = 4;  

    std::map<int, std::vector<int>> columnNodes; 
    
    // 1. Map valid surfaces
    for (int x = STEP; x < width - STEP; x += STEP) {
        for (int y = 1; y < height - 1; ++y) {
            if (!isSolid(x, y - 1, pw) && isSolid(x, y, pw)) {
                bool fits = true;
                for (int cy = y - CLEARANCE_H; cy <= y - 6; ++cy) {
                    for (int cx = x - CLEARANCE_W; cx <= x + CLEARANCE_W; ++cx) {
                        if (isSolid(cx, cy, pw)) { 
                            fits = false; 
                            break; 
                        }
                    }
                    if (!fits) break;
                }
                
                if (fits) {
                    AINode node;
                    node.pos = sf::Vector2f(x, y - 1); 
                    globalNavGraph.push_back(node);
                    columnNodes[x].push_back(globalNavGraph.size() - 1);
                }
            }
        }
    }

    // 2. Connect Walkable Slopes
    for (const auto& [x, indices] : columnNodes) {
        if (columnNodes.find(x + STEP) != columnNodes.end()) {
            const auto& nextIndices = columnNodes[x + STEP];
            
            for (int i : indices) {
                for (int j : nextIndices) {
                    sf::Vector2f p1 = globalNavGraph[i].pos;
                    sf::Vector2f p2 = globalNavGraph[j].pos;
                    
                    if (std::abs(p1.y - p2.y) > 16.0f) continue; 
                    
                    bool blocked = false;
                    for (int s = 1; s < STEP; ++s) {
                        float t = (float)s / STEP;
                        int cx = static_cast<int>(std::round(p1.x + (p2.x - p1.x) * t));
                        float groundY = p1.y + (p2.y - p1.y) * t;
                        
                        for (int cy = static_cast<int>(groundY) - CLEARANCE_H; cy <= static_cast<int>(groundY) - 4; ++cy) {
                            if (isSolid(cx, cy, pw)) { blocked = true; break; }
                        }
                        if (blocked) break;
                    }
                    
                    if (!blocked) {
                        globalNavGraph[i].neighbors.push_back(j);
                        globalNavGraph[j].neighbors.push_back(i);
                        
                        EdgeData walkData;
                        walkData.action = 0;
                        s_EdgeData[{i, j}] = walkData;
                        s_EdgeData[{j, i}] = walkData;
                    }
                }
            }
        }
    }

    // 3. Connect Jump / Fall Parabolas via Simulation
    float sim_dt = 1.0f / 60.0f;
    int max_steps = 75; // 1.25 seconds of simulated flight

    for (size_t i = 0; i < globalNavGraph.size(); ++i) {
        sf::Vector2f startP = globalNavGraph[i].pos;
        
        for (int isJump = 0; isJump <= 1; ++isJump) {
            float speeds[] = {-70.0f, -45.0f, -25.0f, 25.0f, 45.0f, 70.0f}; 
            
            for (float vx : speeds) {
                sf::Vector2f p = startP;
                float currentVx = vx;
                float currentVy = isJump ? -380.0f : 0.0f; 
                
                std::vector<sf::Vector2f> traj;
                traj.push_back(p);
                
                bool isAirborne = (isJump == 1); // Only start airborne if jumping
                bool hitGround = false;
                
                for (int step = 0; step < max_steps; ++step) {
                    
                    if (isAirborne) {
                        currentVy += 980.0f * sim_dt; 
                    } else {
                        currentVy = 0.0f;
                        
                        // Check if we have walked off the edge of the starting platform
                        int checkX = static_cast<int>(std::round(p.x));
                        int groundY = static_cast<int>(std::round(startP.y + 1.0f));
                        
                        if (checkX < 0 || checkX >= width) break;
                        
                        // If there is no solid ground directly beneath us, we are now falling
                        if (!isSolid(checkX, groundY, pw)) {
                            isAirborne = true;
                        }
                    }
                    
                    currentVx *= std::max(0.0f, 1.0f - 1.0f * sim_dt);
                    if (isAirborne) {
                        currentVy *= std::max(0.0f, 1.0f - 1.0f * sim_dt);
                    }
                    
                    p.x += currentVx * sim_dt;
                    p.y += currentVy * sim_dt;
                    
                    if (!isAirborne) {
                        p.y = startP.y; // Keep Y snapped to the platform until we step off
                    }
                    
                    if (p.x < 0 || p.x >= width || p.y < 0 || p.y >= height) break;
                    
                    bool blocked = false;
                    for (int cy = static_cast<int>(p.y) - CLEARANCE_H+8; cy <= static_cast<int>(p.y) - 6; cy += 4) {
                        if (isSolid(p.x, cy, pw) || isSolid(p.x - 2, cy, pw) || isSolid(p.x + 2, cy, pw)) {
                            blocked = true; break;
                        }
                    }
                    if (blocked) break;
                    
                    traj.push_back(p);
                    
                    if (isAirborne && currentVy > 0.0f) {
                        if (isSolid(p.x, p.y + 1, pw)) {
                            hitGround = true;
                            break;
                        }
                    }
                }
                
                if (hitGround) {
                    int closest = -1;
                    float minDist = 16.0f; 
                    for (size_t j = 0; j < globalNavGraph.size(); ++j) {
                        if (i == j) continue;
                        float dist = std::hypot(globalNavGraph[j].pos.x - p.x, globalNavGraph[j].pos.y - p.y);
                        if (dist < minDist) {
                            minDist = dist;
                            closest = j;
                        }
                    }
                    
                    if (closest != -1) {
                        auto edgeKey = std::make_pair(static_cast<int>(i), closest);
                        bool isRedundant = (s_EdgeData.find(edgeKey) != s_EdgeData.end());

                        if (!isRedundant && std::abs(startP.y - globalNavGraph[closest].pos.y) <= 32.0f) {
                            bool gapFound = false;
                            float minX = std::min(startP.x, globalNavGraph[closest].pos.x);
                            float maxX = std::max(startP.x, globalNavGraph[closest].pos.x);
                            
                            if (maxX - minX > 16.0f) {
                                for (float px = minX + 8.0f; px < maxX; px += 8.0f) {
                                    bool groundFound = false;
                                    for (int cy = startP.y - 16; cy <= startP.y + 48; ++cy) {
                                        if (isSolid(px, cy, pw)) { groundFound = true; break; }
                                    }
                                    if (!groundFound) { gapFound = true; break; }
                                }
                                if (!gapFound) isRedundant = true;
                            } else {
                                isRedundant = true;
                            }
                        }

                        if (!isRedundant) {
                            globalNavGraph[i].neighbors.push_back(closest);
                            EdgeData data;
                            data.action = isJump ? 1 : 2;
                            data.dir = (vx > 0) ? 1.0f : -1.0f;
                            data.trajectory = traj;
                            data.requiredVx = vx;
                            s_EdgeData[edgeKey] = data;
                        }
                    }
                }
            }
        }
    }
    
    globalGraphBuilt = true;
    std::cout << "NavMesh Built! Total Nodes: " << globalNavGraph.size() << "\n";
}
int EntitySystem::getClosestNode(sf::Vector2f pos) {
    int bestIdx = -1;
    float bestDist = 1e9f;
    for (size_t i = 0; i < globalNavGraph.size(); ++i) {
        float dist = std::hypot(globalNavGraph[i].pos.x - pos.x, globalNavGraph[i].pos.y - pos.y);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}
std::vector<PathNodeData> EntitySystem::findPath(sf::Vector2f start, sf::Vector2f target) {
    if (globalNavGraph.empty()) return {};

    int startIdx = getClosestNode(start);
    int targetIdx = getClosestNode(target);

    if (startIdx == -1 || targetIdx == -1) return {};

    std::vector<float> gScore(globalNavGraph.size(), 1e9f);
    std::vector<int> cameFrom(globalNavGraph.size(), -1);
    std::vector<bool> closed(globalNavGraph.size(), false); // Added visited list
    gScore[startIdx] = 0.0f;

    auto cmp = [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; };
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, decltype(cmp)> pq(cmp);
    pq.push({0.0f, startIdx});

    int closestReachableIdx = startIdx;
    float minH = std::hypot(globalNavGraph[startIdx].pos.x - globalNavGraph[targetIdx].pos.x, 
                            globalNavGraph[startIdx].pos.y - globalNavGraph[targetIdx].pos.y);

    while (!pq.empty()) {
        int curr = pq.top().second;
        pq.pop();

        // Prevent processing a node if we have already closed it with a shorter route
        if (closed[curr]) continue;
        closed[curr] = true;

        if (curr == targetIdx) {
            closestReachableIdx = targetIdx;
            break;
        }

        float hCurr = std::hypot(globalNavGraph[curr].pos.x - globalNavGraph[targetIdx].pos.x, 
                                 globalNavGraph[curr].pos.y - globalNavGraph[targetIdx].pos.y);
        if (hCurr < minH) {
            minH = hCurr;
            closestReachableIdx = curr;
        }

        for (int nxt : globalNavGraph[curr].neighbors) {
            float dist = std::hypot(globalNavGraph[nxt].pos.x - globalNavGraph[curr].pos.x, 
                                    globalNavGraph[nxt].pos.y - globalNavGraph[curr].pos.y);
                                    
            // Significantly reduce the jump penalty so complex multi-jump paths do not get outright rejected
            auto edgeIt = s_EdgeData.find({curr, nxt});
            if (edgeIt != s_EdgeData.end() && edgeIt->second.action != 0) {
                dist += 150.0f; // Represents the "effort" of jumping, preferring flat walks if both options exist.
            }
            
            float tentative = gScore[curr] + dist;
            
            if (tentative < gScore[nxt]) {
                gScore[nxt] = tentative;
                cameFrom[nxt] = curr;
                float h = std::hypot(globalNavGraph[nxt].pos.x - globalNavGraph[targetIdx].pos.x, 
                                     globalNavGraph[nxt].pos.y - globalNavGraph[targetIdx].pos.y);
                pq.push({tentative + h, nxt});
            }
        }
    }

    std::vector<int> pathIndices;
    int curr = closestReachableIdx;
    while (curr != -1) {
        pathIndices.push_back(curr);
        curr = cameFrom[curr];
    }
    std::reverse(pathIndices.begin(), pathIndices.end());

    std::vector<PathNodeData> finalPath;
    if (!pathIndices.empty()) {
        finalPath.push_back(PathNodeData{globalNavGraph[pathIndices[0]].pos, false, false, false, 0.0f});
    }

    // Construct the structured PathNodeData list, encoding exact jump actions where the navmesh says so!
    for (size_t i = 1; i < pathIndices.size(); ++i) {
        int prev = pathIndices[i - 1];
        int nxt = pathIndices[i];
        
        bool isJump = false;
        bool isFall = false;
        float reqVx = 0.0f;
        auto edgeIt = s_EdgeData.find({prev, nxt});
        if (edgeIt != s_EdgeData.end() && edgeIt->second.action != 0) {
            if (edgeIt->second.action == 1) isJump = true;
            if (edgeIt->second.action == 2) isFall = true;
            reqVx = edgeIt->second.requiredVx;
            
            // Retroactively mark the preceding position as a takeoff strict-tolerance point
            finalPath[i - 1].isJumpTakeoff = true; 
        }
        
        finalPath.push_back(PathNodeData{globalNavGraph[nxt].pos, isJump, isFall, false, reqVx});
    }

    return finalPath;
}

// =========================================================
// RENDERERS
// =========================================================

void EntitySystem::renderDebug(sf::RenderTarget& target) {
    sf::VertexArray lines(sf::PrimitiveType::Lines);

    for (size_t i = 0; i < globalNavGraph.size(); ++i) {
        sf::Vector2f p1 = globalNavGraph[i].pos;
        for (int neighborIdx : globalNavGraph[i].neighbors) {
            auto it = s_EdgeData.find({static_cast<int>(i), neighborIdx});
            if (it != s_EdgeData.end() && it->second.action != 0 && !it->second.trajectory.empty()) {
                // Draw simulated jump parabolas in Pink
                for (size_t t = 0; t + 1 < it->second.trajectory.size(); ++t) {
                    lines.append(sf::Vertex{it->second.trajectory[t], sf::Color(255, 0, 255, 120)});
                    lines.append(sf::Vertex{it->second.trajectory[t+1], sf::Color(255, 0, 255, 120)});
                }
            } else {
                // Draw normal walking edges in Green
                sf::Vector2f p2 = globalNavGraph[neighborIdx].pos;
                lines.append(sf::Vertex{p1, sf::Color(0, 255, 0, 80)});
                lines.append(sf::Vertex{p2, sf::Color(0, 255, 0, 80)});
            }
        }
        lines.append(sf::Vertex{p1 + sf::Vector2f(-1, 0), sf::Color::Cyan});
        lines.append(sf::Vertex{p1 + sf::Vector2f(1, 0), sf::Color::Cyan});
    }

    for (const auto& line : debugLines) {
        lines.append(sf::Vertex{line.p1, line.color});
        lines.append(sf::Vertex{line.p2, line.color});
    }

    if (lines.getVertexCount() > 0) {
        target.draw(lines);
    }
}

void EntitySystem::drawPixelatedHand(sf::RenderTarget& target, const sf::Vector2f& center, sf::Color col) {
    int cx = static_cast<int>(std::round(center.x));
    int cy = static_cast<int>(std::round(center.y));

    sf::VertexArray pixels(sf::PrimitiveType::Triangles);

    auto addPixel = [&](int px, int py) {
        sf::Vector2f tl(static_cast<float>(px), static_cast<float>(py));
        sf::Vector2f tr(static_cast<float>(px + 1), static_cast<float>(py));
        sf::Vector2f br(static_cast<float>(px + 1), static_cast<float>(py + 1));
        sf::Vector2f bl(static_cast<float>(px), static_cast<float>(py + 1));

        pixels.append(sf::Vertex{tl, col}); pixels.append(sf::Vertex{tr, col}); pixels.append(sf::Vertex{br, col});
        pixels.append(sf::Vertex{tl, col}); pixels.append(sf::Vertex{br, col}); pixels.append(sf::Vertex{bl, col});
    };

    addPixel(cx, cy - 1); addPixel(cx - 1, cy); addPixel(cx, cy); addPixel(cx + 1, cy); addPixel(cx, cy + 1);
    target.draw(pixels);
}

void EntitySystem::drawPixelatedLeg(sf::RenderTarget& target, const sf::Vector2f& hip, const sf::Vector2f& foot, sf::Color col) {
    int x0 = static_cast<int>(std::round(hip.x)); int y0 = static_cast<int>(std::round(hip.y));
    int x1 = static_cast<int>(std::round(foot.x)); int y1 = static_cast<int>(std::round(foot.y));

    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1,  sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    sf::VertexArray pixels(sf::PrimitiveType::Triangles);

    while (true) {
        sf::Vector2f tl(static_cast<float>(x0), static_cast<float>(y0));
        sf::Vector2f tr(static_cast<float>(x0 + 1), static_cast<float>(y0));
        sf::Vector2f br(static_cast<float>(x0 + 1), static_cast<float>(y0 + 1));
        sf::Vector2f bl(static_cast<float>(x0), static_cast<float>(y0 + 1));

        pixels.append(sf::Vertex{tl, col}); pixels.append(sf::Vertex{tr, col}); pixels.append(sf::Vertex{br, col});
        pixels.append(sf::Vertex{tl, col}); pixels.append(sf::Vertex{br, col}); pixels.append(sf::Vertex{bl, col});

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
    target.draw(pixels);
}

void EntitySystem::renderEntities(sf::RenderTarget& target) {
    auto view = registry.view<PlayerControllerComponent, PhysicsComponent, ProceduralAnimationComponent, SpriteSheetComponent>();
    view.each([&](auto, auto& player, auto& phys, auto& anim, auto& spriteComp) {

        b2Vec2 b2Pos = b2Body_GetPosition(phys.bodyId);
        sf::Vector2f bodyPos(b2Pos.x * M2P, b2Pos.y * M2P);
        b2Rot rot = b2Body_GetRotation(phys.bodyId);
        float bodyAng = std::atan2(rot.s, rot.c);
        sf::Vector2f vbp(bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY);

        drawPixelatedHand(target, vbp + anim.handA.offset, sf::Color(180, 180, 180));
        drawPixelatedLeg(target, vbp + anim.legA.hipOffset, anim.legA.footWorld, sf::Color::White);
        drawPixelatedLeg(target, vbp + anim.legB.hipOffset, anim.legB.footWorld, sf::Color::White);

        if (spriteComp.sprite.has_value()) {
            float renderAngle = bodyAng * 180.0f / PI;
            sf::Vector2f renderPos = bodyPos;
            
            if (player.isRagdoll) {
                spriteComp.sprite->setOrigin({15.5f, 16.0f});
            } else {
                renderPos.y += anim.bob.offsetY;
                spriteComp.sprite->setOrigin({15.5f, 16.0f});
            }
            
            sf::VertexArray va(sf::PrimitiveType::Triangles);
            float rad = -renderAngle * PI / 180.0f;
            float r_cs = std::cos(rad);
            float r_sn = std::sin(rad);

            sf::IntRect texRect = spriteComp.sprite->getTextureRect();
            sf::Vector2f origin = spriteComp.sprite->getOrigin();
            int width = std::abs(texRect.size.x);
            int height = std::abs(texRect.size.y);
            
            float radius = std::hypot(width, height) + 1.0f;
            int maxD = static_cast<int>(std::ceil(radius));

            for (int dy = -maxD; dy <= maxD; ++dy) {
                for (int dx = -maxD; dx <= maxD; ++dx) {
                    float rx = dx * r_cs - dy * r_sn;
                    float ry = dx * r_sn + dy * r_cs;

                    int lx = static_cast<int>(std::round(rx + origin.x));
                    int ly = static_cast<int>(std::round(ry + origin.y));

                    if (lx >= 0 && lx < width && ly >= 0 && ly < height) {
                        int tx = (texRect.size.x < 0) ? (texRect.position.x - 1 - lx) : (texRect.position.x + lx);
                        int ty = (texRect.size.y < 0) ? (texRect.position.y - 1 - ly) : (texRect.position.y + ly);

                        sf::Vector2f pxPos(renderPos.x + dx, renderPos.y + dy);
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
            states.texture = &spriteComp.sprite->getTexture();
            target.draw(va, states);
        }

        drawPixelatedHand(target, vbp + anim.handB.offset + anim.handB.recoilPos, sf::Color::White);

        if (player.equippedWeapon) {
            float renderAngle = anim.weaponAngle + anim.handB.recoilAngle;
            sf::Vector2f renderHandPos = vbp + anim.handB.offset + anim.handB.recoilPos;
            player.equippedWeapon->renderPixelated(target, renderHandPos, renderAngle, spriteComp.flipX);
        }
    });
}

sf::Vector2f EntitySystem::getPlayerPos() const {
    auto view = registry.view<PhysicsComponent>();
    for (auto [entity, phys] : view.each()) {
        b2Vec2 p = b2Body_GetPosition(phys.bodyId);
        return {p.x * M2P, p.y * M2P};
    }
    return {0.f, 0.f};
}

void EntitySystem::killAndRagdollEntity(entt::entity e, ParticleWorld& pw, MaterialID mat) {
    auto* phys = registry.try_get<PhysicsComponent>(e);
    if (!phys) return;

    b2Vec2 pos = b2Body_GetPosition(phys->bodyId);
    sf::Image img;
    img.resize({5, 16}, sf::Color::Transparent);
    for (unsigned py = 0; py < 16; ++py)
        for (unsigned px = 0; px < 5; ++px)
            img.setPixel({px, py}, sf::Color::Red);

    pw.addRigidBodyFromSprite(img, int(pos.x * M2P - 2.5f), int(pos.y * M2P - 8.0f), mat);
    b2DestroyBody(phys->bodyId);
    registry.destroy(e);
}