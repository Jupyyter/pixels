// text/plain
// complexentity.cpp
#include "Entities/ComplexEntity.hpp"
#include "Entities/EntitySystem.hpp"
#include "ParticleWorld.hpp"
#include "RigidBody.hpp"
#include "Constants.hpp"
#include <cmath>
#include <algorithm>

constexpr float PI = 3.14159265358979323846f;
constexpr float NO_GROUND = 1e9f;

inline float clamp01(float t)       { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }
inline float lerp(float a, float b, float t) { t = clamp01(t); return a + t * (b - a); }

ComplexEntity::ComplexEntity(b2WorldId physWorld, EntitySystem* sys, float x, float y, const EntityDefinition& def, bool isPlayer)
    : Entity(physWorld, sys, x, y, def, isPlayer) 
{
    legA.footWorld  = {x - 3.0f, y + hipBaseY + 9.0f};
    legB.footWorld  = {x + 3.0f, y + hipBaseY + 9.0f};
    legA.footStart  = legA.footTarget = legA.footWorld;
    legB.footStart  = legB.footTarget = legB.footWorld;
    legA.plantedX   = legA.footWorld.x;
    legA.plantedY   = legA.footWorld.y;
    legA.isPlanted  = true;
    legB.plantedX   = legB.footWorld.x;
    legB.plantedY   = legB.footWorld.y;
    legB.isPlanted  = true;
}

ComplexEntity::~ComplexEntity() {
    if (b2Body_IsValid(legA.ragdollBodyId)) b2DestroyBody(legA.ragdollBodyId);
    if (b2Body_IsValid(legB.ragdollBodyId)) b2DestroyBody(legB.ragdollBodyId);
}

