#include "Particles/Explosion.hpp"
#include "ParticleWorld.hpp"
#include "Particles/ExplosiveContainer.hpp"
#include "Constants.hpp"
#include "Particles/Particle.hpp" // Needed for MaterialRegistry
#include <cmath>
#include <algorithm>

Explosion::Explosion(ParticleWorld &worldRef, int x, int y, int r, int s)
    : world(worldRef), centerX(x), centerY(y), radius(r), strength(s) {}

void Explosion::enact()
{
    int boxSize = (radius * 2) + 1;
    std::vector<uint8_t> cache(boxSize * boxSize, CacheState::UNVISITED);

    int rayCount = static_cast<int>(radius * 2.0f * 3.14159f * 1.5f); // Slightly more rays for detail
    if (rayCount < 16)
        rayCount = 16;

    // These variables create a "DNA" for this specific explosion's shape
    float seed1 = Random::randFloat(0, 100);
    float seed2 = Random::randFloat(0, 100);

    for (int i = 0; i < rayCount; i++)
    {
        float angle = (static_cast<float>(i) / rayCount) * 2.0f * 3.14159f;

        // --- NOISY RADIUS CALCULATION ---
        float noise = std::sin(angle * 3.0f + seed1) * 0.15f;
        noise += std::sin(angle * 7.0f + seed2) * 0.10f;
        noise += Random::randFloat(-0.05f, 0.05f); 

        float finalRadiusMultiplier = 1.0f + noise;

        int destX = centerX + static_cast<int>(std::cos(angle) * radius * finalRadiusMultiplier);
        int destY = centerY + static_cast<int>(std::sin(angle) * radius * finalRadiusMultiplier);

        castRay(destX, destY, cache, boxSize);
    }
}

