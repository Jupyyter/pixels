// text/plain
// simpleentity.cpp
#include "SimpleEntity.hpp"
#include "EntitySystem.hpp"
#include "ParticleWorld.hpp"
#include "RigidBody.hpp"
#include "Constants.hpp"
#include <cmath>
#include <algorithm>

SimpleEntity::SimpleEntity(b2WorldId physWorld, EntitySystem* sys, float x, float y, const EntityDefinition& def, bool isPlayer)
    : Entity(physWorld, sys, x, y, def, isPlayer) 
{
    // SFML 3 accesses width and height through the 'size' vector
    this->colWidth = def.colliderRect.size.x;
    this->colHeight = def.colliderRect.size.y;
    
    maxStepUp = def.frameHeight / 4.0f; 

    float h = this->colHeight / 2.0f;
    float w = this->colWidth / 2.0f;

    for (float px = -w; px <= w; px += 1.0f) {
        boundarySamples.push_back({px, h});  // Bottom edge
        boundarySamples.push_back({px, -h}); // Top edge
    }
    for (float py = -h + 1.0f; py < h; py += 1.0f) {
        boundarySamples.push_back({-w, py}); // Left edge
        boundarySamples.push_back({w, py});  // Right edge
    }
}

SimpleEntity::~SimpleEntity() {}

void SimpleEntity::toggleRagdoll(bool enable) {
    pCtrl.isRagdoll = false;
}

