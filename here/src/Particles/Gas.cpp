#include "Particles/Gas.hpp"
#include "Particles/Liquid.hpp" 
#include "Particles/Solid.hpp" 
#include "Constants.hpp"
#include "Random.hpp"
#include <algorithm> 
#include <cmath> 

// --- BASE GAS IMPLEMENTATION ---

Gas::Gas(MaterialID id, float buoy, float chaos) 
    : Particle(id), buoyancy(buoy), chaosLevel(chaos) {
}

void Gas::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);
    
    KinematicsComponent kin;
    kin.velocity = {0,0};
    kin.isFreeFalling = true;
    world.kinematicsManager.add(index, kin);

    FluidComponent fluid;
    fluid.density = 1;
    fluid.dispersionRate = 1;
    world.fluidManager.add(index, fluid);
}

void Gas::update(int x, int y, uint32_t index, float dt, ParticleWorld& world) 
{
    // 1. Visuals - Update based on the starting position
    world.updateParticleColor(index, x, y);

    // 2. Component Fetching
    auto* kin = world.kinematicsManager.get(index);
    if (!kin) return;

    // 3. Velocity Calculation
    // Buoyancy goes UP (negative Y), so we subtract from the gravity-based velocity
    kin->velocity.y = std::clamp(kin->velocity.y - (GRAVITY * dt * buoyancy), -5.0f, 2.0f);
    
    // Apply Chaos/Jitter
    kin->velocity.x += Random::randFloat(-chaosLevel, chaosLevel);
    kin->velocity.x = std::clamp(kin->velocity.x, -3.0f, 3.0f);
    
    // Random Turbulence
    if (Random::chance(5)) {
        kin->velocity.x += Random::randFloat(-1.0f, 1.0f);
        kin->velocity.y += Random::randFloat(-0.5f, 0.5f);
    }

    // 4. Movement Tracking
    // We track coordinates locally because 'index' becomes invalid the moment we move
    int currentX = x;
    int currentY = y;

    int targetX = x + static_cast<int>(std::round(kin->velocity.x));
    int targetY = y + static_cast<int>(std::round(kin->velocity.y));
    
    // Helper Lambda: Tries to move and updates our local trackers
    auto tryMove = [&](int tx, int ty) -> bool {
        if (!world.inBounds(tx, ty)) return false; 
        
        // We must re-fetch the index for our current position because it might have changed
        uint32_t currentIdx = world.getIndex(currentX, currentY);
        uint32_t targetIdx = world.getIndex(tx, ty);

        if (actOnNeighbor(tx, ty, currentIdx, targetIdx, world, true, true, 0)) {
            currentX = tx;
            currentY = ty;
            return true;
        }
        return false;
    };

    // Helper Lambda: Raycast to prevent passing through thin walls at high speed
    auto isPathBlocked = [&](int tx, int ty) -> bool {
        int dX = std::abs(tx - x);
        int dY = std::abs(ty - y);
        int sX = (x < tx) ? 1 : -1;
        int sY = (y < ty) ? 1 : -1;
        int err = dX - dY;
        
        int checkX = x;
        int checkY = y;

        while (true) {
            if (checkX == tx && checkY == ty) break;
            if (checkX != x || checkY != y) {
                BaseComponent* nb = world.baseManager.get(world.getIndex(checkX, checkY));
                if (nb) {
                    // Check logic registry safely
                    Particle* logic = Particle::GetRegistry()[static_cast<int>(nb->id)];
                    if (logic) {
                        MaterialGroup g = logic->getGroup();
                        if (g == MaterialGroup::ImmovableSolid || g == MaterialGroup::MovableSolid) {
                            return true;
                        }
                    }
                }
            }
            int e2 = 2 * err;
            if (e2 > -dY) { err -= dY; checkX += sX; }
            if (e2 < dX)  { err += dX; checkY += sY; }
        }
        return false;
    };

    // 5. Execution of Movement Logic
    bool moved = false;

    // A. Attempt long-distance jump (Velocity based)
    if (!isPathBlocked(targetX, targetY) && tryMove(targetX, targetY)) {
        moved = true;
    }
    // B. Fallback: Rise 1 pixel (Buoyancy)
    else if (tryMove(currentX, currentY - 1)) {
        // Re-fetch kin because moveParticle might have triggered a vector reallocation
        if (auto* k = world.kinematicsManager.get(world.getIndex(currentX, currentY))) {
            k->velocity.y *= 0.5f; 
        }
        moved = true;
    }
    // C. Fallback: Horizontal Drift / Ceiling sliding
    else {
        int driftDir = (kin->velocity.x > 0) ? 1 : -1;
        if (std::abs(kin->velocity.x) < 0.1f) driftDir = Random::randBool() ? 1 : -1;

        if (tryMove(currentX + driftDir, currentY)) {
            moved = true;
        } 
        else if (tryMove(currentX - driftDir, currentY)) {
            moved = true;
        }
        else if (tryMove(currentX + driftDir, currentY - 1)) {
             moved = true;
        }
        else if (tryMove(currentX - driftDir, currentY - 1)) {
             moved = true;
        }
    }

    // 6. Post-Movement Physics
    // Always re-fetch from manager using current position to avoid using stale pointers
    uint32_t finalIndex = world.getIndex(currentX, currentY);
    auto* finalKin = world.kinematicsManager.get(finalIndex);
    
    if (finalKin) {
        finalKin->velocity.x *= 0.8f;
        finalKin->velocity.y *= 0.9f;
    }

    // 7. Side Effects (Heat, Sparks, etc.)
    applyHeatToNeighborsIfIgnited(finalIndex, currentX, currentY, world);
    spawnSparkIfIgnited(finalIndex, currentX, currentY, world);
    checkLifeSpan(finalIndex, world);
    takeEffectsDamage(finalIndex, world);
}

