#pragma once
#include "Particles/Particle.hpp"
#include "ParticleWorld.hpp"

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

class Steam : public Gas {
public:
    Steam() : Gas(MaterialID::Steam, 1.0f, 1.8f) {} 
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    // Updated signature: takes direct pointers
    void checkLifeSpan(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) override;
};

class FlammableGas : public Gas {
public:
    FlammableGas() : Gas(MaterialID::FlammableGas, 1.0f, 1.8f) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
};

class Spark : public Gas {
public:
    Spark() : Gas(MaterialID::Spark, 1.0f, 1.8f) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, ParticleWorld& world, bool isFinal, bool isFirst, int depth) override;
    void spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world) override {}
};

class ExplosionSpark : public Gas {
public:
    ExplosionSpark() : Gas(MaterialID::ExplosionSpark, 1.0f, 2.0f) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, ParticleWorld& world, bool isFinal, bool isFirst, int depth) override;
};

class Smoke : public Gas {
public:
    Smoke() : Gas(MaterialID::Smoke, 0.8f, 1.2f) {}
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
};