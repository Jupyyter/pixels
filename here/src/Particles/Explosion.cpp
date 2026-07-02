#include "Particles/Explosion.hpp"
#include "ParticleWorld.hpp"
#include "Particles/ExplosiveContainer.hpp"
#include "Constants.hpp"
#include "Particles/Particle.hpp" 
#include "Particles/ParticleDef.hpp" 
#include <cmath>
#include <algorithm>

Explosion::Explosion(ParticleWorld &worldRef, int x, int y, int r, int s)
    : world(worldRef), centerX(x), centerY(y), radius(r), strength(s) {}

void Explosion::enact()
{
    if (radius <= 0) return;

    int maxReach = static_cast<int>(std::ceil(radius * 1.5f));
    int boxSize = (maxReach * 2) + 1;
    std::vector<uint8_t> cache(boxSize * boxSize, (uint8_t)CacheState::UNVISITED);

    int rayCount = static_cast<int>(radius * 2.0f * 3.14159f * 1.5f); 
    if (rayCount < 16) rayCount = 16;

    float seed1 = Random::randFloat(0, 100);
    float seed2 = Random::randFloat(0, 100);

    for (int i = 0; i < rayCount; i++)
    {
        float angle = (static_cast<float>(i) / rayCount) * 2.0f * 3.14159f;

        float noise = std::sin(angle * 3.0f + seed1) * 0.15f;
        noise += std::sin(angle * 7.0f + seed2) * 0.10f;
        noise += Random::randFloat(-0.05f, 0.05f); 

        float finalRadiusMultiplier = 1.0f + noise;

        int destX = centerX + static_cast<int>(std::cos(angle) * radius * finalRadiusMultiplier);
        int destY = centerY + static_cast<int>(std::sin(angle) * radius * finalRadiusMultiplier);

        castRay(destX, destY, cache, boxSize, maxReach);
    }
    world.notifyTerrainChanged(static_cast<float>(centerX), static_cast<float>(centerY), static_cast<float>(maxReach) + 15.0f);
}

