#include "Particles/Liquid.hpp"
#include "ParticleWorld.hpp"
#include "Constants.hpp"
#include "Random.hpp"
#include <algorithm> 
#include <cmath> 

// Terminal velocity constants
static const float MAX_VEL_Y = 124.0f;
static const float BOUNCE_VEL_Y = 62.0f;

// --- LIQUID BASE IMPLEMENTATION ---

void Liquid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);

    // Default Kinematics
    KinematicsComponent kin;
    kin.velocity = {0.0f, 0.0f};
    kin.xThreshold = 0.0f;
    kin.yThreshold = 0.0f;
    kin.isFreeFalling = true;
    kin.stoppedMovingCount = 0;
    world.kinematicsManager.add(index, kin);

    // Default Fluid (Overridden by subclasses)
    FluidComponent fluid;
    fluid.density = 1;
    fluid.dispersionRate = 1;
    world.fluidManager.add(index, fluid);
}

void Liquid::update(int x, int y, uint32_t index, float dt, ParticleWorld& world) 
{
    // Retrieve Components
    auto* kin = world.kinematicsManager.get(index);
    auto* fluid = world.fluidManager.get(index);

    if (!kin || !fluid) return;
    
    // --- 1. Gravity & Friction ---
    kin->velocity.y += GRAVITY * dt;
    if (kin->velocity.y > MAX_VEL_Y) kin->velocity.y = MAX_VEL_Y;

    if (kin->isFreeFalling) {
        kin->velocity.x *= 0.8f; 
    }

    // --- 2. Calculate Thresholds ---
    kin->xThreshold += kin->velocity.x * dt;
    kin->yThreshold += kin->velocity.y * dt;

    int velXDeltaTime = static_cast<int>(kin->xThreshold);
    int velYDeltaTime = static_cast<int>(kin->yThreshold);

    kin->xThreshold -= velXDeltaTime;
    kin->yThreshold -= velYDeltaTime;

    int xModifier = (velXDeltaTime < 0) ? -1 : 1;
    int yModifier = (velYDeltaTime < 0) ? -1 : 1;

    int absX = std::abs(velXDeltaTime);
    int absY = std::abs(velYDeltaTime);

    // --- 3. Vector Pathing ---
    int upperBound = std::max(absX, absY);
    
    if (upperBound == 0) {
        // If not moving fast enough, check if we should be freefalling
        if (world.isEmpty(x, y + 1)) {
            kin->isFreeFalling = true; 
        } else {
            kin->velocity.x *= 0.5f;
        }
    }

    int currentX = x;
    int currentY = y;
    
    int minBound = std::min(absX, absY);
    float slope = (upperBound == 0) ? 0.0f : ((float)(minBound) / (upperBound));
    bool xDiffIsLarger = absX > absY;
    sf::Vector2i formerLocation(x, y);

    for (int i = 1; i <= upperBound; i++) {
        int smallerCount = (int)std::floor(i * slope);
        int xIncrease = xDiffIsLarger ? i : smallerCount;
        int yIncrease = xDiffIsLarger ? smallerCount : i;

        int modifiedMatrixX = x + (xIncrease * xModifier);
        int modifiedMatrixY = y + (yIncrease * yModifier);

        if (world.inBounds(modifiedMatrixX, modifiedMatrixY)) {
            if (modifiedMatrixX == currentX && modifiedMatrixY == currentY) continue;

            bool isFinal = (i == upperBound);
            bool isFirst = (i == 1);
            
            // Note: actOnNeighbor now takes indices
            uint32_t targetIdx = world.getIndex(modifiedMatrixX, modifiedMatrixY);
            
            bool stopped = actOnNeighbor(modifiedMatrixX, modifiedMatrixY, index, targetIdx, world, isFinal, isFirst, 0);
            
            // If we moved (index is now at modifiedMatrixX/Y), update current tracking
            if (!stopped) {
                // If the particle moved, we must break because 'index' now points to a new location in space
                // Actually, in our ECS move, the 'index' (grid index) changes.
                // If moveParticle was called, 'index' is invalid for the old X,Y.
                // We need to track currentX/currentY.
                // The actOnNeighbor logic handles the physical move and updates currentX/currentY refs.
            } else {
                break;
            }
        } else {
            // Out of bounds
            if (kin) kin->velocity.y = 0;
    break;
        }
    }

    // Side Effects
    world.updateParticleColor(index, currentX, currentY);
    applyHeatToNeighborsIfIgnited(index, currentX, currentY, world);
    spawnSparkIfIgnited(index, currentX, currentY, world);
    takeEffectsDamage(index, world);

    if (didNotMove(index, currentX, currentY, world)) { // Simplified check
        kin->stoppedMovingCount++;
    } else {
        kin->stoppedMovingCount = 0;
    }
    
    // Stopped threshold logic is usually 10 for liquids
    if (kin->stoppedMovingCount > 10) kin->stoppedMovingCount = 10;
}

