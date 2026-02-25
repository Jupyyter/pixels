#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "Constants.hpp"
#include "Random.hpp"
#include "ParticleWorld.hpp"
#include <iostream>
#include <cstdlib>

class ParticleWorld;

// Global Registry for Particle Logic Classes (The "Brains")
class Particle;
extern Particle* MaterialRegistry[256];

class Particle {
public:
static Particle** GetRegistry() {
        static Particle* registry[256] = { nullptr };
        return registry;
    }
    MaterialID id;

    // --- CONSTRUCTOR (AUTO-REGISTRATION) ---
    // automatically registers this class instance into the global registry
    Particle(MaterialID matId) : id(matId) {
        if (MaterialRegistry[static_cast<int>(matId)] == nullptr) {
            MaterialRegistry[static_cast<int>(matId)] = this;
        }
    }

    virtual ~Particle() = default;

    // --- NEW: COMPONENT SETUP ---
    // Instead of variables, we define what data this particle needs when it spawns.
    virtual void onSpawn(uint32_t index, int x, int y, ParticleWorld& world);

    // --- LOGIC ---
    // Note: We now pass 'index' so the logic knows which data to modify.
    
    virtual void update(int x, int y, uint32_t index, float dt, ParticleWorld& world) = 0;
    
    // Core Actions
    virtual void die(uint32_t index, ParticleWorld& world);
    virtual void dieAndReplace(uint32_t index, int x, int y, MaterialID newType, ParticleWorld& world);

    // Interaction Logic
    virtual bool actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, ParticleWorld& world, bool isFinal, bool isFirst, int depth) { return false; }
    
    virtual bool actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) {
        return false;
    }

    // Specific Behaviors (Now reading from components)
    virtual bool corrode(uint32_t index, ParticleWorld& world);
    virtual void checkIfDead(uint32_t index, ParticleWorld& world);
    virtual bool applyHeatToNeighborsIfIgnited(uint32_t index, int x, int y, ParticleWorld& world);
    virtual void spawnSparkIfIgnited(uint32_t index, int x, int y, ParticleWorld& world);

    virtual void takeEffectsDamage(uint32_t index, ParticleWorld& world);

    // Helpers
    virtual bool didNotMove(uint32_t index, int x, int y, ParticleWorld& world);
    virtual bool shouldApplyHeat(uint32_t index, ParticleWorld& world);

    virtual void checkLifeSpan(uint32_t index, ParticleWorld& world);

    virtual bool receiveHeat(uint32_t index, int heat, ParticleWorld& world);
    virtual bool receiveCooling(uint32_t index, int cooling, ParticleWorld& world);

    virtual void magmatize(uint32_t index, int damage, ParticleWorld& world);
    virtual bool explode(uint32_t index, int strength, ParticleWorld& world);
    virtual bool infect(uint32_t index, ParticleWorld& world);

    // Color Logic
    virtual bool stain(uint32_t index, sf::Color newColor, ParticleWorld& world);
    virtual bool cleanColor(uint32_t index, ParticleWorld& world);

    // Static helper for colors
    static sf::Color getRandomColor(MaterialID id) {
        const auto& props = GetProps(id); // Assuming GetProps still exists for palettes
        if (props.palette.empty()) return sf::Color::White;
        return props.palette[std::rand() % props.palette.size()];
    }
    
    // Helper to get group (can still be virtual or moved to properties)
    virtual MaterialGroup getGroup() const = 0;
};