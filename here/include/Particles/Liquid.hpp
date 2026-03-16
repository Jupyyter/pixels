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
    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;

    // Helpers (Optimized to take cached properties)
    virtual bool actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, 
                       ParticleWorld& world, bool isFinal, bool isFirst, int depth, 
                       int myDensity, int myDispersionRate);

    virtual bool iterateToAdditional(const ParticleContext& ctx, ParticleWorld& world, int startX, int startY, 
                             int distance, int& currentX, int& currentY, int myDensity);
    
    int getAdditional(float val);
    float getAverageVelOrGravity(float myVel, float otherVel);
};

// --- Water ---
class Water : public Liquid {
public:
    Water() : Liquid(MaterialID::Water) {}
    
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    
    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override;
    bool actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) override;
    bool explode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int strength, ParticleWorld& world) override;
};

// --- Oil ---
class Oil : public Liquid {
public:
    Oil() : Liquid(MaterialID::Oil) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) override;
};

// --- Lava ---
class Lava : public Liquid {
public:
    Lava() : Liquid(MaterialID::Lava) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    
    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override { return false; }
    void checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) override;
    
    bool actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) override;
    bool magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) override { return false; } // Immune
    bool receiveCooling(BaseComponent* base, ThermalComponent* therm, int x, int y, int cooling, ParticleWorld& world) override;
};

// --- Acid ---
class Acid : public Liquid {
public:
    Acid() : Liquid(MaterialID::Acid) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    
    bool actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) override;
    bool corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) override { return false; }
    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override { return false; }
};

// --- Cement ---
class Cement : public Liquid {
public:
    Cement() : Liquid(MaterialID::Cement) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;
    bool receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) override { return false; }
};

// --- Blood ---
class Blood : public Liquid {
public:
    Blood() : Liquid(MaterialID::Blood) {}
    
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    bool actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) override;
};