void Explosion::castRay(int destX, int destY, std::vector<uint8_t> &cache, int boxSize, int maxReach)
{
    int x1 = centerX, y1 = centerY;
    int dx = destX - x1, dy = destY - y1;

    int steps = std::max(std::abs(dx), std::abs(dy));
    if (steps == 0) return;

    float xInc = dx / (float)steps;
    float yInc = dy / (float)steps;
    float currX = (float)x1, currY = (float)y1;

    float noise = Random::randFloat(0.8f, 1.2f);
    if (Random::randFloat(0.0f, 1.0f) > 0.97f) noise = 1.5f; 

    float destructionLimit = (radius * 0.75f) * noise;
    float blastLimit = radius * noise;
    float currentRayStrength = static_cast<float>(this->strength) * Random::randFloat(0.8f, 1.2f);

    bool onlyDarken = false;

    for (int i = 0; i <= steps; i++)
    {
        int x = (int)std::round(currX);
        int y = (int)std::round(currY);

        if (!world.inBounds(x, y)) break;

        int localX = x - (centerX - maxReach);
        int localY = y - (centerY - maxReach);
        if (localX < 0 || localX >= boxSize || localY < 0 || localY >= boxSize) break;

        int cacheIndex = localY * boxSize + localX;
        uint8_t state = cache[cacheIndex];

        if (state == CacheState::PROCESSED_UNSTOPPED) {
            currX += xInc; currY += yInc; continue;
        }
        else if (state == CacheState::PROCESSED_STOPPED) {
            onlyDarken = true; currX += xInc; currY += yInc; continue;
        }

        float dist = std::hypot(x - centerX, y - centerY);
        BaseComponent* base = world.get<BaseComponent>(x, y);

        // --- ZONE 1: DESTRUCTION ---
        if (dist < destructionLimit)
        {
            if (onlyDarken)
            {
                if (base) applyDarken(x, y, base, dist / radius);
                cache[cacheIndex] = CacheState::PROCESSED_STOPPED;
                if (Random::randFloat(0, 1) > 0.85f) break;
            }
            else
            {
                if (base == nullptr)
                {
                    if (Random::randFloat(0, 1) > 0.5f) world.spawnParticle(GetMatID("Explosion") , x, y);
                    cache[cacheIndex] = CacheState::PROCESSED_UNSTOPPED;
                }
                else
                {
                    Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                    if (!logic) { currX += xInc; currY += yInc; continue; }

                    DurabilityComponent* dur = world.get<DurabilityComponent>(x, y);
                    int resistance = dur ? dur->explosionResistance : 0;

                    float proximityToEdge = dist / destructionLimit;
                    float survivalChance = std::pow(proximityToEdge, 3);

                    if (Random::randFloat(0, 1) < survivalChance && resistance > 2) {
                        applyDarken(x, y, base, 0.2f); 
                        currentRayStrength *= 0.5f; 
                        onlyDarken = true;
                    }
                    else {
                        bool blewUp = logic->explode(base, dur, x, y, (int)currentRayStrength, world);

                        if (blewUp) {
                            currentRayStrength -= (float)resistance;
                            cache[cacheIndex] = CacheState::PROCESSED_UNSTOPPED;
                            if (currentRayStrength <= 0) onlyDarken = true;
                        }
                        else {
                            ThermalComponent* therm = world.get<ThermalComponent>(x, y);
                            logic->receiveHeat(base, therm, x, y, 500, world);
                            applyDarken(x, y, base, 0.1f);
                            cache[cacheIndex] = CacheState::PROCESSED_STOPPED;
                            onlyDarken = true;
                            currentRayStrength = 0;
                        }
                    }
                }
            }
        }
        // --- ZONE 2: BLAST WAVE ---
        else if (dist <= blastLimit)
        {
            if (onlyDarken)
            {
                if (base) applyDarken(x, y, base, dist / radius);
                cache[cacheIndex] = CacheState::PROCESSED_STOPPED;
                if (Random::randFloat(0, 1) > 0.7f) break;
            }
            else
            {
                if (base == nullptr)
                {
                    if (Random::randFloat(0, 1) > 0.5f) world.spawnParticle(GetMatID("Smoke") , x, y);
                    cache[cacheIndex] = CacheState::PROCESSED_UNSTOPPED;
                }
                else
                {
                    applyDarken(x, y, base, (dist / radius) * 1.5f);
                    
                    Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                    if (logic) {
                        ThermalComponent* therm = world.get<ThermalComponent>(x, y);
                        logic->receiveHeat(base, therm, x, y, 300, world);
                    }

                    sf::Vector2f dir((float)(x - centerX), (float)(y - centerY));
                    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len != 0) dir /= len;

                    float velMult = Random::randFloat(3.5f, 5.0f);
                    sf::Vector2f velocity = dir * (float)radius * velMult;
                    
                    particalize(x, y, base, velocity);

                    if (Random::randFloat(0, 1) > 0.8f) break;
                }
            }
        }

        currX += xInc;
        currY += yInc;
    }
}

void Explosion::applyDarken(int x, int y, BaseComponent* base, float factor)
{
    if (!base) return;

    Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
    if (!logic) return;
    
    MaterialGroup group = logic->getGroup();

    if (group == MaterialGroup::ImmovableSolid || group == MaterialGroup::MovableSolid)
    {
        sf::Color originalColor = base->color;
        float randomVariation = Random::randFloat(0.8f, 1.1f);
        float darkenMult = (0.4f + (factor * 0.5f)) * randomVariation;
        darkenMult = std::clamp(darkenMult, 0.2f, 0.95f);

        sf::Color newColor;
        newColor.r = static_cast<uint8_t>(originalColor.r * darkenMult);
        newColor.g = static_cast<uint8_t>(originalColor.g * darkenMult);
        newColor.b = static_cast<uint8_t>(originalColor.b * darkenMult);
        newColor.a = originalColor.a;

        world.setParticleColor(x, y, newColor);
    }
}

void Explosion::particalize(int x, int y, BaseComponent* base, sf::Vector2f velocity)
{
    if (!base || base->id == GetMatID("ExplosiveContainer") ) return;

    if (base->flags.isRigidBodyPart) {
        world.removeParticle(x, y);
        return;
    }

    MaterialID contentId = base->id;
    sf::Color color = base->color;
    bool ignited = base->flags.isIgnited;

    world.removeParticle(x, y);
    ExplosiveContainer::spawnWithPayload(x, y, contentId, velocity, color, ignited, world);
}