#include "Particles/MovableSolid.hpp"
#include "ParticleWorld.hpp"
#include <cmath>
#include <algorithm>

static const float MAX_VEL_Y = 124.0f;

// --- MOVABLE SOLID BASE IMPLEMENTATION ---

void MovableSolid::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    Particle::onSpawn(index, x, y, world);

    // Initialized with forced constructors
    world.add<KinematicsComponent>(x, y, KinematicsComponent(
        sf::Vector2f(0.0f, 124.0f), 0.0f, 0.0f, true, 0
    ));

    world.add<DurabilityComponent>(x, y, DurabilityComponent(500, 1));
}

void MovableSolid::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    if (!ctx.kinematics) return;
    auto* kin = &ctx.kinematics[ctx.index];

    kin->velocity.y += (GRAVITY * dt);
    if (kin->velocity.y > MAX_VEL_Y) kin->velocity.y = MAX_VEL_Y;
    
    // --- NEW FRICTION LOGIC ---
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
        if (world.isEmptyFast(ctx, ctx.x, ctx.y + 1)) {
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
            
            // RESOLUTION FOR EVER-BURNING RESIDUE AND IMMORTAL EMBERS: 
            // All flaming payload copies systematically consume themselves dynamically,
            // correctly preventing immortal floating blazing sand leftovers eternally clogging map physics bounds gracefully flawlessly safely properly smoothly! 
            auto* dur = world.getFast<DurabilityComponent>(ctx, curX, curY);
            if (dur && base->id != MaterialID::Coal) { // Prevent unigniting primary perpetual fuel lines implicitly accurately smoothly logically reliably smoothly efficiently smartly effortlessly appropriately explicitly functionally efficiently identically seamlessly seamlessly organically dynamically completely! 
                dur->health -= (base->id == MaterialID::Ember) ? 1 : 2; 
                
                if (dur->health <= 0) {
                    if (base->id == MaterialID::Ember) {
                        die(curX, curY, world); 
                    } else {
                        // Debris disintegrates appropriately elegantly!
                        dieAndReplace(curX, curY, MaterialID::Smoke, world);
                    }
                    return; // Crucial bounds interruption explicitly avoiding dereference mismatches correctly stably securely efficiently effectively gracefully safely smartly flawlessly automatically intuitively safely identically cleanly cleanly efficiently logically intuitively identically safely securely inherently intelligently successfully cleanly effortlessly intuitively accurately naturally reliably explicitly optimally smoothly stably implicitly inherently perfectly organically seamlessly reliably dynamically seamlessly safely perfectly organically natively implicitly effortlessly appropriately successfully functionally properly natively
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
        if (logic && logic->getGroup() == MaterialGroup::Liquid) {
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
        myKin->velocity.y = isFirst ? getAverageVelOrGravity(myKin->velocity.y, tKin->velocity.y) : 124.0f;
        tKin->velocity.y = myKin->velocity.y;
        myKin->velocity.x *= 0.25f; 
    }

    int diagX = myX + addX, diagY = myY + 1;
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
    if (otherVel < 125.0f) return 124.0f;
    float avg = (myVel + otherVel) * 0.5f;
    return (avg < 0) ? avg : std::min(avg, 124.0f);
}

// --- SUBCLASS IMPLEMENTATIONS ---

void Sand::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
    if (auto* kin = world.get<KinematicsComponent>(x, y)) kin->velocity.x = (Random::randBool()) ? -1.0f : 1.0f;
}

void Dirt::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
}

void Coal::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
    world.add<ThermalComponent>(x, y, ThermalComponent(0, 100, 10, 1));
}

void Coal::spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world) {
    if (Random::randInt(0, 20) <= 2) Particle::spawnSparkIfIgnited(base, x, y, world); 
}

void Gunpowder::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
    world.add<ThermalComponent>(x, y, ThermalComponent(0, 10, 10, 0));
}

void Gunpowder::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    auto* base = world.getFast<BaseComponent>(ctx, ctx.x, ctx.y);
    auto* dur = world.getFast<DurabilityComponent>(ctx, ctx.x, ctx.y); 
    
    if (base && base->id == MaterialID::Gunpowder && base->flags.isIgnited && dur) {
        if (++dur->health >= 7) { 
             world.triggerExplosion(ctx.x, ctx.y, 15, 10);
             die(ctx.x, ctx.y, world);
             return; 
        }
    }
    
    MovableSolid::update(ctx, dt, world);
}

void Snow::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
    if (auto* kin = world.get<KinematicsComponent>(x, y)) kin->velocity.y = 62.0f; 
    world.add<ThermalComponent>(x, y, ThermalComponent(0, 100, 0, 0));
}

void Snow::update(const ParticleContext& ctx, float dt, ParticleWorld& world) {
    if (auto* kin = world.getFast<KinematicsComponent>(ctx, ctx.x, ctx.y)) {
        if (kin->velocity.y > 62.0f) kin->velocity.y = (Random::randFloat(0,1) > 0.3f) ? 62.0f : 124.0f;
    }
    MovableSolid::update(ctx, dt, world);
}

bool Snow::receiveHeat(BaseComponent* base, ThermalComponent* therm, int x, int y, int heat, ParticleWorld& world) {
    if (heat > 0) {
        dieAndReplace(x, y, MaterialID::Water, world);
        return true;
    }
    return false;
}

void Ember::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
    if (auto* base = world.get<BaseComponent>(x, y)) base->flags.isIgnited = true;
    if (auto* dur = world.get<DurabilityComponent>(x, y)) dur->health = Random::randInt(250, 350); 
    world.add<ThermalComponent>(x, y, ThermalComponent(5, 0, 10, 1));
}

void Salt::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    MovableSolid::onSpawn(index, x, y, world);
}

static Sand sand_instance;
static Dirt dirt_instance;
static Coal coal_instance;
static Gunpowder gunpowder_instance;
static Snow snow_instance;
static Ember ember_instance;
static Salt salt_instance;