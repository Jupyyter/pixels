#include "Particles/ImmovableSolid.hpp"
#include "ParticleWorld.hpp"
#include "Random.hpp"

void ImmovableSolid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);
}

void ImmovableSolid::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    auto* base = world.getFast<BaseComponent>(ctx, ctx.x, ctx.y);
    if (!base) return;

    if (base->flags.isIgnited) {
        auto* therm = world.getFast<ThermalComponent>(ctx, ctx.x, ctx.y);
        applyHeatToNeighborsIfIgnited(base, therm, ctx.x, ctx.y, world);
        spawnSparkIfIgnited(base, ctx.x, ctx.y, world);
    }
    
    auto* dur = world.getFast<DurabilityComponent>(ctx, ctx.x, ctx.y);
    auto* therm = world.getFast<ThermalComponent>(ctx, ctx.x, ctx.y);
    takeEffectsDamage(base, dur, therm, ctx.x, ctx.y, world);
    
    world.updateParticleColor(ctx.index, ctx.x, ctx.y, ctx.chunk);
}

void GenericImmovableSolid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    ImmovableSolid::onSpawn(index, x, y, world);

    if (auto* base = world.get<BaseComponent>(x, y)) {
        base->color = def.getRandomColor();
        if (def.ignite_on_spawn) base->flags.isIgnited = true;
        if (def.heated_on_spawn) base->flags.heated = true;
    }

    if (def.has_durability) {
        int hp = (def.dur_health_max > def.dur_health) ? Random::randInt(def.dur_health, def.dur_health_max) : def.dur_health;
        world.add<DurabilityComponent>(x, y, DurabilityComponent(hp, def.dur_expRes));
    }
    if (def.has_thermal) {
        world.add<ThermalComponent>(x, y, ThermalComponent(def.therm_temp, def.therm_flamRes, def.therm_heat, def.therm_fireDmg));
    }
}

void GenericImmovableSolid::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    ImmovableSolid::update(ctx, dt, world);
    processAdvancedOrganicAndElectricalTraits(def, ctx, world);
}

void GenericImmovableSolid::checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    if (dur && dur->health <= 0 && !base->flags.isDead) {
        if (base->flags.isIgnited && def.transform_on_health_zero_ignited_result != 0) {
            if (Random::randFloat(0,1) < def.transform_on_health_zero_ignited_chance) {
                dieAndReplace(x, y, def.transform_on_health_zero_ignited_result, world);
                return;
            }
        }
        if (def.transform_on_health_zero_result != static_cast<MaterialID>(0)) {
            dieAndReplace(x, y, def.transform_on_health_zero_result, world);
        } else {
            die(x, y, world);
        }
    }
}

bool GenericImmovableSolid::receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) {
    if (def.immune_to_fire) return false;
    if (def.transform_on_max_temp_result != 0 && therm) {
        therm->temperature += heat;
        if (therm->temperature >= def.max_temp_threshold) {
            dieAndReplace(x, y, def.transform_on_max_temp_result, world);
            return true;
        }
    }
    if (def.transform_on_heat_result != 0 && heat > 0) {
        dieAndReplace(x, y, def.transform_on_heat_result, world);
        return true;
    }
    if (!def.has_thermal) return false; 
    return Particle::receiveHeat(base, therm, x, y, heat, world);
}

bool GenericImmovableSolid::corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) {
    if (def.immune_to_corrosion || def.has_trait_corrosive) return false; 
    return Particle::corrode(base, dur, x, y, damage, world);
}

bool GenericImmovableSolid::magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) {
    if (def.immune_to_magmatize || def.has_trait_magmatize) return false; 
    return Particle::magmatize(base, dur, x, y, damage, world);
}

bool GenericImmovableSolid::explode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int strength, ParticleWorld& world) {
    if (!dur) return false;
    if (dur->explosionResistance < strength) {
        die(x, y, world);
        return true;
    } else if (def.transform_on_crush_result != 0) {
        dieAndReplace(x, y, def.transform_on_crush_result, world);
        return true;
    }
    return false;
}

bool GenericImmovableSolid::receiveCharge(BaseComponent* base, int x, int y, ParticleWorld& world) {
    if (def.is_conductive && base && !base->flags.isCharged) {
        base->flags.isCharged = true;
        if (def.transform_on_charged_result != 0) {
            dieAndReplace(x, y, def.transform_on_charged_result, world);
        }
        return true;
    }
    return false;
}

void GenericImmovableSolid::takeEffectsDamage(BaseComponent* base, DurabilityComponent* dur, ThermalComponent* therm, int x, int y, ParticleWorld& world) {
    if (def.smolders) return;
    Particle::takeEffectsDamage(base, dur, therm, x, y, world);
}