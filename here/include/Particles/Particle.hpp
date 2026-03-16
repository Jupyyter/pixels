#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "Constants.hpp"
#include "Random.hpp"
#include <iostream>
#include <cstdlib>

class ParticleWorld;
struct ParticleContext; // Forward declaration (Defined in ParticleWorld.hpp)

// Global Registry for Particle Logic Classes (The "Brains")
class Particle;
extern Particle* MaterialRegistry[256];

// --- COMPONENT SYSTEM (Optimized for SoA + Bitmask) ---

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

    // Default Constructor
    ParticleFlags() 
        : hasBeenUpdatedThisFrame(false), isDead(false), didColorChange(false),
          discolored(false), heated(false), isIgnited(false), 
          isFreeFalling(false), reserved(false), isRigidBodyPart(false) {}

    ParticleFlags(bool updated, bool dead, bool colorChange, bool discolor, 
                  bool heat, bool ignited, bool freeFall, bool res, bool rigid)
        : hasBeenUpdatedThisFrame(updated), isDead(dead), didColorChange(colorChange),
          discolored(discolor), heated(heat), isIgnited(ignited), 
          isFreeFalling(freeFall), reserved(res), isRigidBodyPart(rigid) {}
};

struct BaseComponent {
    MaterialID id;
    sf::Color color;
    ParticleFlags flags;
    uint8_t compMask = 0; // BITMASK: 0 = Empty/Air. 1 = Base. >1 = Has Optional Components.

    // Default Constructor (Required for Chunk Array allocation)
    BaseComponent() 
        : id(static_cast<MaterialID>(0)), color(sf::Color::Transparent), flags(), compMask(0) {}

    // Spawn Constructor
    BaseComponent(MaterialID matId, sf::Color col, ParticleFlags flg) 
        : id(matId), color(col), flags(flg), compMask(1) {} // 1 = HAS_BASE
};

struct KinematicsComponent {
    sf::Vector2f velocity;
    float xThreshold;
    float yThreshold;
    bool isFreeFalling;
    int stoppedMovingCount;

    // Default Constructor
    KinematicsComponent() 
        : velocity(0.f, 0.f), xThreshold(0.f), yThreshold(0.f), 
          isFreeFalling(true), stoppedMovingCount(0) {}

    KinematicsComponent(sf::Vector2f vel, float xTh, float yTh, bool freeFall, int stopped)
        : velocity(vel), xThreshold(xTh), yThreshold(yTh), 
          isFreeFalling(freeFall), stoppedMovingCount(stopped) {}
};

struct DurabilityComponent {
    int health;
    int explosionResistance;

    DurabilityComponent() : health(1), explosionResistance(0) {}

    DurabilityComponent(int hp, int exRes) 
        : health(hp), explosionResistance(exRes) {}
};

struct ThermalComponent {
    int temperature;
    int flammabilityResistance;
    int heatFactor;
    int fireDamage;

    ThermalComponent() : temperature(0), flammabilityResistance(0), heatFactor(0), fireDamage(0) {}

    ThermalComponent(int temp, int flam, int heat, int fire)
        : temperature(temp), flammabilityResistance(flam), 
          heatFactor(heat), fireDamage(fire) {}
};

struct FluidComponent {
    int density;
    int dispersionRate;

    FluidComponent() : density(1), dispersionRate(1) {}

    FluidComponent(int den, int disp) 
        : density(den), dispersionRate(disp) {}
};


class Particle {
public:
    static Particle** GetRegistry() {
        static Particle* registry[256] = { nullptr };
        return registry;
    }
    
    MaterialID id;

    // --- CONSTRUCTOR (AUTO-REGISTRATION) ---
    Particle(MaterialID matId) : id(matId) {
        if (MaterialRegistry[static_cast<int>(matId)] == nullptr) {
            MaterialRegistry[static_cast<int>(matId)] = this;
        }
    }

    virtual ~Particle() = default;

    // --- COMPONENT SETUP ---
    virtual void onSpawn(uint32_t index, int x, int y, ParticleWorld& world);

    // --- LOGIC (OPTIMIZED) ---
    virtual void update(const ParticleContext& ctx, float dt, ParticleWorld& world) = 0;
    
    // Core Actions
    virtual void die(int x, int y, ParticleWorld& world);
    virtual void dieAndReplace(int x, int y, MaterialID newType, ParticleWorld& world);

    // Interaction Logic
    virtual bool actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, ParticleWorld& world, bool isFinal, bool isFirst, int depth) { return false; }
    
    // Pass components directly so the caller doesn't force a re-fetch
    virtual bool actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) {
        return false;
    }

    // Specific Behaviors (ECS Driven with Direct Pointers)
    virtual bool corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world);
    virtual void checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world);
    virtual bool applyHeatToNeighborsIfIgnited(BaseComponent* base, ThermalComponent* therm, int x, int y, ParticleWorld& world);
    virtual void spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world);

    virtual void takeEffectsDamage(BaseComponent* base, DurabilityComponent* dur, ThermalComponent* therm, int x, int y, ParticleWorld& world);

    // Helpers
    virtual bool didNotMove(int currentX, int currentY, int originalX, int originalY);
    virtual bool shouldApplyHeat(BaseComponent* base);

    virtual void checkLifeSpan(BaseComponent* base, DurabilityComponent* dur,int x, int y, ParticleWorld& world);

    virtual bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world);
    virtual bool receiveCooling(BaseComponent* base, ThermalComponent* therm, int x, int y, int cooling, ParticleWorld& world);

    virtual bool magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world);
    virtual bool explode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int strength, ParticleWorld& world);
    virtual bool infect(int x, int y, ParticleWorld& world);

    // Color Logic
    virtual bool stain(BaseComponent* base, int x, int y, sf::Color newColor, ParticleWorld& world);
    virtual bool cleanColor(BaseComponent* base, int x, int y, ParticleWorld& world);

    // Static helper for colors
    static sf::Color getRandomColor(MaterialID id) {
        const auto& props = GetProps(id); 
        if (props.palette.empty()) return sf::Color::White;
        return props.palette[std::rand() % props.palette.size()];
    }
    
    virtual MaterialGroup getGroup() const = 0;
};