void SimpleEntity::updateAnimations(float dt, ParticleWorld& pw) {
    if (!pCtrl.isRagdoll) {
        b2Vec2 b2Vel = b2Body_GetLinearVelocity(bodyId);
        b2Vec2 b2Pos = b2Body_GetPosition(bodyId);
        sf::Vector2f bodyPos(b2Pos.x * M2P, b2Pos.y * M2P);
        
        float groundCheckY = bodyPos.y + colHalfH - 1.0f;
        
        // 1. ORIGINAL center cast (Used to keep rotation stable!)
        float centerGroundY = system->groundCastY(bodyPos.x, groundCheckY, 24.0f, pw, false);
        float centerDistToGround = (centerGroundY >= 1e9f) ? 1000.0f : (centerGroundY - (bodyPos.y + colHalfH));

        // 2. NEW wide sweep cast (Used for Jumping and Animations!)
        float highestGroundY = centerGroundY;
        int startX = static_cast<int>(std::ceil(bodyPos.x - colHalfW));
        int endX   = static_cast<int>(std::floor(bodyPos.x + colHalfW));
        if (startX > endX) startX = endX = static_cast<int>(std::round(bodyPos.x));

        for (int px = startX; px <= endX; ++px) {
            float gY = system->groundCastY(static_cast<float>(px), groundCheckY, 24.0f, pw, false);
            if (gY < highestGroundY) highestGroundY = gY;
        }

        float sweepDistToGround = (highestGroundY >= 1e9f) ? 1000.0f : (highestGroundY - (bodyPos.y + colHalfH));

        bool wasGrounded = pCtrl.isGrounded;
        float airThreshold = (!wasGrounded && b2Vel.y > 0.0f) ? 2.0f : 8.0f;
        
        bool centerIsGrounded = (b2Vel.y >= -15.0f) && (centerDistToGround <= airThreshold);
        
        // As soon as the center leaves the edge, instantly force the entity to become airborne.
        // This removes ground friction (keeping full momentum for a smooth slide-off)
        // and stops you from being able to walk backwards back onto the platform!
        pCtrl.isGrounded = centerIsGrounded;

        if (!pCtrl.isGrounded) {
            if (b2Vel.y > 1.0f) sprite.currentState = "Fall";
            else sprite.currentState = "Jump";
        } else {
            if (std::abs(b2Vel.x) > 1.0f) sprite.currentState = "Walk";
            else sprite.currentState = "Idle";
        }

        if (centerIsGrounded && std::abs(b2Vel.x) > 0.1f) {
            float vx = b2Vel.x;
            float targetX = bodyPos.x + (vx * dt * M2P);
            
            float maxR = std::hypot(colWidth / 2.0f, colHeight / 2.0f);
            int minX = static_cast<int>(std::floor(targetX - maxR - 2.0f));
            int maxX = static_cast<int>(std::ceil(targetX + maxR + 2.0f));
            
            std::vector<float> groundYMap(maxX - minX + 1, 1e9f);
            
            // --- FIX FOR LOW ROOFS ---
            // Start scanning for the ground near the entity's feet instead of its head.
            // This prevents the raycast from hitting the roof, thinking it's the floor,
            // and freezing the entity because the "step up" is too large.
            int startY = static_cast<int>(std::floor(bodyPos.y + (colHeight / 2.0f) - maxStepUp - 6.0f));
            int endY = static_cast<int>(std::ceil(bodyPos.y + maxR + maxStepUp));
            
            for (int px = minX; px <= maxX; ++px) {
                for (int py = startY; py <= endY; ++py) {
                    if (system->isSolid(px, py, pw, false)) {
                        groundYMap[px - minX] = static_cast<float>(py);
                        break;
                    }
                }
            }
            
            float bestAngle = 0.0f;
            float max_cY = -1e9f; 
            
            int angleSteps = 10;
            float maxAngle = 30.0f * 3.14159265f / 180.0f; 
            
            for (int i = -angleSteps; i <= angleSteps; ++i) {
                float a = i * maxAngle / angleSteps;
                float cos_a = std::cos(a);
                float sin_a = std::sin(a);
                
                float min_cY_for_this_angle = 1e9f;
                bool hitGround = false;
                
                for (const auto& pt : boundarySamples) {
                    float rx = pt.x * cos_a - pt.y * sin_a;
                    float ry = pt.x * sin_a + pt.y * cos_a;
                    
                    int px = static_cast<int>(std::round(targetX + rx));
                    if (px >= minX && px <= maxX) {
                        float gY = groundYMap[px - minX];
                        if (gY < 1e8f) {
                            hitGround = true;
                            float allowed_cY = gY - ry;
                            if (allowed_cY < min_cY_for_this_angle) {
                                min_cY_for_this_angle = allowed_cY;
                            }
                        }
                    }
                }
                
                if (hitGround && min_cY_for_this_angle > max_cY) {
                    max_cY = min_cY_for_this_angle;
                    bestAngle = a;
                }
            }
            
            if (max_cY > -1e8f) {
                float stepUp = (bodyPos.y) - max_cY;
                
                if (stepUp > maxStepUp) {
                    b2Vel.x = 0.0f;
                    b2Body_SetLinearVelocity(bodyId, b2Vel);
                } else if (stepUp < -maxStepUp) {
                    // Let physics engine handle the fall.
                } else {
                    b2Rot rot = {std::cos(bestAngle), std::sin(bestAngle)};
                    b2Body_SetTransform(bodyId, {targetX * P2M, max_cY * P2M}, rot);
                    b2Body_SetLinearVelocity(bodyId, {b2Vel.x, 0.0f}); 
                }
            }
        }
    }

    Entity::updateAnimations(dt, pw);
}
void SimpleEntity::render(sf::RenderTarget& target) {
    b2Vec2 b2Pos = b2Body_GetPosition(bodyId);
    sf::Vector2f bodyPos(b2Pos.x * M2P, b2Pos.y * M2P);
    b2Rot rot = b2Body_GetRotation(bodyId);
    float bodyAng = std::atan2(rot.s, rot.c);
    
    sf::Vector2f vbp(bodyPos.x, bodyPos.y + bob.offsetY + armBaseY);
    system->drawPixelatedHand(target, vbp + handA.offset, sf::Color(180, 180, 180));
    
    renderSprite(target, bodyPos, bodyAng);
    renderArmsAndWeapon(target, bodyPos, bodyAng);
}

void SimpleEntity::save(std::ostream& out) const {
    Entity::save(out);
}

void SimpleEntity::load(std::istream& in) {
    Entity::load(in);
}