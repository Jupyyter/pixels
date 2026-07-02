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
        if (base->flags.isIgnited || base->flags.heated) {
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
        
        if (kin->isFreeFalling || kin->stoppedMovingCount < 5) {
           world.wakeParticle(currentX, currentY);
        }
    }
}

bool Liquid::actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, 
                           ParticleWorld& world, bool isFinal, bool isFirst, int depth, int myDensity, int myDispersionRate) 
{
    auto* myBase = world.getFast<BaseComponent>(ctx, myX, myY);
    if (!myBase || myBase->compMask == 0) return true; 

    BaseComponent* targetBase = world.getFast<BaseComponent>(ctx, targetX, targetY);
    bool targetEmpty = (!targetBase || targetBase->compMask == 0);

    if (!targetEmpty) {
        if (this->actOnOther(myBase, myX, myY, targetBase, targetX, targetY, world)) return true; 
    }

    if (myBase->compMask == 0) return true; 
    
    auto* myKin = world.getFast<KinematicsComponent>(ctx, myX, myY);
    if (!myKin) return true; 

    targetBase = world.getFast<BaseComponent>(ctx, targetX, targetY);
    targetEmpty = (!targetBase || targetBase->compMask == 0);

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
    
    if (targetBase) {
        Particle* targetLogic = MaterialRegistry[static_cast<int>(targetBase->id)];
        if (targetLogic) {
            if (targetLogic->getGroup() == MaterialGroup::Liquid) {
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
            } else if (targetLogic->getGroup() == MaterialGroup::Gas) {
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

    if (depth > 0 || isFinal) return true;

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

    int diagX = myX + additionalX;
    int diagY = myY + 1; 
    if (world.inBounds(diagX, diagY)) {
        if (!iterateToAdditional(ctx, world, diagX, diagY, dist, myX, myY, myDensity)) {
            myKin = world.getFast<KinematicsComponent>(ctx, myX, myY);
            if(myKin) myKin->isFreeFalling = true;
            return true; 
        }
    }

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
    
    auto* myKin = world.getFast<KinematicsComponent>(ctx, currentX, currentY);
    auto* myBase = world.getFast<BaseComponent>(ctx, currentX, currentY);
    if (!myKin || !myBase || myBase->compMask == 0) return false;

    bool isEntirelyInChunk = ((((ctx.x ^ startX) | (ctx.x ^ endX) | (ctx.y ^ startY)) & ~63) == 0);

    if (isEntirelyInChunk) {
        for (int i = 0; i <= absDist; i++) {
            int modifiedX = startX + (i * distanceModifier);
            uint32_t targetIdx = ((startY & 63) << 6) | (modifiedX & 63);
            
            BaseComponent* targetBase = &ctx.base[targetIdx];
            bool empty = (targetBase->compMask == 0);

            if (!empty) {
                if (actOnOther(myBase, currentX, currentY, targetBase, modifiedX, startY, world)) return false; 
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
        for (int i = 0; i <= absDist; i++) {
            int modifiedX = startX + (i * distanceModifier);
            if (!world.inBounds(modifiedX, startY)) return true; 

            BaseComponent* targetBase = world.getFast<BaseComponent>(ctx, modifiedX, startY);
            bool empty = (!targetBase || targetBase->compMask == 0);

            if (!empty) {
                if (actOnOther(myBase, currentX, currentY, targetBase, modifiedX, startY, world)) return false; 
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

void GenericLiquid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Liquid::onSpawn(index, x, y, world);

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

void GenericLiquid::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    Liquid::update(ctx, dt, world);

    auto* base = world.getFast<BaseComponent>(ctx, ctx.x, ctx.y);
    if (!base || base->id != this->id) return;

    if (def.transform_on_rest_frames > 0) {
        auto* kin = world.getFast<KinematicsComponent>(ctx, ctx.x, ctx.y);
        if (kin && kin->stoppedMovingCount >= def.transform_on_rest_frames) { 
            dieAndReplace(ctx.x, ctx.y, def.transform_on_rest_result, world);
        }
    }
}

void GenericLiquid::checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    if (def.transform_on_min_temp_result != 0) {
        auto* therm = world.get<ThermalComponent>(x, y);
        if (therm && therm->temperature <= def.min_temp_threshold) {
            dieAndReplace(x, y, def.transform_on_min_temp_result, world);
            if (def.min_temp_transform_neighbors) {
                for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
                        if (offsetX == 0 && offsetY == 0) continue;
                        int tx = x + offsetX, ty = y + offsetY;
                        if (world.inBounds(tx, ty)) {
                            BaseComponent* nb = world.get<BaseComponent>(tx, ty);
                            if (nb) {
                                Particle* nl = MaterialRegistry[static_cast<int>(nb->id)];
                                if (nl && nl->getGroup() == MaterialGroup::Liquid) {
                                    nl->dieAndReplace(tx, ty, def.transform_on_min_temp_result, world);
                                }
                            }
                        }
                    }
                }
            }
            return;
        }
    }

    if (dur && dur->health <= 0) {
        if (def.transform_on_health_zero_result != static_cast<MaterialID>(0)) {
            dieAndReplace(x, y, def.transform_on_health_zero_result, world);
        } else {
            die(x, y, world);
        }
    }
}

bool GenericLiquid::actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) {
    if (executeGenericTraitsAndInteractions(def, myBase, myX, myY, otherBase, otherX, otherY, world)) return true;
    return Liquid::actOnOther(myBase, myX, myY, otherBase, otherX, otherY, world);
}

bool GenericLiquid::receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) {
    if (def.immune_to_fire) return false;
    if (def.transform_on_heat_result != 0 && heat > 0) {
        dieAndReplace(x, y, def.transform_on_heat_result, world);
        return true;
    }
    if (!def.has_thermal) return false; 
    return Particle::receiveHeat(base, therm, x, y, heat, world);
}

bool GenericLiquid::corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) {
    if (def.immune_to_corrosion || def.has_trait_corrosive) return false;
    return Particle::corrode(base, dur, x, y, damage, world);
}
bool GenericLiquid::magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) {
    if (def.immune_to_magmatize || def.has_trait_magmatize) return false; 
    return Particle::magmatize(base, dur, x, y, damage, world);
}

void GenericLiquid::spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world) {
    if (def.spark_chance >= 0.0f) {
        if (!base || (!base->flags.isIgnited && !base->flags.heated)) return;
        int upX = x, upY = y - 1;
        if (world.inBounds(upX, upY) && world.isEmpty(upX, upY)) {
            if (Random::randFloat(0,1) < def.spark_chance) {
                MaterialID spawnId = (Random::randFloat(0.0f, 1.0f) > 0.1f) ? GetMatID("Spark") : GetMatID("Smoke");
                world.spawnParticle(spawnId, upX, upY);
            }
        }
    } else {
        Particle::spawnSparkIfIgnited(base, x, y, world);
    }
}