#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <SFML/Graphics.hpp>
#include "Particle.hpp"
#include "Constants.hpp"
#include "Random.hpp"
#include <iostream>

namespace SandSim
{
    class ParticleWorld
    {
    private:
        std::vector<Particle> particles;
        std::vector<std::uint8_t> pixelBuffer;
        int width, height;
        uint32_t frameCounter;

    public:
    
        bool saveWorld(const std::string &baseFilename = "world");
        bool loadWorld(const std::string &filename);
        std::string getNextAvailableFilename(const std::string &baseName);
        ParticleWorld(unsigned int w,unsigned int h, const std::string &worldFile="")
            : width(w), height(h), frameCounter(0)
        {
            particles.resize(width * height);
            pixelBuffer.resize(width * height * 4); // RGBA

            if (!worldFile.empty() && std::filesystem::exists(worldFile))
            {
                if (!loadWorld(worldFile))
                {
                    std::cerr << "Failed to load world file, starting with empty world" << std::endl;
                    clear();
                }
            }
            else
            {
                clear();
            }
        }

        void clear()
        {
            for (auto &p : particles)
            {
                p = Particle::createEmpty();
            }
            std::fill(pixelBuffer.begin(), pixelBuffer.end(), 0);
        }

        int computeIndex(int x, int y) const
        {
            return y * width + x;
        }

        bool inBounds(int x, int y) const
        {
            return x >= 0 && x < width && y >= 0 && y < height;
        }

        bool isEmpty(int x, int y) const
        {
            return inBounds(x, y) && particles[computeIndex(x, y)].id == MaterialID::Empty;
        }

        Particle &getParticleAt(int x, int y)
        {
            return particles[computeIndex(x, y)];
        }

        const Particle &getParticleAt(int x, int y) const
        {
            return particles[computeIndex(x, y)];
        }

        void setParticleAt(int x, int y, const Particle &particle)
        {
            if (!inBounds(x, y))
                return;

            int idx = computeIndex(x, y);
            particles[idx] = particle;

            // Update pixel buffer
            int pixelIdx = idx * 4;
            pixelBuffer[pixelIdx] = particle.color.r;
            pixelBuffer[pixelIdx + 1] = particle.color.g;
            pixelBuffer[pixelIdx + 2] = particle.color.b;
            pixelBuffer[pixelIdx + 3] = particle.color.a;
        }

        void swapParticles(int x1, int y1, int x2, int y2)
        {
            if (!inBounds(x1, y1) || !inBounds(x2, y2))
                return;

            Particle temp = getParticleAt(x1, y1);
            setParticleAt(x1, y1, getParticleAt(x2, y2));
            setParticleAt(x2, y2, temp);
        }

