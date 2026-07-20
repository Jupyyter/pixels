#include "Particles/Gas.hpp"
#include "ParticleWorld.hpp"
#include <algorithm> 
#include <cmath> 

Gas::Gas(MaterialID id, float buoy, float chaos) 
    : Particle(id), buoyancy(buoy), chaosLevel(chaos) {}

void Gas::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);
    world.add<KinematicsComponent>(x, y, KinematicsComponent(sf::Vector2f(0, 0), 0.0f, 0.0f, true, 0));
    world.add<FluidComponent>(x, y, FluidComponent(1, 1)); 
}

void Gas::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    auto* kin = ctx.kinematics ? &ctx.kinematics[ctx.index] : world.get<KinematicsComponent>(ctx.x, ctx.y);
    if (!kin) return;

    kin->velocity.y = std::clamp(kin->velocity.y - (GRAVITY * dt * buoyancy), -5.0f, 2.0f);
    kin->velocity.x += Random::randFloat(-chaosLevel, chaosLevel);
    kin->velocity.x = std::clamp(kin->velocity.x, -3.0f, 3.0f);
    
    if (Random::randInt(0, 100) < 5) {
        kin->velocity.x += Random::randFloat(-1.0f, 1.0f);
        kin->velocity.y += Random::randFloat(-0.5f, 0.5f);
    }

    int curX = ctx.x, curY = ctx.y;
    int targetX = ctx.x + static_cast<int>(std::round(kin->velocity.x));
    int targetY = ctx.y + static_cast<int>(std::round(kin->velocity.y));
    
    auto tryMove = [&](int tx, int ty) -> bool {
        return !actOnNeighbor(ctx, tx, ty, curX, curY, world, true, true, 0);
    };

    auto isPathBlocked = [&](int tx, int ty) -> bool {
        int dX = std::abs(tx - ctx.x), dY = std::abs(ty - ctx.y);
        int sX = (ctx.x < tx) ? 1 : -1, sY = (ctx.y < ty) ? 1 : -1;
        int err = dX - dY, checkX = ctx.x, checkY = ctx.y;

        while (checkX != tx || checkY != ty) {
            if (checkX != ctx.x || checkY != ctx.y) {
                if (auto* nb = world.getFast<BaseComponent>(ctx, checkX, checkY)) {
                    Particle* logic = MaterialRegistry[static_cast<int>(nb->id)];
                    if (logic && (logic->getGroup() == MaterialGroup::ImmovableSolid || 
                                  logic->getGroup() == MaterialGroup::MovableSolid)) return true;
                }
            }
            int e2 = 2 * err;
            if (e2 > -dY) { err -= dY; checkX += sX; }
            if (e2 < dX)  { err += dX; checkY += sY; }
        }
        return false;
    };

    if (!isPathBlocked(targetX, targetY) && tryMove(targetX, targetY)) {}
    else if (tryMove(curX, curY - 1)) {
        if (auto* k = world.getFast<KinematicsComponent>(ctx, curX, curY)) k->velocity.y *= 0.5f;
    }
    else {
        int drift = (kin->velocity.x > 0) ? 1 : -1;
        if (std::abs(kin->velocity.x) < 0.1f) drift = Random::randBool() ? 1 : -1;
        if (!tryMove(curX + drift, curY)) {
            if (!tryMove(curX - drift, curY)) {
                if (!tryMove(curX + drift, curY - 1)) tryMove(curX - drift, curY - 1);
            }
        }
    }

    auto* finalBase = world.getFast<BaseComponent>(ctx, curX, curY);
    if (!finalBase || finalBase->compMask == 0) return;

    if (auto* fKin = world.getFast<KinematicsComponent>(ctx, curX, curY)) {
        fKin->velocity.x *= 0.8f; fKin->velocity.y *= 0.9f;
        if (std::abs(fKin->velocity.x) > 0.2f || std::abs(fKin->velocity.y) > 0.2f) {
            world.wakeParticle(curX, curY);
        }
    }

    world.updateParticleColor(world.computeIndex(curX, curY), curX, curY);
    
    if (finalBase->flags.isIgnited) {
        auto* therm = world.getFast<ThermalComponent>(ctx, curX, curY);
        applyHeatToNeighborsIfIgnited(finalBase, therm, curX, curY, world);
    }

    auto* dur = world.getFast<DurabilityComponent>(ctx, curX, curY);
    checkLifeSpan(finalBase, dur, curX, curY, world);

    if (finalBase->compMask != 0) {
        auto* therm = world.getFast<ThermalComponent>(ctx, curX, curY);
        takeEffectsDamage(finalBase, dur, therm, curX, curY, world);
    }
}

