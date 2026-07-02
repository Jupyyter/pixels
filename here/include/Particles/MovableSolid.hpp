#pragma once
#include "Particles/Particle.hpp"
#include "ParticleWorld.hpp"
#include "ParticleDef.hpp"
#include "Random.hpp"
#include <cmath>

// --- Base Movable Physics Class ---
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

// --- Generic Data-Driven Movable Solid ---
class GenericMovableSolid : public MovableSolid {
protected:
    ParticleDef def;
public:
    GenericMovableSolid(MaterialID id, const ParticleDef& definition) 
        : MovableSolid(id), def(definition) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;
    void checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) override;
    bool actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) override;

    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override;
    bool corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) override;
    bool magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) override;

    void spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world) override;
};