#include "Particles/Liquid.hpp"
#include "ParticleWorld.hpp"
#include "Constants.hpp"
#include "Random.hpp"
#include <algorithm> 
#include <cmath> 

static const float MAX_VEL_Y = 124.0f;
static const float BOUNCE_VEL_Y = 62.0f;

// --- LIQUID BASE IMPLEMENTATION ---

void Liquid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);

    world.add<KinematicsComponent>(x, y, KinematicsComponent(
        sf::Vector2f(0.0f, 0.0f), 0.0f, 0.0f, true, 0
    ));
    world.add<FluidComponent>(x, y, FluidComponent(1, 1));
}

void Liquid::update(const ParticleContext& ctx, float dt, ParticleWorld& world) 
{
    if (!ctx.kinematics) return;
    auto* kin = &ctx.kinematics[ctx.index];
    
    int myDensity = 1;
    int myDispersionRate = 1;
    if (ctx.fluid) {
        myDensity = ctx.fluid[ctx.index].density;
        myDispersionRate = ctx.fluid[ctx.index].dispersionRate;
    }
    
    kin->velocity.y += GRAVITY * dt;
    if (kin->velocity.y > MAX_VEL_Y) kin->velocity.y = MAX_VEL_Y;

    if (kin->isFreeFalling) kin->velocity.x *= 0.8f; 

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
    int upperBound = std::max(absX, absY);
    
    // Check if we SHOULD be falling
    if (upperBound == 0) {
        if (world.isEmptyFast(ctx, ctx.x, ctx.y + 1)) {
            kin->isFreeFalling = true; 
        } else {
            kin->velocity.x *= 0.5f;
        }
    }

    int currentX = ctx.x;
    int currentY = ctx.y;
    int minBound = std::min(absX, absY);
    float slope = (upperBound == 0) ? 0.0f : ((float)(minBound) / (upperBound));
    bool xDiffIsLarger = absX > absY;

    for (int i = 1; i <= upperBound; i++) {
        int smallerCount = (int)std::floor(i * slope);
        int xIncrease = xDiffIsLarger ? i : smallerCount;
        int yIncrease = xDiffIsLarger ? smallerCount : i;
        int modifiedMatrixX = ctx.x + (xIncrease * xModifier);
        int modifiedMatrixY = ctx.y + (yIncrease * yModifier);

        if (world.inBounds(modifiedMatrixX, modifiedMatrixY)) {
            if (modifiedMatrixX == currentX && modifiedMatrixY == currentY) continue;
            if (actOnNeighbor(ctx, modifiedMatrixX, modifiedMatrixY, currentX, currentY, world, (i == upperBound), (i == 1), 0, myDensity, myDispersionRate)) break;
        } else {
            kin = world.getFast<KinematicsComponent>(ctx, currentX, currentY);
            if (kin) kin->velocity.y = 0;
            break;
        }
    }

    uint32_t finalIdx = world.computeIndex(currentX, currentY);
    world.updateParticleColor(finalIdx, currentX, currentY);

    auto* base = world.getFast<BaseComponent>(ctx, currentX, currentY);
    if (base) {
        if (base->flags.isIgnited) {
            auto* therm = world.getFast<ThermalComponent>(ctx, currentX, currentY);
            applyHeatToNeighborsIfIgnited(base, therm, currentX, currentY, world);
            spawnSparkIfIgnited(base, currentX, currentY, world);
        }
        takeEffectsDamage(base, world.getFast<DurabilityComponent>(ctx, currentX, currentY), world.getFast<ThermalComponent>(ctx, currentX, currentY), currentX, currentY, world);
    }

    kin = world.getFast<KinematicsComponent>(ctx, currentX, currentY);
    if (kin) {
        if (currentX == ctx.x && currentY == ctx.y) kin->stoppedMovingCount++;
        else kin->stoppedMovingCount = 0;
        
        // REFINED SLEEP LOGIC:
        // Wake the particle if it's in the air (isFreeFalling) OR if it just recently moved.
        // This ensures "bumps" fall once the gap below them is cleared.
        if (kin->isFreeFalling || kin->stoppedMovingCount < 5) {
           world.wakeParticle(currentX, currentY);
        }
    }
}

