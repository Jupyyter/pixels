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
        // File I/O operations
        bool saveWorld(const std::string &baseFilename = "world");
        bool loadWorld(const std::string &filename);
        std::string getNextAvailableFilename(const std::string &baseName);
        
        // Constructor - loads world file if specified
        ParticleWorld(unsigned int w, unsigned int h, const std::string &worldFile = "");

        // Reset world to empty state
        void clear();

        // Coordinate/bounds utilities
        int computeIndex(int x, int y) const { return y * width + x; }
        bool inBounds(int x, int y) const { return x >= 0 && x < width && y >= 0 && y < height; }
        bool isEmpty(int x, int y) const { return inBounds(x, y) && particles[computeIndex(x, y)].id == MaterialID::Empty; }

        // Particle access
        Particle &getParticleAt(int x, int y) { return particles[computeIndex(x, y)]; }
        const Particle &getParticleAt(int x, int y) const { return particles[computeIndex(x, y)]; }
        void setParticleAt(int x, int y, const Particle &particle);
        void swapParticles(int x1, int y1, int x2, int y2);

        // Liquid detection utilities
        bool isInLiquid(int x, int y, int *lx, int *ly) const;
        bool isInWater(int x, int y, int *lx, int *ly) const;

        // Main simulation update
        void update(float deltaTime);

        // Rendering
        const std::uint8_t *getPixelBuffer() const { return pixelBuffer.data(); }
        int getWidth() const { return width; }
        int getHeight() const { return height; }

        // Particle placement/removal
        void addParticleCircle(int centerX, int centerY, float radius, MaterialID materialType);
        void eraseCircle(int centerX, int centerY, float radius);

    private:
        // Factory method for creating particles by type
        Particle createParticleByType(MaterialID type);

        // Movement algorithms for different physics types
        void updateLiquidMovement(int x, int y, float dt, float horizontalChance, float velocityMultiplier);
        void updateSolidMovement(int x, int y, float dt, bool canDisplaceLiquids);
        void updateGasMovement(int x, int y, float dt, float buoyancy, float horizontalDrift);

        // Material-specific update functions
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