void ComplexEntity::updateAnimations(float dt, ParticleWorld& pw) {
    if (pCtrl.isRagdoll) {
        auto applySandToPart = [&](b2BodyId bId, sf::Vector2f halfSize) {
            if (!b2Body_IsValid(bId)) return;
            b2Vec2 p = b2Body_GetPosition(bId);
            b2Rot r = b2Body_GetRotation(bId);
            float angle = std::atan2(r.s, r.c);
            sf::Vector2f center(p.x * M2P, p.y * M2P);
            
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
                                if (!moved && std::rand() % 100 < 10) pw.removeParticle(px, py);
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

        applySandToPart(legA.ragdollBodyId, {1.5f, 4.5f});
        applySandToPart(legB.ragdollBodyId, {1.5f, 4.5f});

        b2Vec2 p = b2Body_GetPosition(bodyId);
        sf::Vector2f vbp(p.x * M2P, p.y * M2P + hipBaseY + bob.offsetY);
        
        auto syncLeg = [&](ProceduralLeg& leg, b2BodyId bId) {
            if (b2Body_IsValid(bId)) {
                b2Vec2 bp = b2Body_GetPosition(bId);
                b2Rot r = b2Body_GetRotation(bId);
                float a = std::atan2(r.s, r.c);
                sf::Vector2f center(bp.x * M2P, bp.y * M2P);
                float dirX = -std::sin(a);
                float dirY = std::cos(a);
                leg.footWorld = {center.x + dirX * 4.5f, center.y + dirY * 4.5f};
                sf::Vector2f hip = {center.x - dirX * 4.5f, center.y - dirY * 4.5f};
                leg.hipOffset = {hip.x - vbp.x, hip.y - vbp.y};
            }
        };
        syncLeg(legA, legA.ragdollBodyId);
        syncLeg(legB, legB.ragdollBodyId);

        Entity::updateAnimations(dt, pw);
        return;
    }

    b2Vec2 b2Pos = b2Body_GetPosition(bodyId);
    sf::Vector2f bodyPos(b2Pos.x * M2P, b2Pos.y * M2P);
    b2Vec2 b2Vel = b2Body_GetLinearVelocity(bodyId);

    bool ignorePlatformsSuspension = (pCtrl.uprightStunTimer > 0.0f) || (!pCtrl.isGrounded && b2Vel.y < -5.0f);
    sf::Vector2f vbp(bodyPos.x, bodyPos.y + hipBaseY + bob.offsetY);
    float hipY = vbp.y;
    float minAllowedFootY = vbp.y + 4.0f; 

    auto customGroundCastY = [&](float worldX, float castFromY, float maxDown) -> float {
        int px = static_cast<int>(std::round(worldX));
        int startY = static_cast<int>(std::floor(castFromY));
        for (int i = 0; i <= static_cast<int>(maxDown); ++i) {
            int py = startY + i;
            if (!pw.isEmpty(px, py)) {
                BaseComponent* base = pw.get<BaseComponent>(px, py);
                if (base && base->compMask != 0) {
                    if (base->compMask & COMP_PLATFORM) {
                        if (ignorePlatformsSuspension) continue;
                        if (py < bodyPos.y + colHalfH - 1.0f) continue;
                    }
                    Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                    if (logic && logic->getGroup() != MaterialGroup::Liquid && logic->getGroup() != MaterialGroup::Gas) {
                        return static_cast<float>(py - 1);
                    }
                }
            }
        }
        return NO_GROUND;
    };

    auto castForTarget = [&](float worldX) -> float { return customGroundCastY(worldX, hipY, 24.0f); };
    
    auto getSafeTarget = [&](float lookOffset) -> sf::Vector2f {
        float castDir = lookOffset > 0 ? 1.0f : (lookOffset < 0 ? -1.0f : 0.0f);
        float maxDist = std::abs(lookOffset);
        float bestX = bodyPos.x;
        
        float bestY = customGroundCastY(bodyPos.x, hipY, 24.0f);
        if (bestY >= NO_GROUND) bestY = bodyPos.y + colHalfH + 9.0f;
        if (maxDist < 0.1f) return {bestX, std::max(bestY, minAllowedFootY)};
        float minFootY = bodyPos.y + colHalfH + 4.0f;
        float maxFootY = bodyPos.y + colHalfH + 14.0f; 
        for (int i = 1; i <= static_cast<int>(std::ceil(maxDist)); ++i) {
            float testX = bodyPos.x + castDir * i;
            float ty = customGroundCastY(testX, hipY, 24.0f);
            if (ty < minFootY) break;
            if (ty > maxFootY) break;
            bestX = testX;
            bestY = ty;
        }
        return {bestX, std::max(bestY, minAllowedFootY)};
    };

    float centerGroundY = castForTarget(bodyPos.x);
    float distToGround = (centerGroundY >= NO_GROUND) ? 1000.0f : (centerGroundY - bodyPos.y);

    bool wasGrounded = pCtrl.isGrounded;
    float airThreshold = (!wasGrounded && b2Vel.y > 0.0f) ? 18.0f : 28.0f;
    bool isAirborne  = (b2Vel.y < -15.0f) || (distToGround > airThreshold);

    if (isAirborne && b2Vel.y > 0.0f) pCtrl.lastFallVelocity = b2Vel.y;
    pCtrl.isGrounded = !isAirborne;

    if (!wasGrounded && pCtrl.isGrounded) {
        if (pCtrl.lastFallVelocity > 25.0f) pCtrl.landingTimer = std::min(0.55f, (pCtrl.lastFallVelocity - 25.0f) * 0.030f);
        else pCtrl.landingTimer = 0.0f;
        pCtrl.lastFallVelocity = 0.0f;
    }

    bool wantsToWalk = std::abs(b2Vel.x) > 0.1f && !isAirborne && (pCtrl.landingTimer <= 0.0f);

    auto castNearFoot = [&](float worldX, float fy) -> float { return customGroundCastY(worldX, hipY, 24.0f); };

    auto isWallAhead = [&](float dX) -> bool {
        if (std::abs(dX) < 0.01f) return false;
        int dirSign = dX > 0 ? 1 : -1;
        int startX = static_cast<int>(std::round(bodyPos.x + dirSign * 4.0f));
        int endX   = static_cast<int>(std::round(bodyPos.x + dirSign * 7.0f));
        int pyAnkle = static_cast<int>(std::round(bodyPos.y + colHalfH + 4.0f));
        int pyKnee  = static_cast<int>(std::round(bodyPos.y + colHalfH - 2.0f));

        auto checkSolid = [&](int px, int py) {
            if (pw.isEmpty(px, py)) return false;
            BaseComponent* base = pw.get<BaseComponent>(px, py);
            if (base) {
                if (base->compMask & COMP_PLATFORM) return false;
                Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                if (logic && logic->getGroup() != MaterialGroup::Liquid && logic->getGroup() != MaterialGroup::Gas) return true;
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
        if (sprite.currentState != "Idle") {
            sprite.currentState = "Idle";
            sprite.currentFrameIndex = 0;
        }
    }

    if (!pCtrl.isGrounded) sprite.currentState = "Jump";
    else if (pCtrl.landingTimer > 0.0f) sprite.currentState = "Idle";
    else if (std::abs(b2Vel.x) > 1.0f) sprite.currentState = "Walk";
    else sprite.currentState = "Idle";

    if (isAirborne) {
        steppingLeg = -1;
        isStopping  = false;
        legA.isPlanted = false;
        legB.isPlanted = false;

        legA.hipOffset = {-2.0f, 0.0f}; 
        legB.hipOffset = { 2.0f, 0.0f}; 

        bool facingLeft = sprite.flipX;
        ProceduralLeg* frontLeg = facingLeft ? &legA : &legB;
        ProceduralLeg* backLeg  = facingLeft ? &legB : &legA;

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

        float mappedVy = std::max(-15.0f, std::min(15.0f, vy));
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

        legA.footTarget = legA.footWorld;
        legB.footTarget = legB.footWorld;
        goto end_hips;
    }

    {
        bool  aIsLeft  = legA.footWorld.x < legB.footWorld.x;
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
            isStopping = false;

            if (steppingLeg == -1) {
                bool movingRight = b2Vel.x > 0.0f;
                steppingLeg = movingRight ? (aIsLeft ? 0 : 1) : (aIsLeft ? 1 : 0);
                stepProgress = 0.0f;

                ProceduralLeg* sl = (steppingLeg == 0) ? &legA : &legB;
                ProceduralLeg* pl = (steppingLeg == 0) ? &legB : &legA;

                sl->footStart = sl->footWorld;
                sl->isPlanted = false;

                float look = movingRight ? stepLookahead : -stepLookahead;
                sl->footTarget = getSafeTarget(look);

                if (!pl->isPlanted) plantLeg(*pl);
            }

            float speedX    = std::abs(b2Vel.x * M2P);
            float speedRate = (speedX * dt) / strideDistance;
            bool keyHeld = std::abs(b2Vel.x) > 0.5f; 
            float minRate = keyHeld ? (minStepRate * 2.5f * dt) : (minStepRate * dt);

            stepProgress += std::max(speedRate, minRate);

        } else { 
            if (steppingLeg != -1) {
                isStopping   = true;
                stepProgress += dt * 4.5f;
            } else {
                float distA = std::abs(legA.footWorld.x - targetXA);
                float distB = std::abs(legB.footWorld.x - targetXB);

                if (distA > 3.0f || distB > 3.0f) {
                    isStopping   = true;
                    steppingLeg  = (distA >= distB) ? 0 : 1;
                    stepProgress = 0.0f;

                    ProceduralLeg* sl = (steppingLeg == 0) ? &legA : &legB;
                    ProceduralLeg* pl = (steppingLeg == 0) ? &legB : &legA;

                    sl->footStart  = sl->footWorld;
                    sl->isPlanted  = false;
                    float tx = (steppingLeg == 0) ? targetXA : targetXB;
                    sl->footTarget = getSafeTarget(tx - bodyPos.x);

                    if (!pl->isPlanted) plantLeg(*pl);
                } else {
                    auto snapAndLock = [&](ProceduralLeg& leg) {
                        if (leg.isPlanted) enforcePlant(leg);
                        else               plantLeg(leg);
                    };
                    snapAndLock(legA);
                    snapAndLock(legB);
                    goto update_hips;
                }
            }
        }

        {
            ProceduralLeg* stepLeg = (steppingLeg == 0) ? &legA : &legB;
            ProceduralLeg* plant   = (steppingLeg == 0) ? &legB : &legA;

            if (plant->isPlanted) enforcePlant(*plant);

            float idealLook;
            if (isStopping) {
                float tx = (steppingLeg == 0) ? targetXA : targetXB;
                idealLook = tx - bodyPos.x;
            } else {
                idealLook = b2Vel.x > 0 ? stepLookahead : -stepLookahead;
            }
            
            sf::Vector2f safeIdeal = getSafeTarget(idealLook);
            stepLeg->footTarget.x = lerp(stepLeg->footTarget.x, safeIdeal.x, dt * 20.0f);
            stepLeg->footTarget.y = lerp(stepLeg->footTarget.y, safeIdeal.y, dt * 20.0f);

            float terrainDelta = std::abs(stepLeg->footTarget.y - stepLeg->footStart.y);
            float dynamicArc   = stepArcHeight + std::min(terrainDelta * 0.5f, 8.0f);

            if (stepProgress >= 1.0f) {
                float landedY = castNearFoot(stepLeg->footTarget.x, stepLeg->footTarget.y);
                if (landedY >= NO_GROUND) landedY = stepLeg->footTarget.y;

                stepLeg->footWorld.x = stepLeg->footTarget.x;
                stepLeg->footWorld.y = std::max(landedY, minAllowedFootY);
                stepLeg->plantedX    = stepLeg->footWorld.x;
                stepLeg->plantedY    = stepLeg->footWorld.y;
                stepLeg->isPlanted   = true;

                bob.velocity += 8.0f;

                if (isStopping) {
                    float otherX    = (steppingLeg == 0) ? targetXB : targetXA;
                    float otherDist = std::abs(plant->footWorld.x - otherX);

                    if (otherDist > 1.5f) {
                        steppingLeg  = (steppingLeg == 0) ? 1 : 0;
                        stepProgress = 0.0f;
                        ProceduralLeg* nl = (steppingLeg == 0) ? &legA : &legB;
                        ProceduralLeg* pl = (steppingLeg == 0) ? &legB : &legA;
                        nl->footStart  = nl->footWorld;
                        nl->isPlanted  = false;
                        float tx = (steppingLeg == 0) ? targetXA : targetXB;
                        nl->footTarget = getSafeTarget(tx - bodyPos.x);
                        if (!pl->isPlanted) plantLeg(*pl);
                    } else {
                        steppingLeg = -1;
                        isStopping  = false;
                        if (!plant->isPlanted) plantLeg(*plant);
                    }
                } else {
                    stepProgress -= 1.0f;
                    steppingLeg   = (steppingLeg == 0) ? 1 : 0;
                    ProceduralLeg* nl = (steppingLeg == 0) ? &legA : &legB;
                    nl->footStart  = nl->footWorld;
                    nl->isPlanted  = false;
                    float look2 = b2Vel.x > 0 ? stepLookahead : -stepLookahead;
                    nl->footTarget = getSafeTarget(look2);
                }
            } else {
                float u   = clamp01(stepProgress);
                float arc = 4.0f * u * (1.0f - u) * dynamicArc;

                stepLeg->footWorld.x = lerp(stepLeg->footStart.x, stepLeg->footTarget.x, u);
                stepLeg->footWorld.y = lerp(stepLeg->footStart.y, stepLeg->footTarget.y, u) - arc;

                float midY = castNearFoot(stepLeg->footWorld.x, stepLeg->footWorld.y);
                if (midY < NO_GROUND && stepLeg->footWorld.y > midY) {
                    stepLeg->footWorld.y = std::max(midY, minAllowedFootY);
                }
                if (stepLeg->footWorld.y < minAllowedFootY) stepLeg->footWorld.y = minAllowedFootY;
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
        updateHip(legA);
        updateHip(legB);
    }
    end_hips:

    if (!isAirborne) {
        float avgFootY     = (legA.footWorld.y + legB.footWorld.y) * 0.5f;
        float desiredBodyY = avgFootY - hipBaseY - 9.0f;
        
        float d_dirX = (b2Vel.x > 0.1f) ? 1.0f : ((b2Vel.x < -0.1f) ? -1.0f : 0.0f);
        float targetDownhill = 0.0f;
        if (d_dirX != 0.0f) {
            float gHere = castForTarget(bodyPos.x);
            float gAhead = castForTarget(bodyPos.x + d_dirX * stepLookahead);
            if (gHere < NO_GROUND && gAhead < NO_GROUND) {
                float diff = gAhead - gHere; 
                if (diff > 0.0f) targetDownhill = std::min(3.0f, diff * 0.8f);
            }
        }
        
        downhillOffset = lerp(downhillOffset, targetDownhill, dt * 10.0f);
        desiredBodyY += downhillOffset;
        
        float bodyError = desiredBodyY - bodyPos.y;
        float currentMinVy, currentMaxVy, vertStiffness;
        
        if (pCtrl.landingTimer > 0.0f) {
            currentMinVy  = -80.0f;
            currentMaxVy  = 80.0f;
            vertStiffness = 18.0f;
        } else {
            currentMinVy  = -250.0f; 
            currentMaxVy  = 150.0f;
            vertStiffness = 40.0f; 
        }
        
        float desiredVy_P = std::max(currentMinVy, std::min(currentMaxVy, bodyError * vertStiffness)); 
        b2Vec2 v = b2Body_GetLinearVelocity(bodyId);
        v.y = lerp(v.y, desiredVy_P * P2M, dt * 25.0f); 
        b2Body_SetLinearVelocity(bodyId, v);
    }

    Entity::updateAnimations(dt, pw);
}

void ComplexEntity::render(sf::RenderTarget& target) {
    b2Vec2 b2Pos = b2Body_GetPosition(bodyId);
    sf::Vector2f bodyPos(b2Pos.x * M2P, b2Pos.y * M2P);
    b2Rot rot = b2Body_GetRotation(bodyId);
    float bodyAng = std::atan2(rot.s, rot.c);
    sf::Vector2f vbp(bodyPos.x, bodyPos.y + hipBaseY + bob.offsetY);
    
    system->drawPixelatedHand(target, bodyPos + sf::Vector2f(0.0f, armBaseY + bob.offsetY) + handA.offset, sf::Color(180, 180, 180));
    system->drawPixelatedLeg(target, vbp + legA.hipOffset, legA.footWorld, sf::Color::White);
    system->drawPixelatedLeg(target, vbp + legB.hipOffset, legB.footWorld, sf::Color::White);

    renderSprite(target, bodyPos, bodyAng);
    renderArmsAndWeapon(target, bodyPos, bodyAng);
}

void ComplexEntity::toggleRagdoll(bool enable) {
    if (pCtrl.isRagdoll == enable) return; 
    Entity::toggleRagdoll(enable);
    if (enable) {
        b2Vec2 p = b2Body_GetPosition(bodyId);
        sf::Vector2f bodyPos(p.x * M2P, p.y * M2P);
        sf::Vector2f vbp = {bodyPos.x, bodyPos.y + hipBaseY + bob.offsetY};
        
        auto initLeg = [&](ProceduralLeg& leg, float density) {
            sf::Vector2f hip = vbp + leg.hipOffset;
            sf::Vector2f foot = leg.footWorld;
            sf::Vector2f dir = foot - hip;
            float angle = std::atan2(dir.y, dir.x) - PI/2.0f;
            sf::Vector2f center = hip + dir * 0.5f;
            leg.ragdollBodyId = createRagdollPart(1.5f, 4.5f, center, angle, density);
            createRevoluteJoint(bodyId, leg.ragdollBodyId, {hip.x, hip.y});
        };
        initLeg(legA, 1.0f);
        initLeg(legB, 1.0f);
    } else {
        if (b2Body_IsValid(legA.ragdollBodyId)) b2DestroyBody(legA.ragdollBodyId);
        if (b2Body_IsValid(legB.ragdollBodyId)) b2DestroyBody(legB.ragdollBodyId);
        legA.ragdollBodyId = b2_nullBodyId; legB.ragdollBodyId = b2_nullBodyId;
    }
}

bool ComplexEntity::checkSideSnag(float dir, b2BodyId sideBody) {
    auto checkLeg = [&](const ProceduralLeg& leg) -> bool {
        float ox = leg.footWorld.x + (dir > 0 ? 0.5f : -0.5f);
        b2Vec2 origin = {ox * P2M, (leg.footWorld.y - 2.0f) * P2M}; 
        b2Vec2 trans = {dir * 2.0f * P2M, 0.0f}; 
        b2QueryFilter filter = b2DefaultQueryFilter();
        filter.categoryBits = 0x0008;
        filter.maskBits = 0x0001 | 0x0002;
        b2RayResult hit = b2World_CastRayClosest(physicsWorldId, origin, trans, filter);
        if (hit.hit) {
            b2BodyId hitBody = b2Shape_GetBody(hit.shapeId);
            return (hitBody.index1 == sideBody.index1 && hitBody.generation == sideBody.generation);
        }
        return false;
    };
    return checkLeg(legA) || checkLeg(legB);
}

void ComplexEntity::save(std::ostream& out) const {
    Entity::save(out);
    auto saveLeg = [&](const ProceduralLeg& leg) {
        out.write((const char*)&leg.hipOffset, sizeof(sf::Vector2f));
        out.write((const char*)&leg.footWorld, sizeof(sf::Vector2f));
        out.write((const char*)&leg.footStart, sizeof(sf::Vector2f));
        out.write((const char*)&leg.footTarget, sizeof(sf::Vector2f));
        out.write((const char*)&leg.plantedX, sizeof(float));
        out.write((const char*)&leg.plantedY, sizeof(float));
        out.write((const char*)&leg.isPlanted, sizeof(bool));
    };
    saveLeg(legA);
    saveLeg(legB);
    out.write((const char*)&steppingLeg, sizeof(int));
    out.write((const char*)&stepProgress, sizeof(float));
    out.write((const char*)&isStopping, sizeof(bool));
    out.write((const char*)&downhillOffset, sizeof(float));
}

void ComplexEntity::load(std::istream& in) {
    Entity::load(in);
    auto readLeg = [&](ProceduralLeg& leg) {
        in.read((char*)&leg.hipOffset, sizeof(sf::Vector2f));
        in.read((char*)&leg.footWorld, sizeof(sf::Vector2f));
        in.read((char*)&leg.footStart, sizeof(sf::Vector2f));
        in.read((char*)&leg.footTarget, sizeof(sf::Vector2f));
        in.read((char*)&leg.plantedX, sizeof(float));
        in.read((char*)&leg.plantedY, sizeof(float));
        in.read((char*)&leg.isPlanted, sizeof(bool));
        leg.ragdollBodyId = b2_nullBodyId; 
    };
    readLeg(legA);
    readLeg(legB);
    in.read((char*)&steppingLeg, sizeof(int));
    in.read((char*)&stepProgress, sizeof(float));
    in.read((char*)&isStopping, sizeof(bool));
    in.read((char*)&downhillOffset, sizeof(float));
}