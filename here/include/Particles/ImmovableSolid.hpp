#pragma once
#include "Particles/Solid.hpp" // Assuming Solid is updated to inherit Particle
#include "ParticleWorld.hpp"

// --- Base Immovable Class ---
class ImmovableSolid : public Particle { // Or public Solid if you updated Solid.hpp
public:
    ImmovableSolid(MaterialID id) : Particle(id) {}

    static MaterialGroup getStaticGroup() { return MaterialGroup::ImmovableSolid; }
    MaterialGroup getGroup() const override { return getStaticGroup(); }

    // Even static particles get an update to process heat/damage
    void update(int x, int y, uint32_t index, float dt, ParticleWorld& world) override {
        // 1. Visuals
        // In the new system, we assume World handles visual updates based on flags,
        // or we call updateParticleColor if we need dynamic heat coloring.
        // world.updateParticleColor(index, x, y);

        // 2. Heat & Damage
        applyHeatToNeighborsIfIgnited(index, x, y, world);
        takeEffectsDamage(index, world);
        spawnSparkIfIgnited(index, x, y, world);
    }
};

// --- Specific Implementations ---

class Stone : public ImmovableSolid {
public:
    Stone() : ImmovableSolid(MaterialID::Stone) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override {
        ImmovableSolid::onSpawn(index, x, y, world);
        
        // Stone has durability but no heat transfer usually
        DurabilityComponent dur;
        dur.health = 500;
        dur.explosionResistance = 4;
        world.durabilityManager.add(index, dur);
    }

    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override {
        return false; 
    }
    
    // Clone is no longer needed in Singleton Logic approach
};

class Brick : public ImmovableSolid {
public:
    Brick() : ImmovableSolid(MaterialID::Brick) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override {
        ImmovableSolid::onSpawn(index, x, y, world);
        
        DurabilityComponent dur;
        dur.health = 500;
        dur.explosionResistance = 4;
        world.durabilityManager.add(index, dur);
    }

    bool receiveHeat(uint32_t index, int heat, ParticleWorld& world) override {
        return false; 
    }
};

class SlimeMold : public ImmovableSolid {
public:
    SlimeMold() : ImmovableSolid(MaterialID::SlimeMold) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override {
        ImmovableSolid::onSpawn(index, x, y, world);

        DurabilityComponent dur;
        dur.health = 40;
        world.durabilityManager.add(index, dur);

        ThermalComponent therm;
        therm.flammabilityResistance = 10;
        therm.heatFactor = 5;
        therm.fireDamage = 3;
        world.thermalManager.add(index, therm);
    }
};

class Wood : public ImmovableSolid {
public:
    Wood() : ImmovableSolid(MaterialID::Wood) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override {
        ImmovableSolid::onSpawn(index, x, y, world);

        DurabilityComponent dur;
        dur.health = (std::rand() % 100) + 100;
        world.durabilityManager.add(index, dur);

        ThermalComponent therm;
        therm.flammabilityResistance = 40;
        therm.heatFactor = 10;
        therm.fireDamage = 1; // Burns slowly
        world.thermalManager.add(index, therm);
    }

    void checkIfDead(uint32_t index, ParticleWorld& world) override {
        auto* dur = world.durabilityManager.get(index);
        auto* base = world.baseManager.get(index);

        if (dur && dur->health <= 0) {
            if (base && base->flags.isIgnited && (static_cast<float>(std::rand()) / RAND_MAX) > 0.95f) {
                dieAndReplace(index, 0, 0, MaterialID::Ember, world); // Coords ignored by dieAndReplace wrapper usually, or passed
            } else {
                die(index, world);
            }
        }
    }
};