bool Gas::actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, ParticleWorld& world, bool isFinal, bool isFirst, int depth) {
    if (!world.inBounds(targetX, targetY)) return true;
    
    BaseComponent* targetBase = world.getFast<BaseComponent>(ctx, targetX, targetY);
    BaseComponent* myBase = world.getFast<BaseComponent>(ctx, myX, myY);
    
    if (!myBase) return true;

    if (targetBase && targetBase->compMask != 0) {
        if (actOnOther(myBase, myX, myY, targetBase, targetX, targetY, world)) return true;
        if (targetBase->compMask == 0) targetBase = nullptr; 
    }

    if (!targetBase || targetBase->compMask == 0) {
        if (isFinal) {
            world.moveParticle(myX, myY, targetX, targetY);
            myX = targetX; myY = targetY;
        } 
        return false;
    }

    Particle* logic = MaterialRegistry[static_cast<int>(targetBase->id)];
    if (!logic) return true;

    if (logic->getGroup() == MaterialGroup::Gas) {
        if (compareGasDensities(ctx, myX, myY, targetX, targetY, world)) {
            swapGasForDensities(ctx, world, myX, myY, targetX, targetY);
            return false; 
        }
        else if (Random::randFloat(0,1) > 0.8f) {
            world.swapParticles(myX, myY, targetX, targetY);
            myX = targetX; myY = targetY;
            return false;
        }
    }
    else if (logic->getGroup() == MaterialGroup::Liquid) {
        world.swapParticles(myX, myY, targetX, targetY);
        myX = targetX; myY = targetY;
        return false;
    }
    
    return true;
}

bool Gas::compareGasDensities(const ParticleContext& ctx, int myX, int myY, int otherX, int otherY, ParticleWorld& world) {
    auto* myF = world.getFast<FluidComponent>(ctx, myX, myY);
    auto* otherF = world.getFast<FluidComponent>(ctx, otherX, otherY);
    return (myF && otherF && myF->density > otherF->density && otherY <= myY);
}

void Gas::swapGasForDensities(const ParticleContext& ctx, ParticleWorld& world, int& myX, int& myY, int targetX, int targetY) {
    if (auto* kin = world.getFast<KinematicsComponent>(ctx, myX, myY)) kin->velocity.y = 2.0f;
    world.swapParticles(myX, myY, targetX, targetY);
    myX = targetX; myY = targetY;
}

GenericGas::GenericGas(MaterialID id, const ParticleDef& definition)
    : Gas(id, definition.gas_buoyancy, definition.gas_chaos), def(definition) {}

void GenericGas::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);

    if (auto* base = world.get<BaseComponent>(x, y)) {
        base->color = def.getRandomColor();
        if (def.ignite_on_spawn) base->flags.isIgnited = true;
        if (def.heated_on_spawn) base->flags.heated = true;
    }

    if (def.has_fluid) {
        if (auto* fluid = world.get<FluidComponent>(x, y)) {
            fluid->density = def.fluid_density;
            fluid->dispersionRate = def.fluid_dispersion;
        }
    }
    if (def.has_durability) {
        int hp = (def.dur_health_max > def.dur_health) ? Random::randInt(def.dur_health, def.dur_health_max) : def.dur_health;
        world.add<DurabilityComponent>(x, y, DurabilityComponent(hp, def.dur_expRes));
    }
    if (def.has_thermal) {
        world.add<ThermalComponent>(x, y, ThermalComponent(def.therm_temp, def.therm_flamRes, def.therm_heat, def.therm_fireDmg));
    }
}

void GenericGas::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    if (def.viscosity > 1 && (world.getFrameCounter() % def.viscosity) != 0) return;
    Gas::update(ctx, dt, world);
    processAdvancedOrganicAndElectricalTraits(def, ctx, world);
}

void GenericGas::checkLifeSpan(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    if (!dur) return;

    if (base && base->flags.isIgnited && def.decay_rate_ignited > 0) {
        dur->health -= def.decay_rate_ignited;
    } else if (def.decay_rate > 0) {
        dur->health -= def.decay_rate;
    }

    if (dur->health <= 0) {
        if (def.transform_on_health_zero_result != static_cast<MaterialID>(0)) {
            if (Random::randFloat(0, 1) > 0.5f) {
                dieAndReplace(x, y, def.transform_on_health_zero_result, world);
            } else {
                die(x, y, world);
            }
        } else {
            die(x, y, world);
        }
    }
}

bool GenericGas::actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) {
    if (executeGenericTraitsAndInteractions(def, myBase, myX, myY, otherBase, otherX, otherY, world)) return true;
    return Gas::actOnOther(myBase, myX, myY, otherBase, otherX, otherY, world);
}

bool GenericGas::receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) {
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

bool GenericGas::corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) {
    if (def.immune_to_corrosion || def.has_trait_corrosive) return false;
    return Particle::corrode(base, dur, x, y, damage, world);
}

bool GenericGas::explode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int strength, ParticleWorld& world) {
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

bool GenericGas::receiveCharge(BaseComponent* base, int x, int y, ParticleWorld& world) {
    if (def.is_conductive && base && !base->flags.isCharged) {
        base->flags.isCharged = true;
        if (def.transform_on_charged_result != 0) {
            dieAndReplace(x, y, def.transform_on_charged_result, world);
        }
        return true;
    }
    return false;
}

void GenericGas::takeEffectsDamage(BaseComponent* base, DurabilityComponent* dur, ThermalComponent* therm, int x, int y, ParticleWorld& world) {
    if (def.smolders) return;
    Particle::takeEffectsDamage(base, dur, therm, x, y, world);
}