        bool isInLiquid(int x, int y, int *lx, int *ly) const
        {
            // Check all 8 surrounding positions for liquid
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    int nx = x + dx, ny = y + dy;
                    if (inBounds(nx, ny))
                    {
                        const auto &p = getParticleAt(nx, ny);
                        if (p.id == MaterialID::Water || p.id == MaterialID::Oil)
                        {
                            *lx = nx;
                            *ly = ny;
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        bool isInWater(int x, int y, int *lx, int *ly) const
        {
            // Check all 8 surrounding positions for water
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    int nx = x + dx, ny = y + dy;
                    if (inBounds(nx, ny))
                    {
                        const auto &p = getParticleAt(nx, ny);
                        if (p.id == MaterialID::Water)
                        {
                            *lx = nx;
                            *ly = ny;
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        void update(float deltaTime)
        {
            frameCounter++;
            bool frameCounterEven = (frameCounter % 2) == 0;
            int ran = frameCounterEven ? 0 : 1;

            // Update particles from bottom to top to avoid double updates
            for (int y = height - 1; y >= 0; --y)
            {
                for (int x = ran ? 0 : width - 1; ran ? x < width : x > 0; ran ? ++x : --x)
                {
                    auto &particle = getParticleAt(x, y);

                    if (particle.id == MaterialID::Empty)
                        continue;

                    // Update lifetime
                    particle.lifeTime += deltaTime;

                    // Update based on material type
                    switch (particle.id)
                    {
                    case MaterialID::Sand:
                        updateSand(x, y, deltaTime);
                        break;
                    case MaterialID::Water:
                        updateWater(x, y, deltaTime);
                        break;
                    case MaterialID::Salt:
                        updateSalt(x, y, deltaTime);
                        break;
                    case MaterialID::Fire:
                        updateFire(x, y, deltaTime);
                        break;
                    case MaterialID::Smoke:
                        updateSmoke(x, y, deltaTime);
                        break;
                    case MaterialID::Ember:
                        updateEmber(x, y, deltaTime);
                        break;
                    case MaterialID::Steam:
                        updateSteam(x, y, deltaTime);
                        break;
                    case MaterialID::Gunpowder:
                        updateGunpowder(x, y, deltaTime);
                        break;
                    case MaterialID::Oil:
                        updateOil(x, y, deltaTime);
                        break;
                    case MaterialID::Lava:
                        updateLava(x, y, deltaTime);
                        break;
                    case MaterialID::Acid:
                        updateAcid(x, y, deltaTime);
                        break;
                    default:
                        break;
                    }
                }
            }

            // Reset update flags
            for (auto &p : particles)
            {
                p.hasBeenUpdatedThisFrame = false;
            }
        }

        const std::uint8_t *getPixelBuffer() const
        {
            return pixelBuffer.data();
        }

        int getWidth() const { return width; }
        int getHeight() const { return height; }

        void addParticleCircle(int centerX, int centerY, float radius, MaterialID materialType)
        {
            // Iterate over a square bounding box around the circle
            // Start from -radius to +radius for both x and y offsets
            for (int dy = -static_cast<int>(radius); dy <= static_cast<int>(radius); ++dy)
            {
                for (int dx = -static_cast<int>(radius); dx <= static_cast<int>(radius); ++dx)
                {
                    int x = centerX + dx;
                    int y = centerY + dy;

                    // Check if the current (x, y) coordinate is within the circle's radius
                    float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    if (distance <= radius)
                    {
                        // Only place a particle if the target cell is empty
                        if (isEmpty(x, y))
                        {
                            Particle p = createParticleByType(materialType);
                            // Add a small random velocity to new particles for a more dynamic effect
                            p.velocity = {Random::randFloat(-0.5f, 0.5f), Random::randFloat(-0.5f, 0.5f)};
                            setParticleAt(x, y, p);
                        }
                    }
                }
            }
        }

        void eraseCircle(int centerX, int centerY, float radius)
        {
            for (int i = -radius; i < radius; ++i)
            {
                for (int j = -radius; j < radius; ++j)
                {
                    int x = centerX + j;
                    int y = centerY + i;

                    if (inBounds(x, y))
                    {
                        float distance = std::sqrt(i * i + j * j);
                        if (distance <= radius)
                        {
                            setParticleAt(x, y, Particle::createEmpty());
                        }
                    }
                }
            }
        }

    private:
        Particle createParticleByType(MaterialID type)
        {
            switch (type)
            {
            case MaterialID::Sand:
                return Particle::createSand();
            case MaterialID::Water:
                return Particle::createWater();
            case MaterialID::Salt:
                return Particle::createSalt();
            case MaterialID::Wood:
                return Particle::createWood();
            case MaterialID::Fire:
                return Particle::createFire();
            case MaterialID::Smoke:
                return Particle::createSmoke();
            case MaterialID::Ember:
                return Particle::createEmber();
            case MaterialID::Steam:
                return Particle::createSteam();
            case MaterialID::Gunpowder:
                return Particle::createGunpowder();
            case MaterialID::Oil:
                return Particle::createOil();
            case MaterialID::Lava:
                return Particle::createLava();
            case MaterialID::Stone:
                return Particle::createStone();
            case MaterialID::Acid:
                return Particle::createAcid();
            default:
                return Particle::createEmpty();
            }
        }

        // Update functions for different particle types
        void updateLiquidMovement(int x, int y, float dt, float horizontalChance, float velocityMultiplier);
        void updateSolidMovement(int x, int y, float dt, bool canDisplaceLiquids);
        void updateGasMovement(int x, int y, float dt, float buoyancy, float horizontalDrift); // NEW
        bool canGasPassThrough(MaterialID id);                                                 // NEW

        // Update functions for different particle types
        void updateSand(int x, int y, float dt);
        void updateWater(int x, int y, float dt);
        void updateSalt(int x, int y, float dt);
        void updateFire(int x, int y, float dt);
        void updateSmoke(int x, int y, float dt);
        void updateEmber(int x, int y, float dt);
        void updateSteam(int x, int y, float dt);
        void updateGunpowder(int x, int y, float dt);
        void updateOil(int x, int y, float dt);
        void updateLava(int x, int y, float dt);
        void updateAcid(int x, int y, float dt);
    };
}