bool Liquid::actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, 
                           ParticleWorld& world, bool isFinal, bool isFirst, int depth, int myDensity, int myDispersionRate) 
{
    // Fetch our base FIRST so we can pass it down
    auto* myBase = world.getFast<BaseComponent>(ctx, myX, myY);
    if (!myBase || myBase->compMask == 0) return true; // Particle was destroyed!

    // Fast empty check bypassing hashmap
    BaseComponent* targetBase = world.getFast<BaseComponent>(ctx, targetX, targetY);
    bool targetEmpty = (!targetBase || targetBase->compMask == 0);

    // 1. Interaction (actOnOther)
    if (!targetEmpty) {
        if (this->actOnOther(myBase, myX, myY, targetBase, targetX, targetY, world)) return true; 
    }

    // 2. Pointer Paranoia Fix
    if (myBase->compMask == 0) return true; 
    
    auto* myKin = world.getFast<KinematicsComponent>(ctx, myX, myY);
    if (!myKin) return true; 

    // Re-check target in case actOnOther changed it
    targetBase = world.getFast<BaseComponent>(ctx, targetX, targetY);
    targetEmpty = (!targetBase || targetBase->compMask == 0);

    // 2. Empty Space
    if (targetEmpty) { 
        if (isFinal) {
            myKin->isFreeFalling = true;
            world.moveParticle(myX, myY, targetX, targetY);
            myX = targetX;
            myY = targetY;
            return false; 
        } else {
            return false; 
        }
    }
    
    // 3. Liquid Interaction (Density Swap)
    if (targetBase) {
        Particle* targetLogic = MaterialRegistry[static_cast<int>(targetBase->id)];
        if (targetLogic && targetLogic->getGroup() == MaterialGroup::Liquid) {
            auto* targetFluid = world.getFast<FluidComponent>(ctx, targetX, targetY);
            if (targetFluid && myDensity > targetFluid->density) {
                if (isFinal) {
                    myKin->velocity.y = BOUNCE_VEL_Y; 
                    if (Random::randFloat(0,1) > 0.8f) myKin->velocity.x *= -1;
                    
                    world.swapParticles(myX, myY, targetX, targetY);
                    myX = targetX;
                    myY = targetY;
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

    sf::Vector2f normVel = myKin->velocity;
    float len = std::sqrt(normVel.x*normVel.x + normVel.y*normVel.y);
    if (len != 0) normVel /= len;

    int additionalX = getAdditional(normVel.x);
    int dist = additionalX * (Random::randBool() ? myDispersionRate + 2 : myDispersionRate - 1);

    auto* targetKin = world.getFast<KinematicsComponent>(ctx, targetX, targetY);
    if (targetKin) {
        if (isFirst) myKin->velocity.y = getAverageVelOrGravity(myKin->velocity.y, targetKin->velocity.y);
        else myKin->velocity.y = MAX_VEL_Y; 
        targetKin->velocity.y = myKin->velocity.y;
    }
    
    myKin->velocity.x *= 1.0f; 

    // A. Try Diagonal
    int diagX = myX + additionalX;
    int diagY = myY + 1; 
    if (world.inBounds(diagX, diagY)) {
        if (!iterateToAdditional(ctx, world, diagX, diagY, dist, myX, myY, myDensity)) {
            myKin = world.getFast<KinematicsComponent>(ctx, myX, myY);
            if(myKin) myKin->isFreeFalling = true;
            return true; 
        }
    }

    // B. Try Adjacent
    int adjX = myX + additionalX;
    int adjY = myY; 
    if (world.inBounds(adjX, adjY)) {
        if (iterateToAdditional(ctx, world, adjX, adjY, dist, myX, myY, myDensity)) {
            myKin = world.getFast<KinematicsComponent>(ctx, myX, myY);
            if(myKin) myKin->velocity.x *= -1; 
        } else {
            myKin = world.getFast<KinematicsComponent>(ctx, myX, myY);
            if(myKin) myKin->isFreeFalling = false;
            return true; 
        }
    }

    myKin = world.getFast<KinematicsComponent>(ctx, myX, myY);
    if(myKin) myKin->isFreeFalling = false;
    return true; 
}
bool Liquid::iterateToAdditional(const ParticleContext& ctx, ParticleWorld& world, int startX, int startY, int distance, int& currentX, int& currentY, int myDensity) 
{
    int distanceModifier = (distance > 0) ? 1 : -1;
    int absDist = std::abs(distance);
    int endX = startX + (absDist * distanceModifier);
    
    int lastValidX = currentX;
    int lastValidY = currentY;
    
    // Fetch ONCE before the loop using fast path
    auto* myKin = world.getFast<KinematicsComponent>(ctx, currentX, currentY);
    auto* myBase = world.getFast<BaseComponent>(ctx, currentX, currentY);
    if (!myKin || !myBase || myBase->compMask == 0) return false;

    // 3. The "Chunk-Safe" Loop
    bool isEntirelyInChunk = ((((ctx.x ^ startX) | (ctx.x ^ endX) | (ctx.y ^ startY)) & ~63) == 0);

    if (isEntirelyInChunk) {
        // --- SUPER FAST PATH ---
        for (int i = 0; i <= absDist; i++) {
            int modifiedX = startX + (i * distanceModifier);
            uint32_t targetIdx = ((startY & 63) << 6) | (modifiedX & 63);
            
            BaseComponent* targetBase = &ctx.base[targetIdx];
            bool empty = (targetBase->compMask == 0);

            if (!empty) {
                if (actOnOther(myBase, currentX, currentY, targetBase, modifiedX, startY, world)) return false; 
                
                // NO RE-FETCHING! Just check if we died.
                if (myBase->compMask == 0) return false; 
                
                empty = (targetBase->compMask == 0);
            }
            
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
                Particle* logic = MaterialRegistry[static_cast<int>(targetBase->id)];
                
                if (logic && logic->getGroup() == MaterialGroup::Liquid) {
                    auto* nf = &ctx.fluid[targetIdx];
                    if (isFinal && nf && myDensity > nf->density) {
                        world.swapParticles(currentX, currentY, modifiedX, startY);
                        currentX = modifiedX;
                        currentY = startY;
                        myKin->velocity.y = BOUNCE_VEL_Y;
                        if (Random::randFloat(0,1) > 0.8f) myKin->velocity.x *= -1;
                        return false;
                    }
                } else {
                    if (i == 0) return true; 
                    if (lastValidX != currentX || lastValidY != currentY) {
                        world.moveParticle(currentX, currentY, lastValidX, lastValidY);
                        currentX = lastValidX;
                        currentY = lastValidY;
                        return false;
                    }
                    return true;
                }
            }
        }
        return true; 
    } 
    else {
        // --- SLOW PATH ---
        for (int i = 0; i <= absDist; i++) {
            int modifiedX = startX + (i * distanceModifier);
            if (!world.inBounds(modifiedX, startY)) return true; 

            BaseComponent* targetBase = world.getFast<BaseComponent>(ctx, modifiedX, startY);
            bool empty = (!targetBase || targetBase->compMask == 0);

            if (!empty) {
                if (actOnOther(myBase, currentX, currentY, targetBase, modifiedX, startY, world)) return false; 
                
                // NO RE-FETCHING! Just check if we died.
                if (myBase->compMask == 0) return false; 

                targetBase = world.getFast<BaseComponent>(ctx, modifiedX, startY);
                empty = (!targetBase || targetBase->compMask == 0);
            }
            
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
                Particle* logic = MaterialRegistry[static_cast<int>(targetBase->id)];
                
                if (logic && logic->getGroup() == MaterialGroup::Liquid) {
                    auto* nf = world.getFast<FluidComponent>(ctx, modifiedX, startY);
                    if (isFinal && nf && myDensity > nf->density) {
                        world.swapParticles(currentX, currentY, modifiedX, startY);
                        currentX = modifiedX;
                        currentY = startY;
                        myKin->velocity.y = BOUNCE_VEL_Y;
                        if (Random::randFloat(0,1) > 0.8f) myKin->velocity.x *= -1;
                        return false;
                    }
                } else {
                    if (i == 0) return true; 
                    if (lastValidX != currentX || lastValidY != currentY) {
                        world.moveParticle(currentX, currentY, lastValidX, lastValidY);
                        currentX = lastValidX;
                        currentY = lastValidY;
                        return false;
                    }
                    return true;
                }
            }
        }
        return true; 
    }
}

int Liquid::getAdditional(float val) {
    if (val < -0.1f) return (int)std::floor(val);
    if (val > 0.1f) return (int)std::ceil(val);
    return 0;
}

float Liquid::getAverageVelOrGravity(float myVel, float otherVel) {
    if (otherVel < (MAX_VEL_Y + 1.0f)) return MAX_VEL_Y;
    float avg = (myVel + otherVel) / 2.0f;
    return std::min(std::max(avg, 0.0f), MAX_VEL_Y);
}

// --- SUBCLASS IMPLEMENTATIONS ---

// WATER
void Water::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    if (auto* fluid = world.get<FluidComponent>(x, y)) {
        fluid->density = 5; fluid->dispersionRate = 5;
    }
    world.add<ThermalComponent>(x, y, ThermalComponent(0, 100, 0, 0)); 
    world.add<DurabilityComponent>(x, y, DurabilityComponent(1, 2));
}

bool Water::receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) {
    dieAndReplace(x, y, MaterialID::Steam, world);
    return true; 
}

