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

void Gas::update(const ParticleContext& ctx, float dt, ParticleWorld& world) 
{
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

// --- SUBCLASSES ---

void Steam::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);
    if (auto* f = world.get<FluidComponent>(x, y)) f->density = 5;
    
    // Decreased significantly allowing immediate transformations naturally quicker 
    world.add<DurabilityComponent>(x, y, DurabilityComponent(Random::randInt(200, 500), 0));
}

void Steam::checkLifeSpan(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    if (dur && --dur->health <= 0) {
        if (Random::randFloat(0, 1) > 0.5f) {
            die(x, y, world);
        } else {
            dieAndReplace(x, y, MaterialID::Water, world);
        }
    }
}

void FlammableGas::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);
    // Explicit extreme thermal propagation causing explosions optimally
    world.add<ThermalComponent>(x, y, ThermalComponent(0, 100, 30, 2));
    
    // Default Lifespan threshold for standard dispersion limitations allowing visual flair structurally  
    world.add<DurabilityComponent>(x, y, DurabilityComponent(Random::randInt(40, 150), 0));
}

void FlammableGas::checkLifeSpan(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    if (!dur) return;

    // Fast Flame consumption tracker elegantly evaporating exactly
    if (base && base->flags.isIgnited) {
        dur->health -= 2;

        if (dur->health <= 0) {
            // Provides dynamic satisfying popping during detonations occasionally purely cleanly effectively properly accurately gracefully
            if (Random::randFloat(0, 1) > 0.7f) {
                dieAndReplace(x, y, MaterialID::Smoke, world);
            } else {
                die(x, y, world); // Pure clean flame vapor disappear completely functionally beautifully intuitively stably effectively naturally
            }
        }
    }
}


void Spark::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);
    if (auto* b = world.get<BaseComponent>(x, y)) b->flags.isIgnited = true;
    world.add<ThermalComponent>(x, y, ThermalComponent(3, 25, 10, 1));
    world.add<DurabilityComponent>(x, y, DurabilityComponent(Random::randInt(5, 30), 0));
}

bool Spark::actOnNeighbor(const ParticleContext& ctx, int tx, int ty, int& mx, int& my, ParticleWorld& world, bool isFinal, bool isFirst, int depth) {
    if (!world.inBounds(tx, ty)) return true;

    BaseComponent* targetBase = world.getFast<BaseComponent>(ctx, tx, ty);
    if (!targetBase || targetBase->compMask == 0) {
        if (isFinal) { world.moveParticle(mx, my, tx, ty); mx = tx; my = ty; }
        return false; 
    }

    if (targetBase->id == MaterialID::Spark || targetBase->id == MaterialID::ExplosionSpark) return true;
    if (targetBase->id == MaterialID::Smoke) { world.removeParticle(tx, ty); return false; }
    
    if (auto* logic = MaterialRegistry[static_cast<int>(targetBase->id)]) {
        auto* tTherm = world.get<ThermalComponent>(tx, ty);
        logic->receiveHeat(targetBase, tTherm, tx, ty, 10, world);
        
        if (logic->getGroup() != MaterialGroup::Gas) {
            die(mx, my, world);
            return true;
        }
    }
    return true;
}

void ExplosionSpark::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);
    if (auto* b = world.get<BaseComponent>(x, y)) b->flags.isIgnited = true;
    world.add<ThermalComponent>(x, y, ThermalComponent(3, 25, 10, 1));
    world.add<DurabilityComponent>(x, y, DurabilityComponent(Random::randInt(5, 30), 0));
}

bool ExplosionSpark::actOnNeighbor(const ParticleContext& ctx, int tx, int ty, int& mx, int& my, ParticleWorld& world, bool isFinal, bool isFirst, int depth) {
    if (!world.inBounds(tx, ty)) return true;

    BaseComponent* targetBase = world.getFast<BaseComponent>(ctx, tx, ty);
    if (!targetBase || targetBase->compMask == 0) {
        if (isFinal) { world.moveParticle(mx, my, tx, ty); mx = tx; my = ty; }
        return false; 
    }

    if (targetBase->id == MaterialID::Spark || targetBase->id == MaterialID::ExplosionSpark) return true;
    
    if (auto* logic = MaterialRegistry[static_cast<int>(targetBase->id)]) {
        auto* tTherm = world.get<ThermalComponent>(tx, ty);
        logic->receiveHeat(targetBase, tTherm, tx, ty, 10, world);
        if (logic->getGroup() != MaterialGroup::Gas) {
            die(mx, my, world);
            return true;
        }
    }
    return true;
}

void Smoke::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);
    if (auto* f = world.get<FluidComponent>(x, y)) f->density = 3;
    
    // Rapid expiration allows for clear sight cleanly 
    world.add<DurabilityComponent>(x, y, DurabilityComponent(Random::randInt(100, 250), 0));
}

void Smoke::checkLifeSpan(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    // Simply fades implicitly organically correctly
    if (dur && --dur->health <= 0) {
        die(x, y, world); 
    }
}

// --- GLOBAL STATIC INSTANCES ---
static Steam steam_instance;
static FlammableGas flammablegas_instance;
static Spark spark_instance;
static ExplosionSpark explosionspark_instance;
static Smoke smoke_instance;