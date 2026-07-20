#pragma once
#include "Particles/Particle.hpp"
#include "ParticleWorld.hpp"
#include "ParticleDef.hpp"

// --- Base Physics Gas Class ---
class Gas : public Particle {
protected:
    float buoyancy;
    float chaosLevel;
    
public:
    Gas(MaterialID id, float buoy, float chaos);

    static MaterialGroup getStaticGroup() { return MaterialGroup::Gas; }
    MaterialGroup getGroup() const override { return getStaticGroup(); }
    
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;

    virtual bool actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, ParticleWorld& world, bool isFinal, bool isFirst, int depth) override;
    
    bool compareGasDensities(const ParticleContext& ctx, int myX, int myY, int otherX, int otherY, ParticleWorld& world);
    void swapGasForDensities(const ParticleContext& ctx, ParticleWorld& world, int& myX, int& myY, int targetX, int targetY);
};

// --- Generic Data-Driven Gas ---
class GenericGas : public Gas {
protected:
    ParticleDef def;
public:
    GenericGas(MaterialID id, const ParticleDef& definition);

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;
    void checkLifeSpan(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) override;
    bool actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) override;
    
    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override;
    bool corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) override;
    bool explode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int strength, ParticleWorld& world) override;
    bool receiveCharge(BaseComponent* base, int x, int y, ParticleWorld& world) override;
    void takeEffectsDamage(BaseComponent* base, DurabilityComponent* dur, ThermalComponent* therm, int x, int y, ParticleWorld& world) override;
};