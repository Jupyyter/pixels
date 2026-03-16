#pragma once
#include "Particles/Particle.hpp"
#include "Random.hpp"
#include <cmath>

class MovableSolid : public Particle {
public:
    MovableSolid(MaterialID id) : Particle(id) {}

    static MaterialGroup getStaticGroup() { return MaterialGroup::MovableSolid; }
    MaterialGroup getGroup() const override { return getStaticGroup(); }

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;

protected:
    void setAdjacentNeighborsFreeFalling(const ParticleContext& ctx, int x, int y, ParticleWorld& world, int depth);
    int getAdditional(float val);
    float getAverageVelOrGravity(float myVel, float otherVel);
    
    virtual bool actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, ParticleWorld& world, bool isFinal, bool isFirst, int depth) override;
};

// --- Derived Classes ---

class Sand : public MovableSolid {
public:
    Sand() : MovableSolid(MaterialID::Sand) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
};

class Dirt : public MovableSolid {
public:
    Dirt() : MovableSolid(MaterialID::Dirt) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
};

class Coal : public MovableSolid {
public:
    Coal() : MovableSolid(MaterialID::Coal) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world) override;
};

class Gunpowder : public MovableSolid {
public:
    Gunpowder() : MovableSolid(MaterialID::Gunpowder) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;
};

class Snow : public MovableSolid {
public:
    Snow() : MovableSolid(MaterialID::Snow) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;
    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override;
};

class Ember : public MovableSolid {
public:
    Ember() : MovableSolid(MaterialID::Ember) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
};

class Salt : public MovableSolid {
public:
    Salt() : MovableSolid(MaterialID::Salt) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
};