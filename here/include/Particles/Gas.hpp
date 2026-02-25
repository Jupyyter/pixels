#pragma once
#include "Particles/Particle.hpp"
#include "ParticleWorld.hpp"

class Gas : public Particle {
protected:
    // Properties specific to Gas Logic
    float buoyancy;
    float chaosLevel;
    
public:
    Gas(MaterialID id, float buoy, float chaos);

    static MaterialGroup getStaticGroup() { return MaterialGroup::Gas; }
    MaterialGroup getGroup() const override { return getStaticGroup(); }
    
    // Common Spawn Logic
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;

    // Core Update
    void update(int x, int y, uint32_t index, float dt, ParticleWorld& world) override;

    // Interaction Logic
    bool actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, 
                       ParticleWorld& world, bool isFinal, bool isFirst, int depth) override;
    
    // Helpers
    bool compareGasDensities(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world);
    void swapGasForDensities(ParticleWorld& world, uint32_t myIndex, uint32_t otherIndex, 
                             int neighborX, int neighborY, int& currentX, int& currentY);
    
    bool corrode(uint32_t index, ParticleWorld& world) override { return false; }
};

class Steam : public Gas {
public:
    Steam() : Gas(MaterialID::Steam, 1.0f, 1.8f) {} 
    
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void checkLifeSpan(uint32_t index, ParticleWorld& world) override;
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override { return false; }
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
    bool actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, 
                       ParticleWorld& world, bool isFinal, bool isFirst, int depth) override;
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override { return false; }
    void spawnSparkIfIgnited(uint32_t index, int x, int y, ParticleWorld& world) override {};
};

class ExplosionSpark : public Gas {
public:
    ExplosionSpark() : Gas(MaterialID::ExplosionSpark, 1.0f, 2.0f) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, 
                       ParticleWorld& world, bool isFinal, bool isFirst, int depth) override;
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override { return false; }
    void spawnSparkIfIgnited(uint32_t index, int x, int y, ParticleWorld& world) override {};
};

class Smoke : public Gas {
public:
    Smoke() : Gas(MaterialID::Smoke, 0.8f, 1.2f) {}
    
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override { return false; }
};