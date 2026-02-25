#include "Particles/ExplosiveContainer.hpp"
#include "ParticleWorld.hpp"
#include "Particles/Particle.hpp" // For MaterialRegistry access
#include <cmath>
#include <algorithm>

// --- SPAWN LOGIC ---

void ExplosiveContainer::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);
    
    // Add Kinematics for projectile movement
    KinematicsComponent kin;
    kin.velocity = {0, 0}; 
    kin.xThreshold = 0;
    kin.yThreshold = 0;
    kin.isFreeFalling = true;
    kin.stoppedMovingCount = 0;
    world.kinematicsManager.add(index, kin);

    // Ensure we track payload (Default to Sand if not already set by spawnWithPayload)
    if (world.containerPayloads.find(index) == world.containerPayloads.end()) {
        world.containerPayloads[index] = MaterialID::Sand;
    }
}

// Static Helper to spawn fully configured container projectile
void ExplosiveContainer::spawnWithPayload(int x, int y, MaterialID payload, sf::Vector2f velocity, sf::Color color, bool ignited, ParticleWorld& world) {
    // 1. Spawn base particle
    world.spawnParticle(MaterialID::ExplosiveContainer, x, y);
    uint32_t index = world.getIndex(x, y);

    // 2. Configure State
    if (auto* base = world.baseManager.get(index)) {
        base->color = color;
        base->flags.isIgnited = ignited;
    }
    if (auto* kin = world.kinematicsManager.get(index)) {
        kin->velocity = velocity;
    }

    // 3. Store the unique payload in the world map
    world.containerPayloads[index] = payload;
}

// --- UPDATE LOGIC ---

void ExplosiveContainer::update(int x, int y, uint32_t index, float dt, ParticleWorld& world) {
    auto* kin = world.kinematicsManager.get(index);
    if (!kin) return;

    // Gravity
    kin->velocity.y += 9.81f * 20.0f * dt; 
    
    float maxVel = 500.0f;
    kin->velocity.y = std::clamp(kin->velocity.y, -maxVel, maxVel);

    // Movement Logic (Raycasting)
    float deltaX = kin->velocity.x * dt;
    float deltaY = kin->velocity.y * dt;

    int steps = static_cast<int>(std::max(std::abs(deltaX), std::abs(deltaY)));
    
    if (steps == 0) {
        kin->xThreshold += deltaX;
        kin->yThreshold += deltaY;
        if (std::abs(kin->xThreshold) >= 1.0f) {
            deltaX = kin->xThreshold; kin->xThreshold = 0; steps = 1;
        } else if (std::abs(kin->yThreshold) >= 1.0f) {
            deltaY = kin->yThreshold; kin->yThreshold = 0; steps = 1;
        } else {
            return; 
        }
    }

    float stepX = deltaX / steps;
    float stepY = deltaY / steps;
    float currX = static_cast<float>(x);
    float currY = static_cast<float>(y);

    int lastValidX = x;
    int lastValidY = y;
    bool collided = false;

    for (int i = 0; i < steps; i++) {
        currX += stepX;
        currY += stepY;

        int nextX = static_cast<int>(std::round(currX));
        int nextY = static_cast<int>(std::round(currY));

        if (!world.inBounds(nextX, nextY)) {
    // Detonate at the last valid position before leaving the screen
    detonate(index, lastValidX, lastValidY, world);
    return;
}

        if (nextX == x && nextY == y) continue;

        if (world.isEmpty(nextX, nextY)) {
            lastValidX = nextX;
            lastValidY = nextY;
        } else {
            // Collision with Solid or Liquid
            uint32_t neighborIdx = world.getIndex(nextX, nextY);
            BaseComponent* nb = world.baseManager.get(neighborIdx);
            
            // Look up logic for group check
            Particle* logic = MaterialRegistry[static_cast<int>(nb->id)];
            MaterialGroup nGroup = logic ? logic->getGroup() : MaterialGroup::ImmovableSolid;

            if (nGroup == MaterialGroup::MovableSolid || nGroup == MaterialGroup::ImmovableSolid || nGroup == MaterialGroup::Liquid) {
                collided = true;
                if (lastValidX != x || lastValidY != y) {
                    world.moveParticle(x, y, lastValidX, lastValidY);
                }
                // Recalculate index after potential move
                uint32_t finalIdx = world.getIndex(lastValidX, lastValidY);
                detonate(finalIdx, lastValidX, lastValidY, world);
                return; 
            }
        }
    }

    if (!collided && (lastValidX != x || lastValidY != y)) {
        world.moveParticle(x, y, lastValidX, lastValidY);
    }
}

void ExplosiveContainer::detonate(uint32_t index, int x, int y, ParticleWorld& world) {
    // 1. Retrieve and CACHE data BEFORE deleting the container
    MaterialID content = MaterialID::Sand;
    auto it = world.containerPayloads.find(index);
    if (it != world.containerPayloads.end()) {
        content = it->second;
    }
    
    sf::Color oldColor = sf::Color::White;
    bool oldIgnited = false;
    sf::Vector2f oldVel = {0,0};
    
    if (auto* b = world.baseManager.get(index)) { 
        oldColor = b->color; 
        oldIgnited = b->flags.isIgnited; 
    }
    if (auto* k = world.kinematicsManager.get(index)) {
        oldVel = k->velocity;
    }

    // 2. Remove Container from world and Payload map
    world.containerPayloads.erase(index);
    world.removeParticle(index); 

    // 3. Find a spot to spawn the content (Payload)
    int spawnX = x;
    int spawnY = y;
    bool foundSpot = false;

    if (world.isEmpty(x, y)) {
        foundSpot = true;
    } else {
        // Spiral/Y search for empty space
        int yOffset = 0;
        for (int i = 0; i < 20; i++) {
            int checkY = y + yOffset;
            if (world.inBounds(x, checkY) && world.isEmpty(x, checkY)) {
                spawnY = checkY;
                foundSpot = true;
                break;
            }
            yOffset = (yOffset <= 0) ? -yOffset + 1 : -yOffset;
        }
    }

    // 4. Spawn the content and restore the state
    if (foundSpot) {
        world.spawnParticle(content, spawnX, spawnY);
        uint32_t newIdx = world.getIndex(spawnX, spawnY);
        
        if (auto* b = world.baseManager.get(newIdx)) {
            b->color = oldColor;
            b->flags.isIgnited = oldIgnited;
        }
        if (auto* k = world.kinematicsManager.get(newIdx)) {
            // Apply impact velocity reduction
            k->velocity = oldVel * 0.3f;
        }
    }
}

// Register the class instance for logic handling
static ExplosiveContainer container_instance;