bool Water::actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) {
    if (otherBase) {
        Particle* logic = MaterialRegistry[static_cast<int>(otherBase->id)];
        if (logic) { 
            logic->cleanColor(otherBase, otherX, otherY, world);
            if (logic->shouldApplyHeat(otherBase)) {
                auto* otherTherm = world.get<ThermalComponent>(otherX, otherY);
                if (logic->receiveCooling(otherBase, otherTherm, otherX, otherY, 5, world)) {
                     dieAndReplace(myX, myY, MaterialID::Steam, world);
                     return true;
                }
            }
        }
    }
    return false;
}

bool Water::explode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int strength, ParticleWorld& world) {
    dieAndReplace(x, y, MaterialID::Steam, world);
    return true;
}

// OIL
void Oil::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    if (auto* fluid = world.get<FluidComponent>(x, y)) {
        fluid->density = 4; fluid->dispersionRate = 4;
    }
    world.add<ThermalComponent>(x, y, ThermalComponent(0, 5, 10, 10));
    world.add<DurabilityComponent>(x, y, DurabilityComponent(1000, 0));
}

bool Oil::actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) {
    if (otherBase && (otherBase->flags.isIgnited || otherBase->id == MaterialID::Lava)) {
        auto* myTherm = world.get<ThermalComponent>(myX, myY);
        receiveHeat(myBase, myTherm, myX, myY, 100, world); 
    }
    return false;
}

