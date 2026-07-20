#include "Particles/MovableSolid.hpp"
#include "ParticleWorld.hpp"
#include <cmath>
#include <algorithm>

static const float MAX_VEL_Y = 124.0f;

void MovableSolid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);
    world.add<KinematicsComponent>(x, y, KinematicsComponent(
        sf::Vector2f(0.0f, 124.0f), 0.0f, 0.0f, true, 0
    ));
    world.add<DurabilityComponent>(x, y, DurabilityComponent(500, 1));
}

void MovableSolid::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    if (!ctx.kinematics) return;
    auto* kin = &ctx.kinematics[ctx.index];

    kin->velocity.y += (GRAVITY * getGravityMult() * dt);
    if (kin->velocity.y > MAX_VEL_Y) kin->velocity.y = MAX_VEL_Y;
    if (kin->velocity.y < -MAX_VEL_Y) kin->velocity.y = -MAX_VEL_Y;
    
    if (kin->isFreeFalling) {
        kin->velocity.x *= 0.9f; 
    } else {
        kin->velocity.x *= 0.3f; 
        if (std::abs(kin->velocity.x) < 0.1f) kin->velocity.x = 0.0f;
    }

    kin->xThreshold += std::abs(kin->velocity.x * dt);
    kin->yThreshold += std::abs(kin->velocity.y * dt);

    int velXInt = static_cast<int>(kin->xThreshold);
    int velYInt = static_cast<int>(kin->yThreshold);
    kin->xThreshold -= (float)velXInt;
    kin->yThreshold -= (float)velYInt;

    int xMod = (kin->velocity.x < 0) ? -1 : 1;
    int yMod = (kin->velocity.y < 0) ? -1 : 1;

    int steps = std::max(velXInt, velYInt);
    
    if (steps == 0) {
        int checkY = getGravityMult() < 0 ? (ctx.y - 1) : (ctx.y + 1);
        if (world.isEmptyFast(ctx, ctx.x, checkY)) {
            kin->isFreeFalling = true;
        } else {
            kin->isFreeFalling = false;
        }
    }
    
    int curX = ctx.x, curY = ctx.y;
    float slope = (steps == 0) ? 0.0f : ((float)std::min(velXInt, velYInt) / steps);
    bool xLarger = velXInt > velYInt;

    for (int i = 1; i <= steps; i++) {
        int small = (int)std::floor(i * slope);
        int targetX = ctx.x + (xLarger ? i : small) * xMod;
        int targetY = ctx.y + (xLarger ? small : i) * yMod;

        if (world.inBounds(targetX, targetY)) {
            if (targetX == curX && targetY == curY) continue;
            if (actOnNeighbor(ctx, targetX, targetY, curX, curY, world, (i == steps), (i == 1), 0)) break;
        } else {
            if (auto* k = world.getFast<KinematicsComponent>(ctx, curX, curY)) k->velocity = {0,0}; 
            break; 
        }
    }

    uint32_t finalIdx = world.computeIndex(curX, curY); 
    world.updateParticleColor(finalIdx, curX, curY);

    auto* base = world.getFast<BaseComponent>(ctx, curX, curY);
    if (base) {
        if (base->flags.isIgnited) {
            auto* therm = world.getFast<ThermalComponent>(ctx, curX, curY);
            applyHeatToNeighborsIfIgnited(base, therm, curX, curY, world);
            spawnSparkIfIgnited(base, curX, curY, world);
            
            auto* dur = world.getFast<DurabilityComponent>(ctx, curX, curY);
            if (dur && base->id != GetMatID("Coal")) {
                dur->health -= (base->id == GetMatID("Ember")) ? 1 : 2; 
                
                if (dur->health <= 0) {
                    if (base->id == GetMatID("Ember")) {
                        die(curX, curY, world); 
                    } else {
                        dieAndReplace(curX, curY, GetMatID("Smoke"), world);
                    }
                    return; 
                }
            }
        }
        
        auto* durFinal = world.getFast<DurabilityComponent>(ctx, curX, curY);
        auto* thermFinal = world.getFast<ThermalComponent>(ctx, curX, curY);
        takeEffectsDamage(base, durFinal, thermFinal, curX, curY, world);
    }

    if (auto* fKin = world.getFast<KinematicsComponent>(ctx, curX, curY)) {
        if (curX == ctx.x && curY == ctx.y) {
             fKin->stoppedMovingCount = std::min(fKin->stoppedMovingCount + 1, 5);
        } else {
             fKin->stoppedMovingCount = 0;
        }

        if (fKin->isFreeFalling || fKin->stoppedMovingCount < 5) {
            world.wakeParticle(curX, curY);
        }
    }
}

