#pragma once
#include "Particles/Solid.hpp"
#include "Random.hpp"
#include <cmath>

class MovableSolid : public Particle { // Assuming Solid isn't updated yet, inheriting from Particle
public:
    MovableSolid(MaterialID id) : Particle(id) {}

    static MaterialGroup getStaticGroup() { return MaterialGroup::MovableSolid; }
    MaterialGroup getGroup() const override { return getStaticGroup(); }

    // Common spawn logic for all Movable Solids
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;

    // Core Update
    void update(int x, int y, uint32_t index, float dt, ParticleWorld& world) override;

protected:
    void setAdjacentNeighborsFreeFalling(int x, int y, ParticleWorld& world, int depth);
    int getAdditional(float val);
    float getAverageVelOrGravity(float myVel, float otherVel);
    
    // Core interaction logic
    bool actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, 
                       ParticleWorld& world, bool isFinal, bool isFirst, int depth) override;
};

// --- Derived Classes ---

class Sand : public MovableSolid {
public:
    Sand() : MovableSolid(MaterialID::Sand) {}
    
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override { return false; }
};

class Dirt : public MovableSolid {
public:
    Dirt() : MovableSolid(MaterialID::Dirt) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override { return false; }
};

class Coal : public MovableSolid {
public:
    Coal() : MovableSolid(MaterialID::Coal) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void spawnSparkIfIgnited(uint32_t index, int x, int y, ParticleWorld& world) override;
};

class Gunpowder : public MovableSolid {
public:
    Gunpowder() : MovableSolid(MaterialID::Gunpowder) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(int x, int y, uint32_t index, float dt, ParticleWorld& world) override;
};

class Snow : public MovableSolid {
public:
    Snow() : MovableSolid(MaterialID::Snow) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(int x, int y, uint32_t index, float dt, ParticleWorld& world) override;
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override;
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