// LAVA
void Lava::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    if (auto* base = world.get<BaseComponent>(x, y)) base->flags.heated = true;
    if (auto* fluid = world.get<FluidComponent>(x, y)) {
        fluid->density = 10; fluid->dispersionRate = 1;
    }
    world.add<ThermalComponent>(x, y, ThermalComponent(10, 0, 10, 0));
    world.add<DurabilityComponent>(x, y, DurabilityComponent(100, 0));
}

void Lava::checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    auto* therm = world.get<ThermalComponent>(x, y);
    
    // Check if temperature reached freezing point
    if (therm && therm->temperature <= 0) {
        // 1. Transform the lava particle itself into stone
        dieAndReplace(x, y, MaterialID::Stone, world);

        // 2. Transform neighboring liquids into stone (The missing "Chain Reaction" logic)
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            for (int offsetY = -1; offsetY <= 1; ++offsetY) {
                // Skip the center pixel (we already handled it above)
                if (offsetX == 0 && offsetY == 0) continue;

                int targetX = x + offsetX;
                int targetY = y + offsetY;

                if (world.inBounds(targetX, targetY)) {
                    // Look up the neighbor in the ECS
                    BaseComponent* neighborBase = world.get<BaseComponent>(targetX, targetY);
                    
                    if (neighborBase) {
                        // Check if the neighbor is part of the Liquid group
                        Particle* neighborLogic = MaterialRegistry[static_cast<int>(neighborBase->id)];
                        if (neighborLogic && neighborLogic->getGroup() == MaterialGroup::Liquid) {
                            // Turn the neighboring liquid into stone
                            neighborLogic->dieAndReplace(targetX, targetY, MaterialID::Stone, world);
                        }
                    }
                }
            }
        }
    }
}