bool Liquid::actOnNeighbor(int targetX, int targetY, uint32_t myIndex, uint32_t targetIndex, 
                           ParticleWorld& world, bool isFinal, bool isFirst, int depth) 
{
    // Retrieve our components
    auto* myKin = world.kinematicsManager.get(myIndex);
    auto* myFluid = world.fluidManager.get(myIndex);

    // 1. Interaction (actOnOther)
    // Note: targetIndex comes from getIndex, so it's a grid index.
    if (!world.isEmpty(targetX, targetY)) {
        if (this->actOnOther(myIndex, targetIndex, world)) return true; 
    }

    // 2. Empty Space
    if (world.isEmpty(targetX, targetY)) { 
        if (isFinal) {
            myKin->isFreeFalling = true;
            // Move logic handles component transfer
            int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
            int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
            world.moveParticle(curX, curY, targetX, targetY);
            return false; 
        } else {
            return false; 
        }
    }
    
    // 3. Liquid Interaction (Density Swap)
    BaseComponent* targetBase = world.baseManager.get(targetIndex);
    if (targetBase) {
        Particle* targetLogic = MaterialRegistry[static_cast<int>(targetBase->id)];
        
        if (targetLogic && targetLogic->getGroup() == MaterialGroup::Liquid) {
            auto* targetFluid = world.fluidManager.get(targetIndex);
            
            if (targetFluid && myFluid && myFluid->density > targetFluid->density) {
                if (isFinal) {
                    myKin->velocity.y = BOUNCE_VEL_Y; 
                    if (Random::randFloat(0,1) > 0.8f) myKin->velocity.x *= -1;
                    
                    int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
                    int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
                    world.swapParticles(curX, curY, targetX, targetY);
                    return true; 
                } else {
                    return false; 
                }
            }
        }
    }

    // 4. Blocked -> Dispersion
    if (depth > 0 || isFinal) return true;

    // Friction
    if (myKin->isFreeFalling) {
        float absY = std::max(std::abs(myKin->velocity.y) / 31.0f, 105.0f);
        myKin->velocity.x = (myKin->velocity.x < 0) ? -absY : absY;
    }

    // Normalize Velocity
    sf::Vector2f normVel = myKin->velocity;
    float len = std::sqrt(normVel.x*normVel.x + normVel.y*normVel.y);
    if (len != 0) normVel /= len;

    int additionalX = getAdditional(normVel.x);
    int dist = additionalX * (Random::randBool() ? myFluid->dispersionRate + 2 : myFluid->dispersionRate - 1);

    // Velocity Transfer with neighbor
    auto* targetKin = world.kinematicsManager.get(targetIndex);
    if (targetKin) {
        if (isFirst) {
            myKin->velocity.y = getAverageVelOrGravity(myKin->velocity.y, targetKin->velocity.y);
        } else {
            myKin->velocity.y = MAX_VEL_Y; 
        }
        targetKin->velocity.y = myKin->velocity.y;
    }
    
    myKin->velocity.x *= 1.0f; // Friction factor usually 1.0 for liquids unless defined otherwise

    int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
    int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();

    // A. Try Diagonal
    int diagX = curX + additionalX;
    int diagY = curY + 1; 
    if (world.inBounds(diagX, diagY)) {
        int tempX = curX, tempY = curY;
        if (!iterateToAdditional(world, diagX, diagY, dist, myIndex, tempX, tempY)) {
            myKin->isFreeFalling = true;
            return true; 
        }
    }

    // B. Try Adjacent
    int adjX = curX + additionalX;
    int adjY = curY; 
    if (world.inBounds(adjX, adjY)) {
        int tempX = curX, tempY = curY;
        if (iterateToAdditional(world, adjX, adjY, dist, myIndex, tempX, tempY)) {
             myKin->velocity.x *= -1; 
        } else {
            myKin->isFreeFalling = false;
            return true; 
        }
    }

    myKin->isFreeFalling = false;
    return true; 
}

