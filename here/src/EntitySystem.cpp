#include "EntitySystem.hpp"
#include "Weapon.hpp"
#include <cmath>
#include <iostream>
#include <cstdlib>
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

EntitySystem::EntitySystem(b2WorldId physWorld) : physicsWorldId(physWorld) {
    sf::Image dummy;
    dummy.resize(sf::Vector2u(32, 32), sf::Color(100, 100, 100));
    defaultPlayerTexture = std::make_shared<sf::Texture>(dummy);
}
EntitySystem::~EntitySystem() { registry.clear(); }

entt::entity EntitySystem::spawnPlayer(float x, float y, const std::string& texturePath) {
    auto entity = registry.create();

    b2BodyDef bdef      = b2DefaultBodyDef();
    bdef.type           = b2_dynamicBody;
    bdef.position.x     = x * P2M;
    bdef.position.y     = y * P2M;
    bdef.linearDamping  = 1.0f;
    
    // Moderate angular damping so the player doesn't spin out of control, but isn't completely stiff
    bdef.angularDamping = 10.0f;

    b2BodyId bodyId = b2CreateBody(physicsWorldId, &bdef);
    b2Polygon  box      = b2MakeBox(2.5f * P2M, 8.0f * P2M); 
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    
    // Density set to 10.0f. Heavy enough to push things, but light enough to be pushed slightly.
    shapeDef.density           = 10.0f; 
    shapeDef.material.friction = 0.1f;
    b2CreatePolygonShape(bodyId, &shapeDef, &box);

    registry.emplace<PhysicsComponent>(entity, bodyId);
    registry.emplace<PlayerControllerComponent>(entity);

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
            if (player.equippedWeapon->isGun) {
                continue; // Guns do not swing like melee weapons
            }
            player.isSwinging = true;
            player.swingTimer = 0.0f;
            player.swingEffectApplied = false;
            player.swingTarget = targetWorldPos;
            player.swingRandomness = ((rand() % 100) / 100.0f) * 0.4f - 0.2f;
        }
    }
}

