#pragma once
#include "Particles/Particle.hpp"
#include "Random.hpp"

// --- Base Liquid Class ---
class Liquid : public Particle {
public:
    Liquid(MaterialID id) : Particle(id) {}
    static MaterialGroup getStaticGroup() { return MaterialGroup::Liquid; }
    MaterialGroup getGroup() const override { return getStaticGroup(); }

    // Common spawn logic for all liquids
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;

    // Final update logic
    void update(int x, int y, uint32_t index, float dt, ParticleWorld& world) override;

    // Helpers
    bool actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, 
                       ParticleWorld& world, bool isFinal, bool isFirst, int depth) override;

    bool iterateToAdditional(ParticleWorld& world, int startX, int startY, int distance, 
                             uint32_t myIndex, int& currentX, int& currentY);
    
    int getAdditional(float val);
    float getAverageVelOrGravity(float myVel, float otherVel);
};

// --- Water ---
class Water : public Liquid {
public:
    Water() : Liquid(MaterialID::Water) {}
    
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override;
    bool actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) override;
    bool explode(uint32_t index, int strength, ParticleWorld& world) override;
};

// --- Oil ---
class Oil : public Liquid {
public:
    Oil() : Liquid(MaterialID::Oil) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) override;
};

// --- Lava ---
class Lava : public Liquid {
public:
    Lava() : Liquid(MaterialID::Lava) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override { return false; }
    void checkIfDead(uint32_t index, ParticleWorld& world) override;
    
    bool actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) override;
    void magmatize(uint32_t index, int damage, ParticleWorld& world) override {} // Immune
    bool receiveCooling(uint32_t index, int cooling, ParticleWorld& world) override;
};

// --- Acid ---
class Acid : public Liquid {
public:
    Acid() : Liquid(MaterialID::Acid) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    
    bool actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) override;
    bool corrode(uint32_t index, ParticleWorld& world) override { return false; }
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override { return false; }
};

// --- Cement ---
class Cement : public Liquid {
public:
    Cement() : Liquid(MaterialID::Cement) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(int x, int y, uint32_t index, float dt, ParticleWorld& world) override;
    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override { return false; }
};

// --- Blood ---
class Blood : public Liquid {
public:
    Blood() : Liquid(MaterialID::Blood) {}
    
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) override;
};