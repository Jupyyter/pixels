#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "Constants.hpp"
#include "Random.hpp"
#include <iostream>
#include <cstdlib>
#include "ParticleDef.hpp"

class ParticleWorld;
struct ParticleContext;

class Particle;
extern Particle* MaterialRegistry[256];

struct ParticleFlags {
    bool hasBeenUpdatedThisFrame : 1;
    bool isDead : 1;
    bool didColorChange : 1;
    bool discolored : 1;
    bool heated : 1;
    bool isIgnited : 1;
    bool isFreeFalling : 1;
    bool isCharged : 1;   // <--- CHANGED: Used to hold electricity
    bool isRigidBodyPart : 1;

    ParticleFlags() 
        : hasBeenUpdatedThisFrame(false), isDead(false), didColorChange(false),
          discolored(false), heated(false), isIgnited(false), 
          isFreeFalling(false), isCharged(false), isRigidBodyPart(false) {}

    ParticleFlags(bool updated, bool dead, bool colorChange, bool discolor, 
                  bool heat, bool ignited, bool freeFall, bool charge, bool rigid)
        : hasBeenUpdatedThisFrame(updated), isDead(dead), didColorChange(colorChange),
          discolored(discolor), heated(heat), isIgnited(ignited), 
          isFreeFalling(freeFall), isCharged(charge), isRigidBodyPart(rigid) {}
};

struct BaseComponent {
    MaterialID id;
    sf::Color color;
    ParticleFlags flags;
    uint8_t compMask = 0; 

    BaseComponent() 
        : id(static_cast<MaterialID>(0)), color(sf::Color::Transparent), flags(), compMask(0) {}

    BaseComponent(MaterialID matId, sf::Color col, ParticleFlags flg) 
        : id(matId), color(col), flags(flg), compMask(1) {} 
};

struct KinematicsComponent {
    sf::Vector2f velocity;
    float xThreshold;
    float yThreshold;
    bool isFreeFalling;
    int stoppedMovingCount;

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

    Particle(MaterialID matId) : id(matId) {
        if (MaterialRegistry[static_cast<int>(matId)] == nullptr) {
            MaterialRegistry[static_cast<int>(matId)] = this;
        }
    }

    virtual ~Particle() = default;

    virtual void onSpawn(uint32_t index, int x, int y, ParticleWorld& world);
    virtual void update(const ParticleContext& ctx, float dt, ParticleWorld& world) = 0;
    
    virtual void die(int x, int y, ParticleWorld& world);
    virtual void dieAndReplace(int x, int y, MaterialID newType, ParticleWorld& world);

    virtual bool actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, ParticleWorld& world, bool isFinal, bool isFirst, int depth) { return false; }
    virtual bool actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) { return false; }

    virtual bool corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world);
    virtual void checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world);
    virtual bool applyHeatToNeighborsIfIgnited(BaseComponent* base, ThermalComponent* therm, int x, int y, ParticleWorld& world);
    virtual void spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world);

    virtual void takeEffectsDamage(BaseComponent* base, DurabilityComponent* dur, ThermalComponent* therm, int x, int y, ParticleWorld& world);

    virtual bool didNotMove(int currentX, int currentY, int originalX, int originalY);
    virtual bool shouldApplyHeat(BaseComponent* base);
    virtual void checkLifeSpan(BaseComponent* base, DurabilityComponent* dur,int x, int y, ParticleWorld& world);

    virtual bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world);
    virtual bool receiveCooling(BaseComponent* base, ThermalComponent* therm, int x, int y, int cooling, ParticleWorld& world);
    virtual bool receiveCharge(BaseComponent* base, int x, int y, ParticleWorld& world); // <--- NEW

    virtual bool magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world);
    virtual bool explode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int strength, ParticleWorld& world);
    virtual bool infect(int x, int y, ParticleWorld& world);

    virtual bool stain(BaseComponent* base, int x, int y, sf::Color newColor, ParticleWorld& world);
    virtual bool cleanColor(BaseComponent* base, int x, int y, ParticleWorld& world);
    
    bool executeGenericTraitsAndInteractions(const ParticleDef& def, BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world);
    
    void processAdvancedOrganicAndElectricalTraits(const ParticleDef& def, const ParticleContext& ctx, ParticleWorld& world); // <--- NEW

    virtual float getGravityMult() const { return 1.0f; } // <--- NEW
    virtual float getBounciness() const { return 0.0f; }  // <--- NEW

    static sf::Color getRandomColor(MaterialID id);
    
    virtual MaterialGroup getGroup() const = 0;
};