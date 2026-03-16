#include "Particles/ImmovableSolid.hpp"
#include "ParticleWorld.hpp"
#include "Random.hpp"

// --- BASE IMMOVABLE IMPLEMENTATION ---

void ImmovableSolid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);

    // Default durability component for immovables
    world.add<DurabilityComponent>(x, y, DurabilityComponent(1000, 2));
}

void ImmovableSolid::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    // Zero-overhead fetch since they are in the exact same chunk
    auto* base = world.getFast<BaseComponent>(ctx, ctx.x, ctx.y);
    if (!base) return;

    // FAST PATH: Over 99% of immovables are not on fire.
    // We only fetch ThermalComponent if we are actually ignited.
    if (base->flags.isIgnited) {
        auto* therm = world.getFast<ThermalComponent>(ctx, ctx.x, ctx.y);
        applyHeatToNeighborsIfIgnited(base, therm, ctx.x, ctx.y, world);
        spawnSparkIfIgnited(base, ctx.x, ctx.y, world);
    }
    
    // Process damage (Acid, etc.)
    auto* dur = world.getFast<DurabilityComponent>(ctx, ctx.x, ctx.y);
    auto* therm = world.getFast<ThermalComponent>(ctx, ctx.x, ctx.y);
    takeEffectsDamage(base, dur, therm, ctx.x, ctx.y, world);
    
    // We pass ctx.chunk here so the world doesn't have to look up the chunk to change pixel colors!
    world.updateParticleColor(ctx.index, ctx.x, ctx.y, ctx.chunk);
}

// --- STONE ---

void Stone::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    ImmovableSolid::onSpawn(index, x, y, world);
    
    if (auto* dur = world.get<DurabilityComponent>(x, y)) {
        dur->health = 500;
        dur->explosionResistance = 4;
    }
}

// --- BRICK ---

void Brick::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    ImmovableSolid::onSpawn(index, x, y, world);
    
    if (auto* dur = world.get<DurabilityComponent>(x, y)) {
        dur->health = 500;
        dur->explosionResistance = 4;
    }
}

// --- SLIME MOLD ---

void SlimeMold::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    ImmovableSolid::onSpawn(index, x, y, world);

    if (auto* dur = world.get<DurabilityComponent>(x, y)) {
        dur->health = 40;
    }

    world.add<ThermalComponent>(x, y, ThermalComponent(
        0,   // temp
        10,  // flammabilityResistance 
        5,   // heatFactor 
        3    // fireDamage 
    ));
}

// --- WOOD ---

void Wood::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    ImmovableSolid::onSpawn(index, x, y, world);

    if (auto* dur = world.get<DurabilityComponent>(x, y)) {
        dur->health = Random::randInt(100, 200);
    }

    world.add<ThermalComponent>(x, y, ThermalComponent(
        0,   // temp
        40,  // flammabilityResistance
        10,  // heatFactor
        1    // fireDamage 
    ));
}

void Wood::checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    if (dur && dur->health <= 0) {
        if (base && base->flags.isIgnited && Random::randFloat(0.0f, 1.0f) > 0.95f) {
            dieAndReplace(x, y, MaterialID::Ember, world);
        } else {
            die(x, y, world);
        }
    }
}

// --- AUTO REGISTRATION ---
static Stone stone_instance;
static Brick brick_instance;
static SlimeMold slime_instance;
static Wood wood_instance;