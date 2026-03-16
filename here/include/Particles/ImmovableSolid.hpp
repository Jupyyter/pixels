#pragma once
#include "Particles/Particle.hpp"
#include "ParticleWorld.hpp"

// --- Base Immovable Class ---
class ImmovableSolid : public Particle {
public:
    ImmovableSolid(MaterialID id) : Particle(id) {}

    static MaterialGroup getStaticGroup() { return MaterialGroup::ImmovableSolid; }
    MaterialGroup getGroup() const override { return getStaticGroup(); }

    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;
    
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
};

// --- Specific Implementations ---

class Stone : public ImmovableSolid {
public:
    Stone() : ImmovableSolid(MaterialID::Stone) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override { return false; }
};

class Brick : public ImmovableSolid {
public:
    Brick() : ImmovableSolid(MaterialID::Brick) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override { return false; }
};

class SlimeMold : public ImmovableSolid {
public:
    SlimeMold() : ImmovableSolid(MaterialID::SlimeMold) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
};

class Wood : public ImmovableSolid {
public:
    Wood() : ImmovableSolid(MaterialID::Wood) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) override;
};