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
    bdef.angularDamping = 100000.0f;

    b2BodyId bodyId = b2CreateBody(physicsWorldId, &bdef);
    b2Polygon  box      = b2MakeBox(2.5f * P2M, 8.0f * P2M); 
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density           = 1.0f;
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
        
        // Aiming and Shooting Logic
        bool rightClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
        bool leftClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);

        if (rightClick && player.equippedWeapon) {
            player.isAiming = true;
            player.aimTarget = mouseWorldPos;
            
            if (player.equippedWeapon->isGun) {
                Weapon* w = static_cast<Weapon*>(player.equippedWeapon);
                
                if (player.fireTimer <= 0.0f) {
                    // Check if they are allowed to shoot based on Semi-Auto rules
                    bool canShoot = w->semiAuto ? (leftClick && !player.leftClickPressedLastFrame) : leftClick;
                    
                    if (canShoot) {
                        // Reconstruct rendering hand position
                        sf::Vector2f vbp(bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY);
                        sf::Vector2f handPos = vbp + anim.handB.offset;
                        
                        w->fire(handPos, player.aimTarget, anim.weaponAngle, sprite.flipX, rbs, pw);
                        player.fireTimer = w->fireRate;
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
            float baseCastFrom = bodyPos.y - TARGET_CAST_UP;
            
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

        // Single press jump (Locked if holding, or recovering)
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
            if (base) {
                Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                if (logic) {
                    MaterialGroup group = logic->getGroup();
                    // Ignore liquids and gas completely for ground detection
                    if (group == MaterialGroup::Liquid || group == MaterialGroup::Gas) {
                        continue;
                    }
                }
            }
            return static_cast<float>(py - 1);
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

        auto castForTarget = [&](float worldX) -> float {
            float castFrom = bodyPos.y - TARGET_CAST_UP;
            return groundCastY(worldX, castFrom, TARGET_CAST_UP + TARGET_CAST_DOWN, pw);
        };
        
        auto getSafeTarget = [&](float lookOffset) -> sf::Vector2f {
            float dir = lookOffset > 0 ? 1.0f : (lookOffset < 0 ? -1.0f : 0.0f);
            float maxDist = std::abs(lookOffset);
            float bestX = bodyPos.x;
            float baseCastFrom = bodyPos.y - TARGET_CAST_UP;
            
            float bestY = groundCastY(bodyPos.x, baseCastFrom, TARGET_CAST_UP + TARGET_CAST_DOWN, pw);
            if (bestY >= NO_GROUND) bestY = bodyPos.y + 17.0f;

            if (maxDist < 0.1f) return {bestX, bestY};

            float minFootY = bodyPos.y + 12.0f;
            float maxFootY = bodyPos.y + 22.0f; 

            for (int i = 1; i <= static_cast<int>(std::ceil(maxDist)); ++i) {
                float testX = bodyPos.x + dir * i;
                float ty = groundCastY(testX, baseCastFrom, TARGET_CAST_UP + TARGET_CAST_DOWN, pw);
                
                if (ty < minFootY) break;
                if (ty > maxFootY) break;
                
                bestX = testX;
                bestY = ty;
            }
            return {bestX, bestY};
        };

        float centerGroundY = castForTarget(bodyPos.x);
        float distToGround = (centerGroundY >= NO_GROUND) ? 1000.0f : (centerGroundY - bodyPos.y);

        bool wasGrounded = player.isGrounded;
        float airThreshold = (!wasGrounded && b2Vel.y > 0.0f) ? 18.0f : 28.0f;
        bool isAirborne  = (b2Vel.y < -5.0f) || (distToGround > airThreshold);

        if (isAirborne && b2Vel.y > 0.0f) {
            player.lastFallVelocity = b2Vel.y;
        }

        player.isGrounded = !isAirborne;

        if (!wasGrounded && player.isGrounded) {
            if (player.lastFallVelocity > 25.0f) {
                player.landingTimer = std::min(0.5f, (player.lastFallVelocity - 25.0f) * 0.015f);
            } else {
                player.landingTimer = 0.0f;
            }
            player.lastFallVelocity = 0.0f;
        }

        bool wantsToWalk = std::abs(b2Vel.x) > 0.1f && !isAirborne && (player.landingTimer <= 0.0f);

        auto castNearFoot = [&](float worldX, float footY) -> float {
            float castFrom = footY - FOOT_CAST_UP;
            return groundCastY(worldX, castFrom, FOOT_CAST_UP + FOOT_CAST_DOWN, pw);
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

        // ---- Body-bob spring ----
        {
            float f = -anim.bob.stiffness * anim.bob.offsetY - anim.bob.damping * anim.bob.velocity;
            anim.bob.velocity += f * dt;
            anim.bob.offsetY  += anim.bob.velocity * dt;
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

            sf::Vector2f vbp(bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY);
            
            sf::Vector2f frontDir(std::sin(frontAngle), std::cos(frontAngle));
            sf::Vector2f backDir(std::sin(backAngle),   std::cos(backAngle));

            frontLeg->footWorld = vbp + frontLeg->hipOffset + frontDir * frontLen;
            backLeg->footWorld  = vbp + backLeg->hipOffset  + backDir  * backLen;

            auto preventGroundPenetration = [&](ProceduralLeg* leg) {
                float gY = castNearFoot(leg->footWorld.x, leg->footWorld.y);
                if (gY < NO_GROUND && leg->footWorld.y > gY) {
                    leg->footWorld.y = gY; 
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
                if (sy < NO_GROUND) leg.footWorld.y = sy;
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
                    stepLeg->footWorld.y = landedY;
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
                    if (midY < NO_GROUND && stepLeg->footWorld.y > midY)
                        stepLeg->footWorld.y = midY;
                }
            }
        }

        update_hips:
        {
            sf::Vector2f vbp(bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY);
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

            sf::Vector2f vbp(bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY);
            bool facingLeft = spriteComp.flipX;
            float swingVal  = std::sin(anim.armSwing);

            if (player.isSwinging) {
                player.swingTimer += dt;
                float t = clamp01(player.swingTimer / player.swingDuration);

                // Auto-face the swing direction
                spriteComp.flipX = (player.swingTarget.x < bodyPos.x);

                if (t >= 0.5f && !player.swingEffectApplied) {
                    player.swingEffectApplied = true;
                    if (player.equippedWeapon && player.equippedWeapon->isWeapon) {
                        Weapon* w = static_cast<Weapon*>(player.equippedWeapon);
                        w->performSwingEffect(bodyPos, player.swingTarget, pw, *pw.getRigidBodySystem());
                    }
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
                // Aiming offsets the holding hand from the shoulder, extended towards the mouse cursor
                sf::Vector2f shoulderPos = vbp + sf::Vector2f(0.0f, -4.0f); 
                sf::Vector2f aimDir = player.aimTarget - shoulderPos;
                
                // Auto face aim direction
                spriteComp.flipX = (aimDir.x < 0.0f);
                facingLeft = spriteComp.flipX;
                
                float dist = std::max(length(aimDir), 1.0f);
                sf::Vector2f aimNorm = {aimDir.x / dist, aimDir.y / dist};
                
                float desiredWeaponAngle = 0.0f;
                if (!facingLeft) {
                    desiredWeaponAngle = std::atan2(aimDir.y, aimDir.x) * 180.0f / PI;
                } else {
                    // Left-facing rendering mirrors the sprite, so we mirror the X vector and negate the angle
                    desiredWeaponAngle = -std::atan2(aimDir.y, -aimDir.x) * 180.0f / PI;
                }
                
                // Extension orbit distance for the weapon hand
                float armExtension = 9.0f; 
                
                // Desired hand B offset relative to the vbp
                sf::Vector2f desiredHandBOffset = (shoulderPos - vbp) + aimNorm * armExtension;

                // CONSTRAINT: Ensure the aiming hand doesn't clip below the natural waist/chest line.
                // 0.0f is exactly the resting height when idle. Positive Y goes down towards the legs.
                if (desiredHandBOffset.y > 0.0f) {
                    desiredHandBOffset.y = 0.0f;
                }

                anim.handB.offset.x = lerp(anim.handB.offset.x, desiredHandBOffset.x, dt * 15.0f);
                anim.handB.offset.y = lerp(anim.handB.offset.y, desiredHandBOffset.y, dt * 15.0f);
                
                float angleDiff = desiredWeaponAngle - anim.weaponAngle;
                while (angleDiff > 180.0f) angleDiff -= 360.0f;
                while (angleDiff < -180.0f) angleDiff += 360.0f;
                anim.weaponAngle += angleDiff * dt * 25.0f; 

                // Hand A (Unused hand) swings normally while moving, or rests naturally
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
                    
                    // Unified math works perfectly for both Melee and Guns due to VisualOffsets!
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

                    // Unified math tracks the bounce parabola identically left and right!
                    float desiredWeaponAngle = facingLeft ? (-90.0f - angleDeg) : (90.0f + angleDeg);
                    
                    float angleDiff = desiredWeaponAngle - anim.weaponAngle;
                    while (angleDiff > 180.0f) angleDiff -= 360.0f;
                    while (angleDiff < -180.0f) angleDiff += 360.0f;
                    anim.weaponAngle += angleDiff * dt * 20.0f;
                }
            }
        }

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
            
            float bodyError    = desiredBodyY - bodyPos.y;
            float desiredVy_P  = std::max(-80.0f, std::min(80.0f, bodyError * 18.0f)); 
            
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
        sf::Vector2f vbp(bodyPos.x, bodyPos.y + 8.0f + anim.bob.offsetY);

        drawPixelatedHand(target, vbp + anim.handA.offset, sf::Color(180, 180, 180));

        drawPixelatedLeg(target, vbp + anim.legA.hipOffset, anim.legA.footWorld, sf::Color::White);
        drawPixelatedLeg(target, vbp + anim.legB.hipOffset, anim.legB.footWorld, sf::Color::White);

        if (spriteComp.sprite.has_value()) {
            spriteComp.sprite->setPosition(vbp);
            spriteComp.sprite->setRotation(sf::degrees(0.0f)); 
            target.draw(*spriteComp.sprite);
        }

        drawPixelatedHand(target, vbp + anim.handB.offset, sf::Color::White);

        if (player.equippedWeapon) {
            player.equippedWeapon->renderPixelated(target, vbp + anim.handB.offset, anim.weaponAngle, spriteComp.flipX);
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