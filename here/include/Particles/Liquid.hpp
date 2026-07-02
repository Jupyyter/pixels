#pragma once
#include "Particles/Particle.hpp"
#include "Particles/ParticleDef.hpp"
#include "Random.hpp"

// --- Base Liquid Class ---
class Liquid : public Particle {
public:
    Liquid(MaterialID id) : Particle(id) {}
    static MaterialGroup getStaticGroup() { return MaterialGroup::Liquid; }
    MaterialGroup getGroup() const override { return getStaticGroup(); }

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;

    virtual bool actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, ParticleWorld& world, bool isFinal, bool isFirst, int depth, int myDensity, int myDispersionRate);
    virtual bool iterateToAdditional(const ParticleContext& ctx, ParticleWorld& world, int startX, int startY, int distance, int& currentX, int& currentY, int myDensity);
    
    int getAdditional(float val);
    float getAverageVelOrGravity(float myVel, float otherVel);
};

// --- Generic Data-Driven Liquid ---
class GenericLiquid : public Liquid {
protected:
    ParticleDef def;
public:
    GenericLiquid(MaterialID id, const ParticleDef& definition) 
        : Liquid(id), def(definition) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;
    void checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) override;
    bool actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) override;
    
    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override;
    bool corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) override;
    bool magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) override;
    
    void spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world) override;
};