void Explosion::castRay(int destX, int destY, std::vector<uint8_t> &cache, int boxSize)
{
    int x1 = centerX;
    int y1 = centerY;

    int dx = destX - x1;
    int dy = destY - y1;

    int steps = std::max(std::abs(dx), std::abs(dy));
    if (steps == 0) return;

    float xInc = dx / (float)steps;
    float yInc = dy / (float)steps;

    float currX = (float)x1;
    float currY = (float)y1;

    // --- RANDOMNESS FACTOR ---
    float noise = Random::randFloat(0.8f, 1.2f);
    if (Random::randFloat(0.0f, 1.0f) > 0.97f) {
        noise = 1.5f; // Super Shrapnel
    }

    float destructionLimit = (radius * 0.75f) * noise;
    float blastLimit = radius * noise;
    float currentRayStrength = static_cast<float>(this->strength) * Random::randFloat(0.8f, 1.2f);

    bool onlyDarken = false;

    for (int i = 0; i <= steps; i++)
    {
        int x = (int)std::round(currX);
        int y = (int)std::round(currY);

        int localX = x - (centerX - radius);
        int localY = y - (centerY - radius);

        // Bounds check
        if (!world.inBounds(x, y)) break;
        if (localX < 0 || localX >= boxSize || localY < 0 || localY >= boxSize) break;

        int cacheIndex = localY * boxSize + localX;
        uint8_t state = cache[cacheIndex];

        // Optimized Cache Skipping
        if (state == CacheState::PROCESSED_UNSTOPPED) {
            currX += xInc; currY += yInc; continue;
        }
        else if (state == CacheState::PROCESSED_STOPPED) {
            onlyDarken = true; currX += xInc; currY += yInc; continue;
        }

        float dist = std::hypot(x - centerX, y - centerY);
        uint32_t index = world.getIndex(x, y);
        
        // Retrieve the base component to check existence
        BaseComponent* base = world.baseManager.get(index);

        // --- ZONE 1: DESTRUCTION ---
        if (dist < destructionLimit)
        {
            if (onlyDarken)
            {
                applyDarken(x, y, dist / radius);
                cache[cacheIndex] = CacheState::PROCESSED_STOPPED;
                if (Random::randFloat(0, 1) > 0.85f) break;
            }
            else
            {
                // EMPTY SPOT
                if (base == nullptr)
                {
                    if (Random::randFloat(0, 1) > 0.5f) {
                        world.spawnParticle(MaterialID::ExplosionSpark, x, y);
                    }
                    cache[cacheIndex] = CacheState::PROCESSED_UNSTOPPED;
                }
                // PARTICLE HIT
                else
                {
                    // Retrieve Logic and Durability
                    Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                    DurabilityComponent* dur = world.durabilityManager.get(index);
                    int resistance = dur ? dur->explosionResistance : 0;

                    // STOCHASTIC CHANCE
                    float proximityToEdge = dist / destructionLimit;
                    float survivalChance = std::pow(proximityToEdge, 3);

                    if (Random::randFloat(0, 1) < survivalChance && resistance > 2) {
                        // Survived
                        applyDarken(x, y, 0.2f); 
                        currentRayStrength *= 0.5f; 
                        onlyDarken = true;
                    }
                    else {
                        // Attempt Explode
                        bool blewUp = false;
                        if (logic) {
                            blewUp = logic->explode(index, (int)currentRayStrength, world);
                        }

                        if (blewUp) {
                            currentRayStrength -= resistance;
                            cache[cacheIndex] = CacheState::PROCESSED_UNSTOPPED;
                            if (currentRayStrength <= 0) onlyDarken = true;
                        }
                        else {
                            // Resisted
                            if (logic) logic->receiveHeat(index, 500, world);
                            applyDarken(x, y, 0.1f);
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
                applyDarken(x, y, dist / radius);
                cache[cacheIndex] = CacheState::PROCESSED_STOPPED;
                if (Random::randFloat(0, 1) > 0.7f) break;
            }
            else
            {
                if (base == nullptr)
                {
                    if (Random::randFloat(0, 1) > 0.5f) {
                        world.spawnParticle(MaterialID::Smoke, x, y);
                    }
                    cache[cacheIndex] = CacheState::PROCESSED_UNSTOPPED;
                }
                else
                {
                    applyDarken(x, y, (dist / radius) * 1.5f);
                    
                    Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                    if (logic) logic->receiveHeat(index, 300, world);

                    sf::Vector2f dir((float)(x - centerX), (float)(y - centerY));
                    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len != 0) dir /= len;

                    float velMult = Random::randFloat(3.5f, 5.0f);
                    sf::Vector2f velocity = dir * (float)radius * velMult;
                    
                    particalize(x, y, velocity);

                    if (Random::randFloat(0, 1) > 0.8f) break;
                }
            }
        }

        currX += xInc;
        currY += yInc;
    }
}

void Explosion::applyDarken(int x, int y, float factor)
{
    uint32_t index = world.getIndex(x, y);
    BaseComponent* base = world.baseManager.get(index);
    
    if (base)
    {
        // Check group via Logic Class
        Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
        if (!logic) return;
        
        MaterialGroup group = logic->getGroup(); // Ensure logic class has this

        if (group == MaterialGroup::ImmovableSolid || group == MaterialGroup::MovableSolid)
        {
            sf::Color c = base->color;

            float randomVariation = Random::randFloat(0.8f, 1.1f);
            float darkenMult = (0.4f + (factor * 0.5f)) * randomVariation;

            if (darkenMult < 0.2f) darkenMult = 0.2f;
            if (darkenMult > 0.95f) darkenMult = 0.95f;

            base->color.r = static_cast<uint8_t>(c.r * darkenMult);
            base->color.g = static_cast<uint8_t>(c.g * darkenMult);
            base->color.b = static_cast<uint8_t>(c.b * darkenMult);
            
            base->flags.didColorChange = true;
            base->flags.discolored = true;
            
            // Immediately update visual
            world.updateParticleColor(index, x, y);
        }
    }
}

void Explosion::particalize(int x, int y, sf::Vector2f velocity)
{
    uint32_t index = world.getIndex(x, y);
    BaseComponent* base = world.baseManager.get(index);
    
    if (!base) return;

    // Safety check: Bedrock never moves, other explosives don't recurse
    if (base->id == MaterialID::ExplosiveContainer) return;

    MaterialID contentId = base->id;
    sf::Color color = base->color;
    bool ignited = base->flags.isIgnited;

    // Kill the original
    world.removeParticle(index);

    // Create projectile using the static helper for ExplosiveContainer
    // This correctly sets up the payload map
    ExplosiveContainer::spawnWithPayload(x, y, contentId, velocity, color, ignited, world);
}