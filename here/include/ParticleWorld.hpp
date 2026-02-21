#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <SFML/Graphics.hpp>
#include "particles/Particle.hpp"
#include "Constants.hpp"
#include "Random.hpp"
#include "RigidBody.hpp"
#include <iostream>
#include "Particles/Explosion.hpp"
enum class RigidBodyShape; 
class RigidBodySystem; 

class ParticleWorld {
private:
    std::vector<std::unique_ptr<Particle>> particles;
    std::vector<std::uint8_t> pixelBuffer;
    int width, height;
    uint32_t frameCounter;

    std::unique_ptr<RigidBodySystem> rigidBodySystem;
    void updatePixelColor(int x, int y, const sf::Color& color);

public:
    ParticleWorld(unsigned int w, unsigned int h, const std::string& worldFile = "");
    ~ParticleWorld();

    void updateParticleColor(Particle* particle, ParticleWorld& world);
    
    // File I/O
    bool saveWorld(const std::string& baseFilename = "world");
    bool loadWorld(const std::string& filename);
    std::string getNextAvailableFilename(const std::string& baseName);

    void clear();

    // Coordinate utilities
    int computeIndex(int x, int y) const { return y * width + x; }
    bool inBounds(int x, int y) const { return x >= 0 && x < width && y >= 0 && y < height; }
    
    // NULL-SAFE: Checking if a spot is empty now just means checking if the pointer is null
    bool isEmpty(int x, int y) const {
        if (!inBounds(x, y)) return false;
        return particles[computeIndex(x, y)] == nullptr;
    }

    // Particle access
    Particle* getParticleAt(int x, int y) { 
        if (!inBounds(x, y)) return nullptr;
        return particles[computeIndex(x, y)].get(); 
    }
void triggerExplosion(int x, int y, int radius, int strength) {
        // Create the explosion object on the stack and enact it immediately
        Explosion boom(*this, x, y, radius, strength);
        boom.enact();
    }
    void setParticleAt(int x, int y, std::unique_ptr<Particle> particle);
    void swapParticles(int x1, int y1, int x2, int y2);
    // Move logic: Handles transferring pointers and updating pixel buffers
    void moveParticle(int oldX, int oldY, int newX, int newY);

    void update(float deltaTime);

    // Getters
    const std::uint8_t* getPixelBuffer() const { return pixelBuffer.data(); }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

    void addParticleCircle(int centerX, int centerY, float radius, MaterialID materialType);
    void eraseCircle(int centerX, int centerY, float radius);

    void addRigidBody(int centerX, int centerY, float size, RigidBodyShape shape, MaterialID materialType);
    RigidBodySystem* getRigidBodySystem() { return rigidBodySystem.get(); }

    std::unique_ptr<Particle> createParticleByType(MaterialID type);
};