bool Liquid::iterateToAdditional(ParticleWorld& world, int startX, int startY, int distance, uint32_t myIndex, int& currentX, int& currentY) 
{
    int distanceModifier = (distance > 0) ? 1 : -1;
    int absDist = std::abs(distance);
    
    // Need my Fluid data
    auto* myFluid = world.fluidManager.get(myIndex);
    auto* myKin = world.kinematicsManager.get(myIndex);

    int lastValidX = currentX;
    int lastValidY = currentY;
    
    for (int i = 0; i <= absDist; i++) {
        int modifiedX = startX + (i * distanceModifier);
        if (!world.inBounds(modifiedX, startY)) return true; 

        uint32_t neighborIdx = world.getIndex(modifiedX, startY);
        bool empty = world.isEmpty(modifiedX, startY);

        if (!empty && actOnOther(myIndex, neighborIdx, world)) return false; 

        bool isFinal = (i == absDist);

        if (empty) {
            if (isFinal) {
                world.moveParticle(currentX, currentY, modifiedX, startY);
                currentX = modifiedX;
                currentY = startY;
                return false; 
            }
            lastValidX = modifiedX;
            lastValidY = startY;
        } 
        else {
            // Check density swap
            BaseComponent* nb = world.baseManager.get(neighborIdx);
            Particle* logic = MaterialRegistry[static_cast<int>(nb->id)];
            
            if (logic && logic->getGroup() == MaterialGroup::Liquid) {
                auto* nf = world.fluidManager.get(neighborIdx);
                if (isFinal && myFluid && nf && myFluid->density > nf->density) {
                    world.swapParticles(currentX, currentY, modifiedX, startY);
                    currentX = modifiedX;
                    currentY = startY;
                    myKin->velocity.y = BOUNCE_VEL_Y;
                    if (Random::randFloat(0,1) > 0.8f) myKin->velocity.x *= -1;
                    return false;
                }
            } else {
                // Blocked
                if (i == 0) return true; 
                if (lastValidX != currentX || lastValidY != currentY) {
                    world.moveParticle(currentX, currentY, lastValidX, lastValidY);
                    currentX = lastValidX;
                    currentY = lastValidY;
                }
                return false;
            }
        }
    }
    return true; 
}

int Liquid::getAdditional(float val) {
    if (val < -0.1f) return (int)std::floor(val);
    if (val > 0.1f) return (int)std::ceil(val);
    return 0;
}

float Liquid::getAverageVelOrGravity(float myVel, float otherVel) {
    if (otherVel < (MAX_VEL_Y + 1.0f)) return MAX_VEL_Y;
    float avg = (myVel + otherVel) / 2.0f;
    if (avg < 0) return avg;
    return std::min(avg, MAX_VEL_Y);
}

// --- SUBCLASS IMPLEMENTATIONS ---

// WATER
void Water::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    
    auto* fluid = world.fluidManager.get(index);
    fluid->density = 5;
    fluid->dispersionRate = 5;

    // Water needs to track heat? Only if it converts to steam.
    // It doesn't usually burn, but it receives heat.
    // We can add thermal component to track "heat received" if we used temperature.
    // But receiveHeat logic in old code was instant conversion.
    ThermalComponent therm;
    therm.heatFactor = 0; // Doesn't give heat
    therm.flammabilityResistance = 100; // Not used
    therm.temperature = 0;
    world.thermalManager.add(index, therm); 
}

bool Water::receiveHeat(uint32_t index, int heat, ParticleWorld& world) {
    // Instant conversion to Steam
    int curX = world.baseManager.denseToGrid[world.baseManager.sparse[index]] % world.getWidth();
    int curY = world.baseManager.denseToGrid[world.baseManager.sparse[index]] / world.getWidth();
    dieAndReplace(index, curX, curY, MaterialID::Steam, world);
    return true; 
}

bool Water::actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) {
    // Logic: clean color, cool down neighbor
    auto* otherBase = world.baseManager.get(otherIndex);
    if (otherBase) {
        Particle* logic = MaterialRegistry[static_cast<int>(otherBase->id)];
        if (logic && logic->cleanColor(otherIndex, world)) {
            // stained
        }
        
        if (logic && logic->shouldApplyHeat(otherIndex, world)) {
            if (logic->receiveCooling(otherIndex, 5, world)) {
                 // Water boils?
                 // Simple logic: Water dies
                 int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
                 int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
                 dieAndReplace(myIndex, curX, curY, MaterialID::Steam, world);
                 return true;
            }
        }
    }
    return false;
}

bool Water::explode(uint32_t index, int strength, ParticleWorld& world) {
    // Water has 0 resistance
    int curX = world.baseManager.denseToGrid[world.baseManager.sparse[index]] % world.getWidth();
    int curY = world.baseManager.denseToGrid[world.baseManager.sparse[index]] / world.getWidth();
    dieAndReplace(index, curX, curY, MaterialID::Steam, world);
    return true;
}

// OIL
void Oil::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    auto* fluid = world.fluidManager.get(index);
    fluid->density = 4;
    fluid->dispersionRate = 4;

    ThermalComponent therm;
    therm.flammabilityResistance = 5;
    therm.heatFactor = 10;
    therm.fireDamage = 10;
    world.thermalManager.add(index, therm);

    DurabilityComponent dur;
    dur.health = 1000;
    world.durabilityManager.add(index, dur);
}