bool Gas::actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, ParticleWorld& world, bool isFinal, bool isFirst, int depth) {
    
    if (!world.inBounds(targetX, targetY)) return false;
    
    if (!world.isEmpty(targetX, targetY)) {
        if (actOnOther(myIndex, targetIndex, world)) return true;
    }

    if (world.isEmpty(targetX, targetY)) {
        if (isFinal) {
            int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
            int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
            world.moveParticle(curX, curY, targetX, targetY);
            return true; 
        } 
        return false;
    }

    BaseComponent* neighborBase = world.baseManager.get(targetIndex);
    if (neighborBase) {
        Particle* logic = MaterialRegistry[static_cast<int>(neighborBase->id)];
        if (!logic) return false;

        if (logic->getGroup() == MaterialGroup::Gas) {
            if (compareGasDensities(myIndex, targetIndex, world)) {
                int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
                int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
                swapGasForDensities(world, myIndex, targetIndex, targetX, targetY, curX, curY);
                return true; 
            }
            return false;
        }
        else if (logic->getGroup() == MaterialGroup::Liquid) {
            int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
            int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
            world.swapParticles(curX, curY, targetX, targetY);
            return true;
        }
    }
    return false;
}

bool Gas::compareGasDensities(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) {
    auto* myF = world.fluidManager.get(myIndex);
    auto* otherF = world.fluidManager.get(otherIndex);
    
    int myY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
    int otherY = world.baseManager.denseToGrid[world.baseManager.sparse[otherIndex]] / world.getWidth();

    if (myF && otherF) {
        return (myF->density > otherF->density && otherY <= myY);
    }
    return false;
}

void Gas::swapGasForDensities(ParticleWorld& world, uint32_t myIndex, uint32_t otherIndex, int neighborX, int neighborY, int& currentX, int& currentY) {
    if (auto* kin = world.kinematicsManager.get(myIndex)) {
        kin->velocity.y = 2.0f;
    }
    world.swapParticles(currentX, currentY, neighborX, neighborY);
}

// --- SUBCLASS IMPLEMENTATIONS ---

// STEAM
void Steam::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);
    auto* fluid = world.fluidManager.get(index);
    fluid->density = 5;
    fluid->dispersionRate = 2;
    DurabilityComponent dur;
    dur.health = Random::randInt(1000, 3000); 
    world.durabilityManager.add(index, dur);
}

void Steam::checkLifeSpan(uint32_t index, ParticleWorld& world) {
    auto* dur = world.durabilityManager.get(index);
    if (dur && dur->health > 0) {
        dur->health--;
        if (dur->health <= 0) {
            if (Random::randFloat(0, 1) > 0.5f) {
                die(index, world);
            } else {
                int curX = world.baseManager.denseToGrid[world.baseManager.sparse[index]] % world.getWidth();
                int curY = world.baseManager.denseToGrid[world.baseManager.sparse[index]] / world.getWidth();
                dieAndReplace(index, curX, curY, MaterialID::Water, world);
            }
        }
    }
}

