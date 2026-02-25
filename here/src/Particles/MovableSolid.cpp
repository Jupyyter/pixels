#include "Particles/MovableSolid.hpp"
#include "ParticleWorld.hpp"
#include <cmath>
#include <algorithm>

// Terminal velocity (Positive for Down)
static const float MAX_VEL_Y = 124.0f;

// --- MOVABLE SOLID BASE IMPLEMENTATION ---

void MovableSolid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);

    // 1. Default Kinematics
    KinematicsComponent kin;
    kin.velocity = {0.0f, 124.0f}; // Default falling speed
    kin.xThreshold = 0.0f;
    kin.yThreshold = 0.0f;
    kin.isFreeFalling = true;
    kin.stoppedMovingCount = 0;
    world.kinematicsManager.add(index, kin);

    // 2. Default Durability (From old Particle.hpp base: health = 500)
    DurabilityComponent dur;
    dur.health = 500;
    dur.explosionResistance = 1; // Default resistance
    world.durabilityManager.add(index, dur);
}

void MovableSolid::update(int x, int y, uint32_t index, float dt, ParticleWorld& world) {
    auto* kin = world.kinematicsManager.get(index);
    if (!kin) return;

    // 1. Gravity
    kin->velocity.y += (GRAVITY * dt);
    
    // Cap falling speed
    if (kin->velocity.y > MAX_VEL_Y) kin->velocity.y = MAX_VEL_Y;
    
    if (kin->isFreeFalling) kin->velocity.x *= 0.9f;

    // 2. Thresholds
    kin->xThreshold += std::abs(kin->velocity.x * dt);
    kin->yThreshold += std::abs(kin->velocity.y * dt);

    int velXInt = static_cast<int>(kin->xThreshold);
    int velYInt = static_cast<int>(kin->yThreshold);

    kin->xThreshold -= static_cast<float>(velXInt);
    kin->yThreshold -= static_cast<float>(velYInt);

    int xMod = (kin->velocity.x < 0) ? -1 : 1;
    int yMod = (kin->velocity.y < 0) ? -1 : 1;

    // 3. Bresenham Vector Pathing
    int upperBound = std::max(velXInt, velYInt);
    
    if (upperBound == 0) {
        if (world.isEmpty(x, y + 1)) {
            kin->isFreeFalling = true; 
        }
    }
    
    int currentX = x;
    int currentY = y;

    int minBound = std::min(velXInt, velYInt);
    float slope = (upperBound == 0) ? 0.0f : ((float)(minBound) / (upperBound));
    bool xDiffIsLarger = velXInt > velYInt;
    sf::Vector2i formerLocation = {x, y};

    for (int i = 1; i <= upperBound; i++) {
        int smallerCount = (int)std::floor(i * slope);
        int xInc = xDiffIsLarger ? i : smallerCount;
        int yInc = xDiffIsLarger ? smallerCount : i;

        int targetX = x + (xInc * xMod);
        int targetY = y + (yInc * yMod);

        if (world.inBounds(targetX, targetY)) {
            if (targetX == currentX && targetY == currentY) continue;

            bool isFinal = (i == upperBound);
            bool isFirst = (i == 1);
            
            uint32_t targetIdx = world.getIndex(targetX, targetY);
            bool stopped = actOnNeighbor(targetX, targetY, index, targetIdx, world, isFinal, isFirst, 0);
            
            if (stopped) break; 
            
            currentX = targetX;
            currentY = targetY;
        } else {
            if (kin) kin->velocity = {0, 0}; 
            break; 
        }
    }

    uint32_t newIndex = world.getIndex(currentX, currentY); 
    
    applyHeatToNeighborsIfIgnited(newIndex, currentX, currentY, world);
    world.updateParticleColor(newIndex, currentX, currentY);
    takeEffectsDamage(newIndex, world);
    spawnSparkIfIgnited(newIndex, currentX, currentY, world);

    auto* finalKin = world.kinematicsManager.get(newIndex);
    if (finalKin) {
        if (currentX == formerLocation.x && currentY == formerLocation.y) {
            finalKin->stoppedMovingCount++;
            if (finalKin->stoppedMovingCount > 5) finalKin->stoppedMovingCount = 5;
        } else {
            finalKin->stoppedMovingCount = 0;
        }
    }
}

bool MovableSolid::actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, 
                                 ParticleWorld& world, bool isFinal, bool isFirst, int depth) 
{
    auto* myKin = world.kinematicsManager.get(myIndex);

    if (!world.isEmpty(targetX, targetY)) {
        if (this->actOnOther(myIndex, targetIndex, world)) return true;
    }

    if (world.isEmpty(targetX, targetY)) {
        int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
        int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
        
        setAdjacentNeighborsFreeFalling(curX, curY, world, depth);
        
        if (isFinal) {
            myKin->isFreeFalling = true;
            world.moveParticle(curX, curY, targetX, targetY);
        }
        return false; 
    }

    BaseComponent* targetBase = world.baseManager.get(targetIndex);
    if (targetBase) {
        Particle* logic = Particle::GetRegistry()[static_cast<int>(targetBase->id)];
        if (logic && logic->getGroup() == MaterialGroup::Liquid) {
            myKin->isFreeFalling = true;
            int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
            int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
            world.swapParticles(curX, curY, targetX, targetY);
            return true; 
        }
    }

    if (depth > 0 || isFinal) return true;

    if (myKin->isFreeFalling) {
        float absY = std::max(std::abs(myKin->velocity.y) / 31.0f, 105.0f);
        myKin->velocity.x = (myKin->velocity.x < 0) ? -absY : absY;
    }

    sf::Vector2f normVel = myKin->velocity;
    float len = std::sqrt(normVel.x*normVel.x + normVel.y*normVel.y);
    if (len != 0) normVel /= len;

    int addX = getAdditional(normVel.x);

    auto* targetKin = world.kinematicsManager.get(targetIndex);
    if (targetKin) {
        if (isFirst) {
            myKin->velocity.y = getAverageVelOrGravity(myKin->velocity.y, targetKin->velocity.y);
        } else {
            myKin->velocity.y = 124.0f; 
        }
        targetKin->velocity.y = myKin->velocity.y;
        myKin->velocity.x *= 0.25f; // Combined friction approximation
    }

    int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
    int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();

    int diagX = curX + addX;
    int diagY = curY + 1; 
    if (world.inBounds(diagX, diagY)) {
        uint32_t diagIdx = world.getIndex(diagX, diagY);
        bool stoppedDiag = actOnNeighbor(diagX, diagY, myIndex, diagIdx, world, true, false, depth + 1);
        if (!stoppedDiag) {
            myKin->isFreeFalling = true;
            return true; 
        }
    }

    int adjX = curX + addX;
    if (world.inBounds(adjX, curY)) {
        uint32_t adjIdx = world.getIndex(adjX, curY);
        bool stoppedAdj = actOnNeighbor(adjX, curY, myIndex, adjIdx, world, true, false, depth + 1);
        if (stoppedAdj) {
            myKin->velocity.x *= -1; 
        } else {
            myKin->isFreeFalling = false;
            return true;
        }
    }

    myKin->isFreeFalling = false;
    return true; 
}