bool MovableSolid::actOnNeighbor(const ParticleContext& ctx, int targetX, int targetY, int& myX, int& myY, 
                                 ParticleWorld& world, bool isFinal, bool isFirst, int depth) 
{
    auto* myBase = world.getFast<BaseComponent>(ctx, myX, myY);
    auto* myKin = world.getFast<KinematicsComponent>(ctx, myX, myY);
    if (!myBase || myBase->compMask == 0 || !myKin) return true;

    BaseComponent* targetBase = world.getFast<BaseComponent>(ctx, targetX, targetY);
    bool targetEmpty = (!targetBase || targetBase->compMask == 0);

    if (!targetEmpty) {
        if (this->actOnOther(myBase, myX, myY, targetBase, targetX, targetY, world)) return true;
        
        if (myBase->compMask == 0) return true;
        
        targetBase = world.getFast<BaseComponent>(ctx, targetX, targetY);
        targetEmpty = (!targetBase || targetBase->compMask == 0);
    }

    if (targetEmpty) {
        setAdjacentNeighborsFreeFalling(ctx, myX, myY, world, depth);
        if (isFinal) {
            myKin->isFreeFalling = true;
            world.moveParticle(myX, myY, targetX, targetY);
            myX = targetX; myY = targetY;
        }
        return false; 
    }

    if (targetBase) {
        Particle* logic = MaterialRegistry[static_cast<int>(targetBase->id)];
        if (logic && (logic->getGroup() == MaterialGroup::Liquid || logic->getGroup() == MaterialGroup::Gas)) {
            myKin->isFreeFalling = true;
            world.swapParticles(myX, myY, targetX, targetY);
            myX = targetX; myY = targetY;
            return true; 
        }
    }

    if (depth > 0 || isFinal) return true;

    if (myKin->isFreeFalling) {
        if (std::abs(myKin->velocity.y) > 60.0f) {
            float speed = std::abs(myKin->velocity.y) * 0.4f; 
            myKin->velocity.x = (myKin->velocity.x < 0) ? -speed : speed;
        } else {
            myKin->velocity.x *= 0.5f; 
        }
    }

    int addX = getAdditional(myKin->velocity.x);
    if (auto* tKin = world.getFast<KinematicsComponent>(ctx, targetX, targetY)) {
        if (getBounciness() > 0.0f && std::abs(myKin->velocity.y) > 10.0f) {
            myKin->velocity.y = -myKin->velocity.y * getBounciness();
            myKin->isFreeFalling = true;
        } else {
            myKin->velocity.y = isFirst ? getAverageVelOrGravity(myKin->velocity.y, tKin->velocity.y) : 124.0f * getGravityMult();
        }
        tKin->velocity.y = myKin->velocity.y;
        myKin->velocity.x *= 0.25f; 
    }

    int diagX = myX + addX, diagY = myY + (getGravityMult() > 0 ? 1 : -1);
    if (world.inBounds(diagX, diagY)) {
        if (!actOnNeighbor(ctx, diagX, diagY, myX, myY, world, true, false, depth + 1)) {
            if (auto* k = world.getFast<KinematicsComponent>(ctx, myX, myY)) k->isFreeFalling = true;
            return true; 
        }
    }

    int adjX = myX + addX;
    if (world.inBounds(adjX, myY)) {
        if (actOnNeighbor(ctx, adjX, myY, myX, myY, world, true, false, depth + 1)) {
            if (auto* k = world.getFast<KinematicsComponent>(ctx, myX, myY)) k->velocity.x *= -1; 
        } else {
            if (auto* k = world.getFast<KinematicsComponent>(ctx, myX, myY)) k->isFreeFalling = false;
            return true;
        }
    }

    if (auto* k = world.getFast<KinematicsComponent>(ctx, myX, myY)) k->isFreeFalling = false;
    return true; 
}

void MovableSolid::setAdjacentNeighborsFreeFalling(const ParticleContext& ctx, int x, int y, ParticleWorld& world, int depth) {
    if (depth > 0) return;
    for (int dx : {1, -1}) {
        if (auto* kin = world.getFast<KinematicsComponent>(ctx, x + dx, y)) {
            if (Random::randFloat(0, 1) > 0.1f) kin->isFreeFalling = true;
        }
    }
}

int MovableSolid::getAdditional(float val) {
    return (val < -0.1f) ? -1 : (val > 0.1f ? 1 : 0);
}

float MovableSolid::getAverageVelOrGravity(float myVel, float otherVel) {
    float maxV = 124.0f * getGravityMult();
    if (std::abs(otherVel) < std::abs(maxV) + 1.0f) return maxV;
    float avg = (myVel + otherVel) * 0.5f;
    return getGravityMult() > 0 ? std::min(std::max(avg, 0.0f), maxV) : std::max(std::min(avg, 0.0f), maxV);
}