bool Lava::receiveCooling(BaseComponent* base, ThermalComponent* therm, int x, int y, int cooling, ParticleWorld& world) {
    if (therm) {
        therm->temperature -= cooling;
        return true;
    }
    return false;
}

bool Lava::actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) {
    if (otherBase) {
        Particle* otherLogic = MaterialRegistry[static_cast<int>(otherBase->id)];
        if (otherLogic) {
            // New interaction: Lava + Water = Stone
            if (otherBase->id == MaterialID::Water) {
                world.spawnParticle(MaterialID::Steam, otherX, otherY);
                dieAndReplace(myX, myY, MaterialID::Stone, world);
                return true;
            } else {
                auto* otherDur = world.get<DurabilityComponent>(otherX, otherY);
                otherLogic->magmatize(otherBase, otherDur, otherX, otherY, Random::randInt(0, 10), world);
            }
        }
    }
    return false;
}

// ACID
void Acid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    if (auto* fluid = world.get<FluidComponent>(x, y)) {
        fluid->density = 2; fluid->dispersionRate = 2;
    }
    world.add<DurabilityComponent>(x, y, DurabilityComponent(3, 0));
}

bool Acid::actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) {
    if (otherBase) {
        Particle* logic = MaterialRegistry[static_cast<int>(otherBase->id)];
        if (logic) { 
            logic->stain(otherBase, otherX, otherY, sf::Color(0, 255, 0, 100), world);

            auto* otherDur = world.get<DurabilityComponent>(otherX, otherY);
            if (logic->corrode(otherBase, otherDur, otherX, otherY, world)) {
                auto* myDur = world.get<DurabilityComponent>(myX, myY);
                if (myDur) {
                    myDur->health--;
                    if (myDur->health <= 0) {
                         dieAndReplace(myX, myY, MaterialID::FlammableGas, world);
                    }
                }
                return true;
            }
        }
    }
    return false;
}

// CEMENT
void Cement::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    if (auto* fluid = world.get<FluidComponent>(x, y)) {
        fluid->density = 9; fluid->dispersionRate = 1;
    }
    world.add<DurabilityComponent>(x, y, DurabilityComponent(200, 2));
}

void Cement::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    // 1. Run standard liquid physics (this increments stoppedMovingCount)
    Liquid::update(ctx, dt, world);

    // 2. Check if the particle is still alive after the update.
    auto* base = world.getFast<BaseComponent>(ctx, ctx.x, ctx.y);
    if (!base || base->id != MaterialID::Cement) return;

    // 3. Check the kinematics counter
    auto* kin = world.getFast<KinematicsComponent>(ctx, ctx.x, ctx.y);
    if (kin && kin->stoppedMovingCount >= 50) { 
        dieAndReplace(ctx.x, ctx.y, MaterialID::Stone, world);
    }
}

// BLOOD
void Blood::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);
    if (auto* fluid = world.get<FluidComponent>(x, y)) {
        fluid->density = 6; fluid->dispersionRate = 5;
    }
    world.add<DurabilityComponent>(x, y, DurabilityComponent(1, 2));
}

bool Blood::actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) {
    if (otherBase) {
        Particle* logic = MaterialRegistry[static_cast<int>(otherBase->id)];
        if (logic) {
            logic->stain(otherBase, otherX, otherY, sf::Color(150, 0, 0), world);
            if (logic->shouldApplyHeat(otherBase)) {
                 dieAndReplace(myX, myY, MaterialID::Steam, world);
                 return true;
            }
        }
    }
    return false;
}

static Water water_instance;
static Oil oil_instance;
static Lava lava_instance;
static Acid acid_instance;
static Cement cement_instance;
static Blood blood_instance;