void MovableSolid::setAdjacentNeighborsFreeFalling(int x, int y, ParticleWorld& world, int depth) {
    if (depth > 0) return;
    int checks[2] = {1, -1};
    for (int dx : checks) {
        if (world.inBounds(x + dx, y)) {
            uint32_t nIdx = world.getIndex(x + dx, y);
            auto* kin = world.kinematicsManager.get(nIdx);
            if (kin && Random::randFloat(0, 1) > 0.1f) { 
                kin->isFreeFalling = true;
            }
        }
    }
}

int MovableSolid::getAdditional(float val) {
    if (val < -0.1f) return -1;
    if (val > 0.1f) return 1;
    return 0;
}

float MovableSolid::getAverageVelOrGravity(float myVel, float otherVel) {
    if (otherVel < 125.0f) return 124.0f;
    float avg = (myVel + otherVel) / 2.0f;
    return (avg < 0) ? avg : std::min(avg, 124.0f);
}

// --- SUBCLASS IMPLEMENTATIONS ---

void Sand::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
    auto* kin = world.kinematicsManager.get(index);
    if(kin) kin->velocity.x = (Random::randBool()) ? -1.0f : 1.0f;
}

void Dirt::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
}

void Coal::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
    ThermalComponent therm;
    therm.flammabilityResistance = 100;
    therm.heatFactor = 10;
    therm.fireDamage = 1;
    world.thermalManager.add(index, therm);
}

void Coal::spawnSparkIfIgnited(uint32_t index, int x, int y, ParticleWorld& world) {
    if (Random::randInt(0, 20) > 2) return;
    Particle::spawnSparkIfIgnited(index, x, y, world); 
}

void Gunpowder::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
    
    ThermalComponent therm;
    therm.flammabilityResistance = 10;
    therm.heatFactor = 10;
    world.thermalManager.add(index, therm);
    
    // Overwrite default health to act as ignited timer (starts at 0)
    if (auto* dur = world.durabilityManager.get(index)) {
        dur->health = 0;
    }
}

void Gunpowder::update(int x, int y, uint32_t index, float dt, ParticleWorld& world) {
    MovableSolid::update(x, y, index, dt, world);
    auto* base = world.baseManager.get(index);
    auto* dur = world.durabilityManager.get(index); 

    if (base && base->flags.isIgnited && dur) {
        dur->health++; 
        if (dur->health >= 7) { 
             world.triggerExplosion(x, y, 15, 10);
             die(index, world);
        }
    }
}

void Snow::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
    auto* kin = world.kinematicsManager.get(index);
    if(kin) kin->velocity.y = 62.0f; 
    
    ThermalComponent therm;
    therm.flammabilityResistance = 100; 
    world.thermalManager.add(index, therm);
}

void Snow::update(int x, int y, uint32_t index, float dt, ParticleWorld& world) {
    MovableSolid::update(x, y, index, dt, world);
    auto* kin = world.kinematicsManager.get(index);
    if (kin && kin->velocity.y > 62.0f) { 
        kin->velocity.y = (Random::randFloat(0,1) > 0.3f) ? 62.0f : 124.0f;
    }
}

bool Snow::receiveHeat(uint32_t index, int heat, ParticleWorld& world) {
    if (heat > 0) {
        int curX = world.baseManager.denseToGrid[world.baseManager.sparse[index]] % world.getWidth();
        int curY = world.baseManager.denseToGrid[world.baseManager.sparse[index]] / world.getWidth();
        dieAndReplace(index, curX, curY, MaterialID::Water, world);
        return true;
    }
    return false;
}

void Ember::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
    auto* base = world.baseManager.get(index);
    if (base) base->flags.isIgnited = true;

    // Overwrite default health with random lifespan (250-350 frames)
    if (auto* dur = world.durabilityManager.get(index)) {
        dur->health = Random::randInt(250, 350); 
    }
    
    ThermalComponent therm;
    therm.temperature = 5;
    therm.heatFactor = 10;
    world.thermalManager.add(index, therm);
}

void Salt::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
}

// --- AUTO REGISTRATION ---
static Sand sand_instance;
static Dirt dirt_instance;
static Coal coal_instance;
static Gunpowder gunpowder_instance;
static Snow snow_instance;
static Ember ember_instance;
static Salt salt_instance;