// FLAMMABLE GAS
void FlammableGas::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);
    auto* fluid = world.fluidManager.get(index);
    fluid->density = 1;
    fluid->dispersionRate = 2;
    ThermalComponent therm;
    therm.flammabilityResistance = 10;
    world.thermalManager.add(index, therm);
    DurabilityComponent dur;
    dur.health = Random::randInt(3000, 3500); 
    world.durabilityManager.add(index, dur);
}

// SPARK
void Spark::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);
    
    auto* base = world.baseManager.get(index);
    base->flags.isIgnited = true;

    auto* fluid = world.fluidManager.get(index);
    fluid->density = 4;
    fluid->dispersionRate = 4;

    ThermalComponent therm;
    therm.flammabilityResistance = 25;
    therm.temperature = 3;
    therm.heatFactor = 10; 
    world.thermalManager.add(index, therm);

    DurabilityComponent dur;
    dur.health = Random::randInt(0, 20); 
    world.durabilityManager.add(index, dur);
}

bool Spark::actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, ParticleWorld& world, bool isFinal, bool isFirst, int depth) {
    
    if (!world.inBounds(targetX, targetY)) return false;

    // Interaction Check
    if (!world.isEmpty(targetX, targetY)) {
        if (actOnOther(myIndex, targetIndex, world)) return true;
    }

    if (world.isEmpty(targetX, targetY)) {
        if (isFinal) {
            int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
            int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
            world.swapParticles(curX, curY, targetX, targetY);
        }
        return true; 
    }
    
    BaseComponent* nb = world.baseManager.get(targetIndex);
    if (!nb) return false;

    if (nb->id == MaterialID::Spark || nb->id == MaterialID::ExplosionSpark) {
        return false; 
    }
    else if (nb->id == MaterialID::Smoke) {
        world.removeParticle(targetIndex);
        return false;
    }
    else {
        // Ignite logic
        Particle* logic = MaterialRegistry[static_cast<int>(nb->id)];
        if (logic) {
            auto* therm = world.thermalManager.get(myIndex);
            int heat = therm ? therm->heatFactor : 10;
            logic->receiveHeat(targetIndex, heat, world);
            die(myIndex, world); // Spark dies on contact
            return true;
        }
    }
    return false;
}

// EXPLOSION SPARK
void ExplosionSpark::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);
    
    auto* base = world.baseManager.get(index);
    base->flags.isIgnited = true;

    auto* fluid = world.fluidManager.get(index);
    fluid->density = 4;
    fluid->dispersionRate = 4;
    
    ThermalComponent therm;
    therm.flammabilityResistance = 25;
    therm.temperature = 3;
    therm.heatFactor = 10;
    world.thermalManager.add(index, therm);

    DurabilityComponent dur;
    dur.health = Random::randInt(0, 20);
    world.durabilityManager.add(index, dur);
}

bool ExplosionSpark::actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, ParticleWorld& world, bool isFinal, bool isFirst, int depth) {
    // Duplicated logic from Spark to avoid inheritance issues
    if (!world.inBounds(targetX, targetY)) return false;
    
    if (!world.isEmpty(targetX, targetY)) {
        if (actOnOther(myIndex, targetIndex, world)) return true;
    }
    
    if (world.isEmpty(targetX, targetY)) {
        if (isFinal) {
            int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
            int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
            world.swapParticles(curX, curY, targetX, targetY);
        }
        return true; 
    }
    
    BaseComponent* nb = world.baseManager.get(targetIndex);
    if (!nb) return false;
    
    if (nb->id == MaterialID::Spark || nb->id == MaterialID::ExplosionSpark) { return false; }
    else if (nb->id == MaterialID::Smoke) { world.removeParticle(targetIndex); return false; }
    else {
        Particle* logic = MaterialRegistry[static_cast<int>(nb->id)];
        if (logic) {
            auto* therm = world.thermalManager.get(myIndex);
            int heat = therm ? therm->heatFactor : 10;
            logic->receiveHeat(targetIndex, heat, world);
            die(myIndex, world);
            return true;
        }
    }
    return false;
}

// SMOKE
void Smoke::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Gas::onSpawn(index, x, y, world);
    auto* fluid = world.fluidManager.get(index);
    fluid->density = 3;
    fluid->dispersionRate = 2;
    DurabilityComponent dur;
    dur.health = Random::randInt(450, 700);
    world.durabilityManager.add(index, dur);
}

// --- AUTO REGISTRATION ---
static Steam steam_instance;
static FlammableGas flammablegas_instance;
static Spark spark_instance;
static ExplosionSpark explosionspark_instance;
static Smoke smoke_instance;