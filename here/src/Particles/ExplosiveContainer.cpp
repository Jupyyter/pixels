#include "Particles/ExplosiveContainer.hpp"
#include "ParticleWorld.hpp"
#include "Particles/ParticleDef.hpp"
#include <cmath>
#include <algorithm>

inline uint32_t getGlobalKey(int x, int y) {
    // Generate a unique key safely bridging chunks
    return (static_cast<uint32_t>(y) << 16) | static_cast<uint16_t>(x);
}

void ExplosiveContainer::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);
    
    world.add<KinematicsComponent>(x, y, KinematicsComponent(
        sf::Vector2f(0.0f, 0.0f), 0.0f, 0.0f, true, 0
    ));

    uint32_t gKey = getGlobalKey(x, y);
    if (world.containerPayloads.find(gKey) == world.containerPayloads.end()) {
        world.containerPayloads[gKey] = GetMatID("Sand");
    }
}

void ExplosiveContainer::spawnWithPayload(int x, int y, MaterialID payload, sf::Vector2f velocity, sf::Color color, bool ignited, ParticleWorld& world) {
    if (!world.inBounds(x, y)) return;

    world.spawnParticle(GetMatID("ExplosiveContainer"), x, y);
    uint32_t index = world.computeIndex(x, y);

    if (auto* base = world.getByIndex<BaseComponent>(index, x, y)) {
        base->color = color;
        base->flags.isIgnited = ignited;
    }
    if (auto* kin = world.getByIndex<KinematicsComponent>(index, x, y)) {
        kin->velocity = velocity;
    }

    world.containerPayloads[getGlobalKey(x, y)] = payload;
}

void ExplosiveContainer::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
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
            if (lastValidX != curX || lastValidY != curY) {
                uint32_t currentKey = getGlobalKey(curX, curY);
                MaterialID payload = world.containerPayloads[currentKey];
                world.containerPayloads.erase(currentKey);
                
                world.moveParticle(curX, curY, lastValidX, lastValidY);
                
                world.containerPayloads[getGlobalKey(lastValidX, lastValidY)] = payload;
                curX = lastValidX; curY = lastValidY;
            }
            detonate(curX, curY, world);
            return;
        }

        if (nextX == curX && nextY == curY) continue;

        if (world.isEmpty(nextX, nextY)) {
            lastValidX = nextX;
            lastValidY = nextY;
        } else {
            auto* nb = world.get<BaseComponent>(nextX, nextY);
            Particle* logic = nb ? MaterialRegistry[static_cast<int>(nb->id)] : nullptr;
            MaterialGroup nGroup = logic ? logic->getGroup() : MaterialGroup::ImmovableSolid;

            if (nGroup == MaterialGroup::MovableSolid || nGroup == MaterialGroup::ImmovableSolid || nGroup == MaterialGroup::Liquid) {
                if (lastValidX != curX || lastValidY != curY) {
                    uint32_t currentKey = getGlobalKey(curX, curY);
                    MaterialID payload = world.containerPayloads[currentKey];
                    world.containerPayloads.erase(currentKey);
                    
                    world.moveParticle(curX, curY, lastValidX, lastValidY);
                    
                    world.containerPayloads[getGlobalKey(lastValidX, lastValidY)] = payload;
                    curX = lastValidX; curY = lastValidY;
                }
                detonate(curX, curY, world);
                return; 
            }
        }
    }

    if (lastValidX != ctx.x || lastValidY != ctx.y) {
        uint32_t currentKey = getGlobalKey(ctx.x, ctx.y);
        MaterialID payload = world.containerPayloads[currentKey];
        world.containerPayloads.erase(currentKey);
        
        world.moveParticle(ctx.x, ctx.y, lastValidX, lastValidY);
        world.containerPayloads[getGlobalKey(lastValidX, lastValidY)] = payload;
    }
}

void ExplosiveContainer::detonate(int x, int y, ParticleWorld& world) {
    MaterialID content = GetMatID("Sand");
    uint32_t gKey = getGlobalKey(x, y);
    
    auto it = world.containerPayloads.find(gKey);
    if (it != world.containerPayloads.end()) {
        content = it->second;
        world.containerPayloads.erase(it);
    }
    
    sf::Color oldColor = sf::Color::White;
    bool oldIgnited = false;
    sf::Vector2f oldVel = {0,0};
    
    uint32_t index = world.computeIndex(x, y);
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
            k->velocity = oldVel * 0.3f;
        }
    }
}

// NOTE: We successfully removed the static 'container_instance' initialization that caused the crash!