void EntitySystem::updateInput(float dt, sf::Vector2f mouseWorldPos, RigidBodySystem& rbs, ParticleWorld& pw) {
    auto view = registry.view<PlayerControllerComponent, PhysicsComponent, SpriteSheetComponent, ProceduralAnimationComponent>();
    view.each([&](auto, auto& player, auto& phys, auto& sprite, auto& anim) {
        b2Vec2 vel = b2Body_GetLinearVelocity(phys.bodyId);
        b2Vec2 pos = b2Body_GetPosition(phys.bodyId);
        sf::Vector2f bodyPos(pos.x * M2P, pos.y * M2P);

        float dir = 0.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) dir = -1.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) dir =  1.0f;
        
        bool rightClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
        bool leftClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);

        // --- RAGDOLL TOGGLE ---
        bool fPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F);
        
        b2Rot currentRot = b2Body_GetRotation(phys.bodyId);
        float currentBodyAng = std::atan2(currentRot.s, currentRot.c);
        float maxAngle = 45.0f * PI / 180.0f;
        
        // Force ragdoll if we fall over past 45 degrees
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
                    
                    if (isCircle) {
                        b2Circle circle = {{0, 0}, w * P2M};
                        b2CreateCircleShape(partId, &shapeDef, &circle);
                    } else {
                        b2Polygon box = b2MakeBox(w * P2M, h * P2M);
                        b2CreatePolygonShape(partId, &shapeDef, &box);
                    }
                    return partId;
                };

                auto createRevoluteJoint = [&](b2BodyId bA, b2BodyId bB, b2Vec2 anchorWorldPx) {
                    b2RevoluteJointDef jd = b2DefaultRevoluteJointDef();
                    jd.bodyIdA = bA;
                    jd.bodyIdB = bB;
                    jd.localAnchorA = b2Body_GetLocalPoint(bA, {anchorWorldPx.x * P2M, anchorWorldPx.y * P2M});
                    jd.localAnchorB = b2Body_GetLocalPoint(bB, {anchorWorldPx.x * P2M, anchorWorldPx.y * P2M});
                    jd.enableLimit = true;
                    jd.lowerAngle = -PI/1.5f;
                    jd.upperAngle = PI/1.5f;
                    jd.collideConnected = false; 
                    b2CreateRevoluteJoint(physicsWorldId, &jd);
                };
                
                auto createDistanceJoint = [&](b2BodyId bA, b2BodyId bB, b2Vec2 anchorWorldPx) {
                    b2DistanceJointDef djd = b2DefaultDistanceJointDef();
                    djd.bodyIdA = bA;
                    djd.bodyIdB = bB;
                    djd.localAnchorA = b2Body_GetLocalPoint(bA, {anchorWorldPx.x * P2M, anchorWorldPx.y * P2M});
                    djd.localAnchorB = b2Vec2_zero; 
                    djd.minLength = 0.0f;
                    djd.maxLength = 9.0f * P2M;
                    djd.collideConnected = false; 
                    b2CreateDistanceJoint(physicsWorldId, &djd);
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
                b2Polygon box = b2MakeBox(2.5f * P2M, 8.0f * P2M); 
                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.density = 10.0f;
                shapeDef.material.friction = 0.1f;
                b2CreatePolygonShape(phys.bodyId, &shapeDef, &box);

                if (b2Body_IsValid(anim.legA.ragdollBodyId)) b2DestroyBody(anim.legA.ragdollBodyId);
                if (b2Body_IsValid(anim.legB.ragdollBodyId)) b2DestroyBody(anim.legB.ragdollBodyId);
                if (b2Body_IsValid(anim.handA.ragdollBodyId)) b2DestroyBody(anim.handA.ragdollBodyId);
                if (b2Body_IsValid(anim.handB.ragdollBodyId)) b2DestroyBody(anim.handB.ragdollBodyId);
                
                anim.legA.ragdollBodyId = b2_nullBodyId; anim.legB.ragdollBodyId = b2_nullBodyId;
                anim.handA.ragdollBodyId = b2_nullBodyId; anim.handB.ragdollBodyId = b2_nullBodyId;

                anim.bob.offsetY = 0.0f;
                anim.bob.velocity = 0.0f;
            }
        }
        player.fPressedLastFrame = fPressed;

        if (player.isRagdoll) {
            player.isAiming = false;
            player.isSwinging = false;
            player.leftClickPressedLastFrame = leftClick;
            player.wPressedLastFrame = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
            player.ePressedLastFrame = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E);
            return; 
        }

        // --- UPRIGHT SPRING (PD Controller) ---
        b2Rot rot = b2Body_GetRotation(phys.bodyId);
        float bodyAng = std::atan2(rot.s, rot.c);
        float angVel = b2Body_GetAngularVelocity(phys.bodyId);

        // PERFECT RESTORATION: Snap to exactly 0 to fix pixelated sprite rotation artifacts
        if (std::abs(bodyAng) < 0.03f && std::abs(angVel) < 0.5f) {
            if (bodyAng != 0.0f || angVel != 0.0f) {
                b2Body_SetTransform(phys.bodyId, b2Body_GetPosition(phys.bodyId), b2MakeRot(0.0f));
                b2Body_SetAngularVelocity(phys.bodyId, 0.0f);
                bodyAng = 0.0f;
                angVel = 0.0f;
            }
        }
        
        // Check if the body is currently being pushed further away from 0
        bool movingAway = (bodyAng * angVel > 0.0f);
        
        float stiffness;
        float damping;
        
        if (movingAway) {
            // EASILY ROTATED: Yield easily to impacts pushing us away from upright.
            // Even a hit near the middle (low torque) easily overcomes this low stiffness.
            stiffness = 15.0f; 
            damping = 2.0f;
        } else {
            // FASTER RECOVERY: Strong force returning us to upright.
            // Critically damped (~ 2 * sqrt(stiffness)) to guarantee we zoom back without bouncing.
            stiffness = 250.0f;
            damping = 31.0f; 
        }
        
        // Soft-cap near max angle to heavily resist right before going ragdoll
        if (std::abs(bodyAng) > maxAngle * 0.7f) {
            float excess = (std::abs(bodyAng) - maxAngle * 0.7f) / (maxAngle * 0.3f);
            stiffness += excess * 600.0f;
            damping += excess * 40.0f;
        }

        float torque = (-bodyAng * stiffness - angVel * damping) * b2Body_GetMass(phys.bodyId);
        b2Body_ApplyTorque(phys.bodyId, torque, true);

        if (rightClick && player.equippedWeapon) {
            player.isAiming = true;
            player.aimTarget = mouseWorldPos;
            
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

                        float playerKickMult = 0.5f;
                        float effectiveMass = 0.8f; 
                        
                        vel.x += (-shootDir.x * w->recoilForce * playerKickMult) / effectiveMass;
                        vel.y += (-shootDir.y * w->recoilForce * playerKickMult) / effectiveMass;
                    }
                }
            }
        } else {
            player.isAiming = false;
        }
        
        player.leftClickPressedLastFrame = leftClick;
        
        if (player.fireTimer > 0.0f) {
            player.fireTimer -= dt;
        }
        
        float speedFactor = 1.0f;
        
        if (player.isGrounded && dir != 0.0f) {
            float maxDx = 0.0f;
            int maxLook = static_cast<int>(std::ceil(anim.stepLookahead));
            float baseCastFrom = bodyPos.y +8;
            
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

            // =================================================================================
            // NEW: Prevent pushing the rigid body we are currently standing on
            // =================================================================================
            b2QueryFilter filter = b2DefaultQueryFilter();
            
            // 1. Sideways Raycast (Parallel to body, exact height, 1 pixel away, rotates with body)
            b2Transform xf = b2Body_GetTransform(phys.bodyId);
            float localX = dir * 3.5f * P2M; // 2.5 half-width + 1.0 pixel
            b2Vec2 localTop = {localX, -8.0f * P2M};
            b2Vec2 localBottom = {localX, 8.0f * P2M};
            
            b2Vec2 worldTop = b2TransformPoint(xf, localTop);
            b2Vec2 worldBottom = b2TransformPoint(xf, localBottom);
            b2Vec2 sideTrans = {worldBottom.x - worldTop.x, worldBottom.y - worldTop.y};
            
            b2RayResult sideHit = b2World_CastRayClosest(physicsWorldId, worldTop, sideTrans, filter);
            
            if (sideHit.hit) {
                b2BodyId sideBody = b2Shape_GetBody(sideHit.shapeId);
                
                if (sideBody.index1 != phys.bodyId.index1) { // Ignore hitting ourselves
                    // 2. Downward Raycasts (One for each leg, 1 pixel wide, 0.5 pixels under foot)
                    auto checkLeg = [&](const ProceduralLeg& leg) -> bool {
                        b2Vec2 origin = {(leg.footWorld.x - 0.5f) * P2M, (leg.footWorld.y + 0.5f) * P2M};
                        b2Vec2 trans = {1.0f * P2M, 0.0f}; // 1 pixel wide horizontal line
                        b2RayResult hit = b2World_CastRayClosest(physicsWorldId, origin, trans, filter);
                        if (hit.hit) {
                            b2BodyId hitBody = b2Shape_GetBody(hit.shapeId);
                            return (hitBody.index1 == sideBody.index1 && hitBody.generation == sideBody.generation);
                        }
                        return false;
                    };

                    if (checkLeg(anim.legA) || checkLeg(anim.legB)) {
                        speedFactor = 0.0f;
                        vel.x = 0.0f; // Instantly kill velocity to prevent micro-pushes
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

        if (player.isGrounded) {
            vel.x = lerp(vel.x, desiredVelX, dt * 8.0f);
        }

        bool wPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
        if (wPressed && !player.wPressedLastFrame && player.isGrounded && player.landingTimer <= 0.0f) {
            vel.y = player.jumpForce;
        }
        player.wPressedLastFrame = wPressed;

        if      (!player.isGrounded)         sprite.currentState = "Jump";
        else if (player.landingTimer > 0.0f) sprite.currentState = "Idle";
        else if (std::abs(vel.x) > 1.0f)     sprite.currentState = "Walk";
        else                                 sprite.currentState = "Idle";

        b2Body_SetLinearVelocity(phys.bodyId, vel);

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E)) {
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
                    // Ignore liquids and gas completely for ground detection
                    if (group != MaterialGroup::Liquid && group != MaterialGroup::Gas) {
                        return static_cast<float>(py - 1);
                    }
                }
            }
        }
    }
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

        // ---- Body-bob spring ----
        {
            float f = -anim.bob.stiffness * anim.bob.offsetY - anim.bob.damping * anim.bob.velocity;
            anim.bob.velocity += f * dt;
            anim.bob.offsetY  += anim.bob.velocity * dt;
        }

        sf::Vector2f vbp(bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY);
        
        // EXACT FIX: We establish the hip coordinates as the absolute ceiling of our raycasts.
        float hipY = vbp.y;
        float minAllowedFootY = vbp.y + 4.0f; // STRICT LIMIT: Feet can never go higher than 4 pixels below the hip

        auto castForTarget = [&](float worldX) -> float {
            return groundCastY(worldX, hipY, 24.0f, pw);
        };
        
        auto getSafeTarget = [&](float lookOffset) -> sf::Vector2f {
            float dir = lookOffset > 0 ? 1.0f : (lookOffset < 0 ? -1.0f : 0.0f);
            float maxDist = std::abs(lookOffset);
            float bestX = bodyPos.x;
            
            float bestY = groundCastY(bodyPos.x, hipY, 24.0f, pw);
            if (bestY >= NO_GROUND) bestY = bodyPos.y + 17.0f;

            if (maxDist < 0.1f) return {bestX, std::max(bestY, minAllowedFootY)};

            float minFootY = bodyPos.y + 12.0f;
            float maxFootY = bodyPos.y + 22.0f; 

            for (int i = 1; i <= static_cast<int>(std::ceil(maxDist)); ++i) {
                float testX = bodyPos.x + dir * i;
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
        
        // === MODIFIED HERE: Changed from -5.0f to -15.0f to prevent heavy items from causing glitchy air detection ===
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

        auto isWallAhead = [&](float dirX) -> bool {
            if (std::abs(dirX) < 0.01f) return false;
            int dirSign = dirX > 0 ? 1 : -1;
            int startX = static_cast<int>(std::round(bodyPos.x + dirSign * 4.0f));
            int endX   = static_cast<int>(std::round(bodyPos.x + dirSign * 7.0f));
            int pyAnkle = static_cast<int>(std::round(bodyPos.y + 12.0f));
            int pyKnee  = static_cast<int>(std::round(bodyPos.y +  6.0f));

            auto isSolid = [&](int px, int py) {
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
                if (isSolid(px, pyAnkle) && isSolid(px, pyKnee)) return true;
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

        // ---- Sprite sheet ----
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
            float dx = vx * signY;
            float dy = std::abs(angleVy);
            if (dx == 0.0f && dy == 0.0f) dy = 1.0f;

            float baseAngle = std::atan2(dx, dy); 

            float maxAngle = 60.0f * PI / 180.0f;
            if (baseAngle > maxAngle)  baseAngle = maxAngle;
            if (baseAngle < -maxAngle) baseAngle = -maxAngle;

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

                bool keyHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
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

                    sf::Vector2f dir = player.swingTarget - bodyPos;
                    float dist = std::max(length(dir), 1.0f);
                    sf::Vector2f norm = { dir.x / dist, dir.y / dist };
                    
                    anim.handB.recoilVel += norm * 250.0f; 
                    anim.handB.recoilAngularVel += ((std::rand() % 100) / 100.0f - 0.5f) * 800.0f;
                }

                if (t >= 1.0f) {
                    player.isSwinging = false;
                }

                sf::Vector2f dir = player.swingTarget - bodyPos;
                float dist = std::max(length(dir), 1.0f);
                sf::Vector2f F = { dir.x / dist, dir.y / dist };
                
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

        float stiffness = 120.0f;
        float damping = 8.0f;
        
        sf::Vector2f springForce = -stiffness * anim.handB.recoilPos - damping * anim.handB.recoilVel;
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
        
        float angSpringForce = -stiffness * anim.handB.recoilAngle - damping * anim.handB.recoilAngularVel;
        anim.handB.recoilAngularVel += angSpringForce * dt;
        anim.handB.recoilAngle += anim.handB.recoilAngularVel * dt;

        if (anim.handB.recoilAngle > 90.0f) anim.handB.recoilAngle = 90.0f;
        if (anim.handB.recoilAngle < -90.0f) anim.handB.recoilAngle = -90.0f;

        if (!isAirborne) {
            float avgFootY     = (anim.legA.footWorld.y + anim.legB.footWorld.y) * 0.5f;
            float desiredBodyY = avgFootY - 17.0f;
            
            float dirX = (b2Vel.x > 0.1f) ? 1.0f : ((b2Vel.x < -0.1f) ? -1.0f : 0.0f);
            float targetDownhill = 0.0f;
            
            if (dirX != 0.0f) {
                float gHere = castForTarget(bodyPos.x);
                float gAhead = castForTarget(bodyPos.x + dirX * anim.stepLookahead);
                if (gHere < NO_GROUND && gAhead < NO_GROUND) {
                    float diff = gAhead - gHere; 
                    if (diff > 0.0f) { 
                        targetDownhill = std::min(3.0f, diff * 0.8f);
                    }
                }
            }
            
            anim.downhillOffset = lerp(anim.downhillOffset, targetDownhill, dt * 10.0f);
            desiredBodyY += anim.downhillOffset;
            
            // === MODIFIED HERE: Dynamic Leg Strength Based On Landing Pause ===
            float bodyError = desiredBodyY - bodyPos.y;
            
            float currentMinVy, currentMaxVy, vertStiffness;
            
            if (player.landingTimer > 0.0f) {
                // WEAK LEGS: During the landing pause, keep values soft so we don't bounce and cancel the pause
                currentMinVy  = -80.0f;
                currentMaxVy  = 80.0f;
                vertStiffness = 18.0f;
            } else {
                // STRONG LEGS: Pause is over, player has strength to push heavy objects on head
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

void EntitySystem::drawPixelatedLeg(sf::RenderTarget& target, const sf::Vector2f& hip,
                                    const sf::Vector2f& foot, sf::Color col) {
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
        
        // We use the old vbp logic so the sprite and limbs render exactly as they did before
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
            
            // Custom pixelated dynamic rotation renderer so the body visually tilts with the physics
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

        // Visually render the Hand and Weapon exactly on the recoiled bouncing path and angle
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