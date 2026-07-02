#pragma once
#include "Particles/Particle.hpp"
#include "ParticleWorld.hpp"
#include "ParticleDef.hpp"

// --- Base Immovable Physics Class ---
class ImmovableSolid : public Particle {
public:
    ImmovableSolid(MaterialID id) : Particle(id) {}

    static MaterialGroup getStaticGroup() { return MaterialGroup::ImmovableSolid; }
    MaterialGroup getGroup() const override { return getStaticGroup(); }

    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
};

// --- Generic Data-Driven Immovable Solid ---
class GenericImmovableSolid : public ImmovableSolid {
protected:
    ParticleDef def;
public:
    GenericImmovableSolid(MaterialID id, const ParticleDef& definition) 
        : ImmovableSolid(id), def(definition) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) override;
    
    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override;
    bool corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) override;
    bool magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) override;
};