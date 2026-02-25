#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <SFML/Graphics.hpp>
#include "Constants.hpp"
#include "Random.hpp"
#include <iostream>
#include <string>

// Forward Declarations
class Particle; 
class RigidBodySystem;
enum class RigidBodyShape;

// --- COMPONENT SYSTEM ---
// (Included directly here to ensure files compile together, usually goes in Components.hpp)

struct ParticleFlags {
    bool hasBeenUpdatedThisFrame : 1;
    bool isDead : 1;
    bool didColorChange : 1;
    bool discolored : 1;
    bool heated : 1;
    bool isIgnited : 1;
    bool isFreeFalling : 1;
    bool reserved : 1;
    bool isRigidBodyPart : 1;
};

struct BaseComponent {
    MaterialID id;
    sf::Color color;
    ParticleFlags flags;
};

struct KinematicsComponent {
    sf::Vector2f velocity;
    float xThreshold;
    float yThreshold;
    bool isFreeFalling;
    int stoppedMovingCount;
};

struct DurabilityComponent {
    int health;
    int explosionResistance;
};

struct ThermalComponent {
    int temperature;
    int flammabilityResistance;
    int heatFactor;
    int fireDamage;
};

struct FluidComponent {
    int density;
    int dispersionRate;
};

// --- COMPONENT MANAGER TEMPLATE ---
const uint32_t INVALID_INDEX = 0xFFFFFFFF;

template <typename T>
class ComponentManager {
public:
    std::vector<uint32_t> sparse;
    std::vector<T> dense;
    std::vector<uint32_t> denseToGrid;

    void init(size_t gridArea) {
        sparse.assign(gridArea, INVALID_INDEX);
        dense.reserve(gridArea / 10); // Reserve 10% capacity
        denseToGrid.reserve(gridArea / 10);
    }

    inline T* get(uint32_t gridIndex) {
        if (gridIndex >= sparse.size()) return nullptr;
        uint32_t denseIndex = sparse[gridIndex];
        if (denseIndex == INVALID_INDEX) return nullptr;
        return &dense[denseIndex];
    }

    void add(uint32_t gridIndex, const T& component) {
        if (gridIndex >= sparse.size() || sparse[gridIndex] != INVALID_INDEX) return;
        sparse[gridIndex] = dense.size();
        denseToGrid.push_back(gridIndex);
        dense.push_back(component);
    }

    void remove(uint32_t gridIndex) {
        if (gridIndex >= sparse.size()) return;
        uint32_t denseIndex = sparse[gridIndex];
        if (denseIndex == INVALID_INDEX) return;

        uint32_t lastDenseIndex = dense.size() - 1;
        if (denseIndex != lastDenseIndex) {
            dense[denseIndex] = std::move(dense[lastDenseIndex]);
            uint32_t lastGridIndex = denseToGrid[lastDenseIndex];
            denseToGrid[denseIndex] = lastGridIndex;
            sparse[lastGridIndex] = denseIndex;
        }
        dense.pop_back();
        denseToGrid.pop_back();
        sparse[gridIndex] = INVALID_INDEX;
    }

    void move(uint32_t oldGridIndex, uint32_t newGridIndex) {
        if (oldGridIndex >= sparse.size() || newGridIndex >= sparse.size()) return;
        uint32_t denseIndex = sparse[oldGridIndex];
        if (denseIndex == INVALID_INDEX) return;

        sparse[newGridIndex] = denseIndex;
        sparse[oldGridIndex] = INVALID_INDEX;
        denseToGrid[denseIndex] = newGridIndex;
    }
    
    void clear() {
        std::fill(sparse.begin(), sparse.end(), INVALID_INDEX);
        dense.clear();
        denseToGrid.clear();
    }
};

// --- MAIN CLASS ---

class ParticleWorld {
private:
    std::vector<std::uint8_t> pixelBuffer;
    int width, height;
    uint32_t frameCounter;

    std::unique_ptr<RigidBodySystem> rigidBodySystem;
    void updatePixelColor(int x, int y, const sf::Color& color);
public:
    // --- MANAGERS ---
    // Public so Logic Classes (Particle.cpp) can access them directly
    ComponentManager<BaseComponent>       baseManager;
    ComponentManager<KinematicsComponent> kinematicsManager;
    ComponentManager<DurabilityComponent> durabilityManager;
    ComponentManager<ThermalComponent>    thermalManager;
    ComponentManager<FluidComponent>      fluidManager;

    ParticleWorld(unsigned int w, unsigned int h, const std::string& worldFile = "");
    ~ParticleWorld();

    void updateParticleColor(uint32_t index, int x, int y);
    std::unordered_map<uint32_t, MaterialID> containerPayloads;
    // File I/O
    bool saveWorld(const std::string& baseFilename = "world");
    bool loadWorld(const std::string& filename);
    std::string getNextAvailableFilename(const std::string& baseName);

    void clear();

    // Coordinate utilities
    inline int computeIndex(int x, int y) const { return y * width + x; }
    inline bool inBounds(int x, int y) const { return x >= 0 && x < width && y >= 0 && y < height; }
    
    // Check if a spot is empty (Fast lookup via BaseManager)
    inline bool isEmpty(int x, int y) {
        if (!inBounds(x, y)) return false;
        return baseManager.get(computeIndex(x, y)) == nullptr;
    }

    // --- REPLACEMENT FOR getParticleAt ---
    // Returns index for manager lookups
    inline uint32_t getIndex(int x, int y) { return computeIndex(x, y); }

    void triggerExplosion(int x, int y, int radius, int strength);
    
    // --- NEW SPAWN/MOVE LOGIC ---
    void spawnParticle(MaterialID id, int x, int y);
    void removeParticle(int x, int y); // Helper to remove from all managers
    void removeParticle(uint32_t index);
    
    // Handles transferring pointers within managers and updating pixel buffers
    void moveParticle(int oldX, int oldY, int newX, int newY);
    void swapParticles(int x1, int y1, int x2, int y2);

    void update(float deltaTime);

    // Getters
    const std::uint8_t* getPixelBuffer() const { return pixelBuffer.data(); }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

    void addParticleCircle(int centerX, int centerY, float radius, MaterialID materialType);
    void eraseCircle(int centerX, int centerY, float radius);

    void addRigidBody(int centerX, int centerY, float size, RigidBodyShape shape, MaterialID materialType);
    RigidBodySystem* getRigidBodySystem() { return rigidBodySystem.get(); }
    // In ParticleWorld.hpp
BaseComponent* getParticleAt(int x, int y) { 
    return baseManager.get(computeIndex(x, y)); 
}
    // Deprecated helpers removed or adapted
    // std::unique_ptr<Particle> createParticleByType(MaterialID type); // No longer needed
};