#include "particles/Explosion.hpp"
#include "ParticleWorld.hpp"
#include "particles/ExplosiveContainer.hpp"
#include "Constants.hpp"
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
        // We combine sines to simulate a noise function
        float noise = std::sin(angle * 3.0f + seed1) * 0.15f;
        noise += std::sin(angle * 7.0f + seed2) * 0.10f;
        noise += Random::randFloat(-0.05f, 0.05f); // High-frequency jitter

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
    if (steps == 0)
        return;

    float xInc = dx / (float)steps;
    float yInc = dy / (float)steps;

    float currX = (float)x1;
    float currY = (float)y1;

    // --- RANDOMNESS FACTOR ---
    // 1. General Variance: +/- 20% length for rugged edges
    float noise = Random::randFloat(0.8f, 1.2f);

    // 2. Cracks/Spikes: 3% chance for a ray to be "Super Shrapnel" (extends much further)
    if (Random::randFloat(0.0f, 1.0f) > 0.97f)
    {
        noise = 1.5f;
    }

    // Apply noise to the thresholds
    float destructionLimit = (radius * 0.75f) * noise;
    float blastLimit = radius * noise;

    // 3. Strength Variance: Some rays hit harder than others
    float currentRayStrength = static_cast<float>(this->strength) * Random::randFloat(0.8f, 1.2f);

    bool onlyDarken = false;

    for (int i = 0; i <= steps; i++)
    {
        int x = (int)std::round(currX);
        int y = (int)std::round(currY);

        int localX = x - (centerX - radius);
        int localY = y - (centerY - radius);

        // Bounds check
        if (!world.inBounds(x, y))
            break;
        if (localX < 0 || localX >= boxSize || localY < 0 || localY >= boxSize)
            break;

        int cacheIndex = localY * boxSize + localX;
        uint8_t state = cache[cacheIndex];

        // Optimized Cache Skipping
        if (state == CacheState::PROCESSED_UNSTOPPED)
        {
            currX += xInc;
            currY += yInc;
            continue;
        }
        else if (state == CacheState::PROCESSED_STOPPED)
        {
            onlyDarken = true;
            currX += xInc;
            currY += yInc;
            continue;
        }

        float dist = std::hypot(x - centerX, y - centerY);

        // --- ZONE 1: DESTRUCTION ---
        if (dist < destructionLimit)
        {
            if (onlyDarken)
            {
                applyDarken(x, y, dist / radius); // Use base radius for color gradients to keep them smooth
                cache[cacheIndex] = CacheState::PROCESSED_STOPPED;

                // Randomly stop darkening to create "noise" in the soot pattern
                if (Random::randFloat(0, 1) > 0.85f)
                    break;
            }
            else
            {
                Particle *p = world.getParticleAt(x, y);
                if (p == nullptr)
                {
                    if (Random::randFloat(0, 1) > 0.5f)
                    {
                        auto spark = world.createParticleByType(MaterialID::ExplosionSpark);
                        world.setParticleAt(x, y, std::move(spark));
                    }
                    cache[cacheIndex] = CacheState::PROCESSED_UNSTOPPED;
                }
                else
                {
                    // STOCHASTIC CHANCE:
            // As we get closer to the edge of the destructionLimit, 
            // there is a higher chance the particle survives.
            float proximityToEdge = dist / destructionLimit; // 0 at center, 1 at edge
            float survivalChance = std::pow(proximityToEdge, 3); // Exponential curve

            if (Random::randFloat(0, 1) < survivalChance && p->explosionResistance > 2) {
                // The particle survived the blast but gets charred
                applyDarken(x, y, 0.2f); 
                currentRayStrength *= 0.5f; // Wall absorbed half the ray energy
                onlyDarken = true;
            }
            else{
int resistance = p->explosionResistance;

                    // We pass the JITTERED strength
                    bool blewUp = p->explode(world, (int)currentRayStrength);

                    if (blewUp)
                    {
                        currentRayStrength -= resistance;
                        cache[cacheIndex] = CacheState::PROCESSED_UNSTOPPED;
                        if (currentRayStrength <= 0)
                        {
                            onlyDarken = true;
                        }
                    }
                    else
                    {
                        // This is a particle that RESISTED (like your stone wall)
                        p->receiveHeat(500); // Give it a lot of heat

                        // FORCE DARKEN: Use a lower factor (like 0.1) to represent
                        // the point of impact being heavily charred.
                        applyDarken(x, y, 0.1f);

                        cache[cacheIndex] = CacheState::PROCESSED_STOPPED;
                        onlyDarken = true;
                        currentRayStrength = 0;
                        // We don't 'break' here, so the ray can still
                        // potentially darken the very next pixel (the "soot" behind the impact)
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
                if (Random::randFloat(0, 1) > 0.7f)
                    break;
            }
            else
            {
                Particle *p = world.getParticleAt(x, y);
                if (p == nullptr)
                {
                    if (Random::randFloat(0, 1) > 0.5f)
                    {
                        auto spark = world.createParticleByType(MaterialID::Smoke);
                        world.setParticleAt(x, y, std::move(spark));
                    }
                    cache[cacheIndex] = CacheState::PROCESSED_UNSTOPPED;
                }
                else
                {
                    applyDarken(x, y, (dist / radius) * 1.5f);
                    p->receiveHeat(300);

                    sf::Vector2f dir((float)(x - centerX), (float)(y - centerY));
                    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len != 0)
                        dir /= len;

                    // Vary the velocity slightly too
                    float velMult = Random::randFloat(3.5f, 5.0f);
                    sf::Vector2f velocity = dir * (float)radius * velMult;
                    particalize(x, y, velocity);

                    if (Random::randFloat(0, 1) > 0.8f)
                        break;
                }
            }
        }

        currX += xInc;
        currY += yInc;
    }
}
void Explosion::applyDarken(int x, int y, float factor)
{
    Particle *p = world.getParticleAt(x, y);
    if (p)
    {
        MaterialGroup group = p->getGroup();

        if (group == MaterialGroup::ImmovableSolid || group == MaterialGroup::MovableSolid)
        {
            sf::Color c = p->color;

            // NEW MATH:
            // 'factor' is dist/radius (0.0 at center, 1.0 at edge).
            // We want it to be DARKER at the center.
            // Let's make it: 0.4 (very dark) at center, 0.9 (slightly dark) at edge.
            float randomVariation = Random::randFloat(0.8f, 1.1f);
            float darkenMult = (0.4f + (factor * 0.5f)) * randomVariation;

            // Clamp just in case
            if (darkenMult < 0.2f)
                darkenMult = 0.2f;
            if (darkenMult > 0.95f)
                darkenMult = 0.95f;

            p->color.r = static_cast<uint8_t>(c.r * darkenMult);
            p->color.g = static_cast<uint8_t>(c.g * darkenMult);
            p->color.b = static_cast<uint8_t>(c.b * darkenMult);
            p->didColorChange = true;
            p->discolored = true; // Mark as discolored so it doesn't clean easily
        }
    }
}
void Explosion::particalize(int x, int y, sf::Vector2f velocity)
{
    Particle *p = world.getParticleAt(x, y);
    if (!p)
        return;

    // Safety check: Bedrock never moves, other explosives don't recurse
    if (p->id == MaterialID::ExplosiveContainer)
        return;

    MaterialID contentId = p->id;

    // Kill the original
    world.setParticleAt(x, y, nullptr);

    // Create projectile
    auto container = std::make_unique<ExplosiveContainer>(contentId, velocity);
    container->color = p->color;
    container->isIgnited = p->isIgnited;

    world.setParticleAt(x, y, std::move(container));
}