bool Oil::actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) {
    // If neighbor is ignited or lava, Oil ignites
    auto* otherBase = world.baseManager.get(otherIndex);
    if (otherBase) {
        if (otherBase->flags.isIgnited || otherBase->id == MaterialID::Lava) {
            receiveHeat(myIndex, 100, world); // Force ignite
        }
    }
    return false;
}

// LAVA
void Lava::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    
    auto* base = world.baseManager.get(index);
    base->flags.heated = true;

    auto* fluid = world.fluidManager.get(index);
    fluid->density = 10;
    fluid->dispersionRate = 1;

    ThermalComponent therm;
    therm.temperature = 10; 
    therm.heatFactor = 10;
    world.thermalManager.add(index, therm);

    DurabilityComponent dur;
    dur.health = 100; // "health" here acts as mass/cooling buffer?
    world.durabilityManager.add(index, dur);
}

void Lava::checkIfDead(uint32_t index, ParticleWorld& world) {
    auto* therm = world.thermalManager.get(index);
    if (therm && therm->temperature <= 0) {
        int curX = world.baseManager.denseToGrid[world.baseManager.sparse[index]] % world.getWidth();
        int curY = world.baseManager.denseToGrid[world.baseManager.sparse[index]] / world.getWidth();
        dieAndReplace(index, curX, curY, MaterialID::Stone, world);
        // Harden neighbors (logic omitted for brevity, would check neighbors and replace)
    }
}

bool Lava::receiveCooling(uint32_t index, int cooling, ParticleWorld& world) {
    auto* therm = world.thermalManager.get(index);
    if (therm) {
        therm->temperature -= cooling;
        return true;
    }
    return false;
}

bool Lava::actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) {
    Particle* otherLogic = MaterialRegistry[static_cast<int>(world.baseManager.get(otherIndex)->id)];
    if (otherLogic) {
        otherLogic->magmatize(otherIndex, Random::randInt(0, 10), world);
    }
    return false;
}

// ACID
void Acid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    auto* fluid = world.fluidManager.get(index);
    fluid->density = 2;
    fluid->dispersionRate = 2;

    // Use Durability as corrosion count
    DurabilityComponent dur;
    dur.health = 3; // Corrodes 3 things then dies
    world.durabilityManager.add(index, dur);
}

bool Acid::actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) {
    auto* otherBase = world.baseManager.get(otherIndex);
    if (otherBase) {
        Particle* logic = MaterialRegistry[static_cast<int>(otherBase->id)];
        
        logic->stain(otherIndex, sf::Color(0, 255, 0, 100), world);

        if (logic->corrode(otherIndex, world)) {
            auto* myDur = world.durabilityManager.get(myIndex);
            if (myDur) {
                myDur->health--;
                if (myDur->health <= 0) {
                     int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
                     int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
                     dieAndReplace(myIndex, curX, curY, MaterialID::FlammableGas, world);
                }
            }
            return true;
        }
    }
    return false;
}

// CEMENT
void Cement::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    auto* fluid = world.fluidManager.get(index);
    fluid->density = 9;
    fluid->dispersionRate = 1;
}

void Cement::update(int x, int y, uint32_t index, float dt, ParticleWorld& world) {
    Liquid::update(x, y, index, dt, world);
    
    // Check hardening
    auto* kin = world.kinematicsManager.get(index);
    if (kin && kin->stoppedMovingCount >= 50) {
        dieAndReplace(index, x, y, MaterialID::Stone, world);
    }
}

// BLOOD
void Blood::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    auto* fluid = world.fluidManager.get(index);
    fluid->density = 6;
    fluid->dispersionRate = 5;
}

bool Blood::actOnOther(uint32_t myIndex, uint32_t otherIndex, ParticleWorld& world) {
    auto* otherBase = world.baseManager.get(otherIndex);
    Particle* logic = MaterialRegistry[static_cast<int>(otherBase->id)];
    
    logic->stain(otherIndex, sf::Color(150, 0, 0), world);
    
    if (logic->shouldApplyHeat(otherIndex, world)) {
         int curX = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] % world.getWidth();
         int curY = world.baseManager.denseToGrid[world.baseManager.sparse[myIndex]] / world.getWidth();
         dieAndReplace(myIndex, curX, curY, MaterialID::Steam, world);
         return true;
    }
    return false;
}

// --- AUTO REGISTRATION ---
static Water water_instance;
static Oil oil_instance;
static Lava lava_instance;
static Acid acid_instance;
static Cement cement_instance;
static Blood blood_instance;