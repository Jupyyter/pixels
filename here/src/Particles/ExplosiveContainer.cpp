#include "Particles/ExplosiveContainer.hpp"
#include "ParticleWorld.hpp"
#include <cmath>
#include <algorithm>

// --- SPAWN LOGIC ---

void ExplosiveContainer::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);
    
    // Add Kinematics for projectile movement with forced constructor
    world.add<KinematicsComponent>(x, y, KinematicsComponent(
        sf::Vector2f(0.0f, 0.0f), // velocity
        0.0f, 0.0f,               // thresholds
        true,                     // isFreeFalling
        0                         // stoppedMovingCount
    ));

    // Default to Sand if no payload is specified
    // Note: containerPayloads still needs a global key. 
    // Since we're using infinite chunks, we need a unique global key. 
    // IMPORTANT: 'index' here is LOCAL to the chunk. Using it as a global key is dangerous!
    // We should combine (ChunkHash + LocalIndex) or use (x,y) as key.
    // For now, let's assume world.computeIndex(x,y) gives a LOCAL index and we map it badly.
    // FIX: Using a combined key or string map would be safer, but let's stick to the requested structure.
    // To make this work with chunks, we really should use (x,y) or a 64-bit key.
    // However, I will use the provided structure but warn that containerPayloads 
    // needs to map (ChunkID << 32 | LocalIndex) to work perfectly in infinite worlds.
    // For this specific file request, I will adhere to the existing logic but pass coords to getByIndex.

    if (world.containerPayloads.find(index) == world.containerPayloads.end()) {
        world.containerPayloads[index] = MaterialID::Sand;
    }
}

void ExplosiveContainer::spawnWithPayload(int x, int y, MaterialID payload, sf::Vector2f velocity, sf::Color color, bool ignited, ParticleWorld& world) {
    if (!world.inBounds(x, y)) return;

    world.spawnParticle(MaterialID::ExplosiveContainer, x, y);
    uint32_t index = world.computeIndex(x, y);

    if (auto* base = world.getByIndex<BaseComponent>(index, x, y)) {
        base->color = color;
        base->flags.isIgnited = ignited;
    }
    if (auto* kin = world.getByIndex<KinematicsComponent>(index, x, y)) {
        kin->velocity = velocity;
    }

    world.containerPayloads[index] = payload;
}

// --- UPDATE LOGIC ---

void ExplosiveContainer::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    // DIRECT MEMORY ACCESS
    if (!ctx.kinematics) return;
    auto* kin = &ctx.kinematics[ctx.index];

    kin->velocity.y += 9.81f * 20.0f * dt; 
    
    float maxVel = 500.0f;
    kin->velocity.y = std::clamp(kin->velocity.y, -maxVel, maxVel);

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
    float currX = static_cast<float>(ctx.x);
    float currY = static_cast<float>(ctx.y);

    int lastValidX = ctx.x;
    int lastValidY = ctx.y;
    int curX = ctx.x;
    int curY = ctx.y;

    for (int i = 0; i < steps; i++) {
        currX += stepX;
        currY += stepY;

        int nextX = static_cast<int>(std::round(currX));
        int nextY = static_cast<int>(std::round(currY));

        if (!world.inBounds(nextX, nextY)) {
            detonate(world.computeIndex(curX, curY), curX, curY, world);
            return;
        }

        if (nextX == curX && nextY == curY) continue;

        if (world.isEmpty(nextX, nextY)) {
            lastValidX = nextX;
            lastValidY = nextY;
        } else {
            // Revert back to `world.get` since the neighbor could be in an entirely different chunk
            auto* nb = world.get<BaseComponent>(nextX, nextY);
            
            Particle* logic = nb ? MaterialRegistry[static_cast<int>(nb->id)] : nullptr;
            MaterialGroup nGroup = logic ? logic->getGroup() : MaterialGroup::ImmovableSolid;

            if (nGroup == MaterialGroup::MovableSolid || nGroup == MaterialGroup::ImmovableSolid || nGroup == MaterialGroup::Liquid) {
                if (lastValidX != curX || lastValidY != curY) {
                    uint32_t currentIdx = world.computeIndex(curX, curY);
                    MaterialID payload = world.containerPayloads[currentIdx];
                    world.containerPayloads.erase(currentIdx);
                    
                    world.moveParticle(curX, curY, lastValidX, lastValidY);
                    
                    uint32_t newIdx = world.computeIndex(lastValidX, lastValidY);
                    world.containerPayloads[newIdx] = payload;
                    curX = lastValidX; curY = lastValidY;
                }
                detonate(world.computeIndex(curX, curY), curX, curY, world);
                return; 
            }
        }
    }

    if (lastValidX != ctx.x || lastValidY != ctx.y) {
        uint32_t currentIdx = world.computeIndex(ctx.x, ctx.y);
        MaterialID payload = world.containerPayloads[currentIdx];
        world.containerPayloads.erase(currentIdx);
        
        world.moveParticle(ctx.x, ctx.y, lastValidX, lastValidY);
        
        uint32_t newIdx = world.computeIndex(lastValidX, lastValidY);
        world.containerPayloads[newIdx] = payload;
    }
}
void ExplosiveContainer::detonate(uint32_t index, int x, int y, ParticleWorld& world) {
    MaterialID content = MaterialID::Sand;
    auto it = world.containerPayloads.find(index);
    if (it != world.containerPayloads.end()) {
        content = it->second;
        world.containerPayloads.erase(it);
    }
    
    sf::Color oldColor = sf::Color::White;
    bool oldIgnited = false;
    sf::Vector2f oldVel = {0,0};
    
    if (auto* b = world.getByIndex<BaseComponent>(index, x, y)) { 
        oldColor = b->color; 
        oldIgnited = b->flags.isIgnited; 
    }
    if (auto* k = world.getByIndex<KinematicsComponent>(index, x, y)) {
        oldVel = k->velocity;
    }

    world.removeParticle(x, y); 

    int spawnX = x;
    int spawnY = y;
    bool foundSpot = false;

    if (world.isEmpty(x, y)) {
        foundSpot = true;
    } else {
        // Search for nearest exit point to spawn the payload
        int yOffset = 0;
        for (int i = 0; i < 10; i++) {
            int checkY = y + yOffset;
            if (world.inBounds(x, checkY) && world.isEmpty(x, checkY)) {
                spawnY = checkY;
                foundSpot = true;
                break;
            }
            yOffset = (yOffset <= 0) ? -yOffset + 1 : -yOffset;
        }
    }

    if (foundSpot) {
        world.spawnParticle(content, spawnX, spawnY);
        uint32_t newIdx = world.computeIndex(spawnX, spawnY);
        
        if (auto* b = world.getByIndex<BaseComponent>(newIdx, spawnX, spawnY)) {
            b->color = oldColor;
            b->flags.isIgnited = oldIgnited;
        }
        if (auto* k = world.getByIndex<KinematicsComponent>(newIdx, spawnX, spawnY)) {
            k->velocity = oldVel * 0.3f; // Dampen velocity on impact
        }
    }
}

static ExplosiveContainer container_instance;