void GenericMovableSolid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);

    if (auto* base = world.get<BaseComponent>(x, y)) {
        base->color = def.getRandomColor();
        if (def.ignite_on_spawn) base->flags.isIgnited = true;
        if (def.heated_on_spawn) base->flags.heated = true;
    }

    if (def.scatter_on_spawn) {
        if (auto* kin = world.get<KinematicsComponent>(x, y)) {
            kin->velocity.x = (Random::randBool()) ? -1.0f : 1.0f;
        }
    }

    if (def.has_durability) {
        if (auto* dur = world.get<DurabilityComponent>(x, y)) {
            int hp = (def.dur_health_max > def.dur_health) ? Random::randInt(def.dur_health, def.dur_health_max) : def.dur_health;
            dur->health = hp;
            dur->explosionResistance = def.dur_expRes;
        }
    }
    if (def.has_thermal) {
        world.add<ThermalComponent>(x, y, ThermalComponent(def.therm_temp, def.therm_flamRes, def.therm_heat, def.therm_fireDmg));
    }
}

void GenericMovableSolid::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    if (def.viscosity > 1 && (world.getFrameCounter() % def.viscosity) != 0) return;

    if (def.stickiness > 0.0f && Random::randFloat(0,1) < def.stickiness) {
        bool touchingWall = false;
        for (int ox=-1; ox<=1; ox++) {
            if (ox==0) continue;
            if (!world.isEmptyFast(ctx, ctx.x+ox, ctx.y)) { touchingWall = true; break; }
        }
        if (touchingWall) {
            if (auto* kin = world.getFast<KinematicsComponent>(ctx, ctx.x, ctx.y)) {
                kin->velocity.y = 0; kin->velocity.x = 0; kin->isFreeFalling = false;
            }
        }
    }

    auto* base = world.getFast<BaseComponent>(ctx, ctx.x, ctx.y);
    if (!base || base->id != this->id) return;
    
    if (def.has_trait_explosive_on_ignite && base->flags.isIgnited) {
        auto* dur = world.getFast<DurabilityComponent>(ctx, ctx.x, ctx.y);
        if (dur) {
            dur->health--;
            if (dur->health <= 0) {
                world.triggerExplosion(ctx.x, ctx.y, def.explosive_radius, def.explosive_strength);
                die(ctx.x, ctx.y, world);
                return;
            }
        }
    }

    if (def.flutter_fall) {
        auto* kin = world.getFast<KinematicsComponent>(ctx, ctx.x, ctx.y);
        if (kin && kin->velocity.y > 62.0f * getGravityMult()) {
            kin->velocity.y = (Random::randFloat(0,1) > 0.3f) ? 62.0f * getGravityMult() : 124.0f * getGravityMult();
        }
    }
    
    MovableSolid::update(ctx, dt, world);

    base = world.getFast<BaseComponent>(ctx, ctx.x, ctx.y);
    if (!base || base->id != this->id) return;

    processAdvancedOrganicAndElectricalTraits(def, ctx, world);

    if (def.transform_on_rest_ticks > 0) {
        auto* kin = world.getFast<KinematicsComponent>(ctx, ctx.x, ctx.y);
        if (kin && kin->stoppedMovingCount >= def.transform_on_rest_ticks) { 
            dieAndReplace(ctx.x, ctx.y, def.transform_on_rest_result, world);
        }
    }
}

void GenericMovableSolid::checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
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

bool GenericMovableSolid::actOnOther(BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) {
    if (executeGenericTraitsAndInteractions(def, myBase, myX, myY, otherBase, otherX, otherY, world)) return true;
    return MovableSolid::actOnOther(myBase, myX, myY, otherBase, otherX, otherY, world);
}

bool GenericMovableSolid::receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) {
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

bool GenericMovableSolid::corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) {
    if (def.immune_to_corrosion || def.has_trait_corrosive) return false;
    return Particle::corrode(base, dur, x, y, damage, world);
}

bool GenericMovableSolid::magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) {
    if (def.immune_to_magmatize || def.has_trait_magmatize) return false;
    return Particle::magmatize(base, dur, x, y, damage, world);
}

bool GenericMovableSolid::explode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int strength, ParticleWorld& world) {
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

bool GenericMovableSolid::receiveCharge(BaseComponent* base, int x, int y, ParticleWorld& world) {
    if (def.is_conductive && base && !base->flags.isCharged) {
        base->flags.isCharged = true;
        if (def.transform_on_charged_result != 0) {
            dieAndReplace(x, y, def.transform_on_charged_result, world);
        }
        return true;
    }
    return false;
}

void GenericMovableSolid::takeEffectsDamage(BaseComponent* base, DurabilityComponent* dur, ThermalComponent* therm, int x, int y, ParticleWorld& world) {
    if (def.smolders) return;
    Particle::takeEffectsDamage(base, dur, therm, x, y, world);
}

void GenericMovableSolid::spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world) {
    if (def.spark_chance >= 0.0f) {
        if (!base || !base->flags.isIgnited) return;
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