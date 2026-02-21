#include "Particles/Gas.hpp"
#include "Particles/Liquid.hpp"
#include "Particles/Solid.hpp"
#include "Constants.hpp"
#include "Random.hpp"
#include <algorithm> 
#include <cmath> 

Gas::Gas(MaterialID id, float buoy, float chaos) 
    : Particle(id), buoyancy(buoy), chaosLevel(chaos) {
    density = 1;
    dispersionRate = 1;
}

// --- CORE MOVEMENT ---
// --- CORE MOVEMENT ---
void Gas::update(int x, int y, float dt, ParticleWorld& world) 
{
    world.updateParticleColor(this, world);
    if (hasBeenUpdatedThisFrame) return;
    hasBeenUpdatedThisFrame = true;

    // 1. Calculate Velocity
    velocity.y = std::clamp(velocity.y - (GRAVITY * dt * buoyancy), -5.0f, 2.0f);
    
    // Apply Chaos
    velocity.x += Random::randFloat(-chaosLevel, chaosLevel);
    velocity.x = std::clamp(velocity.x, -3.0f, 3.0f);
    
    // Turbulence
    if (Random::chance(5)) {
        velocity.x += Random::randFloat(-1.0f, 1.0f);
        velocity.y += Random::randFloat(-0.5f, 0.5f);
    }

    int targetX = x + static_cast<int>(std::round(velocity.x));
    int targetY = y + static_cast<int>(std::round(velocity.y));
    
    // Helper: Tries to move to a specific spot
    auto tryMove = [&](int tx, int ty) -> bool {
        return actOnNeighbor(tx, ty, x, y, world, true, true, 0);
    };

    // Helper: Raycast - Checks if the line between (x,y) and (tx,ty) contains a Solid
    auto isPathBlocked = [&](int tx, int ty) -> bool {
        int dX = std::abs(tx - x);
        int dY = std::abs(ty - y);
        int sX = (x < tx) ? 1 : -1;
        int sY = (y < ty) ? 1 : -1;
        int err = dX - dY;
        
        int currX = x;
        int currY = y;

        while (true) {
            // Reached target (don't check target here, tryMove does that)
            if (currX == tx && currY == ty) break;

            // Don't check the starting pixel
            if (currX != x || currY != y) {
                Particle* p = world.getParticleAt(currX, currY);
                // If we hit a Solid (Movable or Immovable), the path is blocked
                if (p && (p->getGroup() == MaterialGroup::ImmovableSolid || 
                          p->getGroup() == MaterialGroup::MovableSolid)) {
                    return true;
                }
            }

            int e2 = 2 * err;
            if (e2 > -dY) { err -= dY; currX += sX; }
            if (e2 < dX)  { err += dX; currY += sY; }
        }
        return false;
    };

    // --- Movement Logic ---

    // 1. Try Direct Movement (High Velocity Jump)
    // CRITICAL FIX: We added !isPathBlocked(...) check here
    if (!isPathBlocked(targetX, targetY) && tryMove(targetX, targetY)) {
        // Moved successfully via long jump
    }
    // 2. Fallback: Try 1-pixel Upward Movement (Standard Rising)
    // If the long jump failed (hit a wall), this will naturally catch the gas next to the wall
    else if (tryMove(x, y - 1)) {
        velocity.y *= 0.5f; // Damping on collision
    }
    // 3. Fallback: Horizontal Movement (Drift/Dispersion)
    else {
        int direction = (velocity.x > 0) ? 1 : -1;
        if (std::abs(velocity.x) < 0.1f) direction = Random::randBool() ? 1 : -1;

        if (tryMove(x + direction, y)) {
            velocity.x += direction * 0.5f;
        } 
        else if (tryMove(x - direction, y)) {
            velocity.x -= direction * 0.5f;
        }
        // 4. Fallback: Diagonal Upward (Slide along ceilings)
        else if (tryMove(x + 1, y - 1)) {
             velocity.x += 0.1f;
        }
        else if (tryMove(x - 1, y - 1)) {
             velocity.x -= 0.1f;
        }
    }

    // 5. Friction
    velocity.x *= 0.8f;
    velocity.y *= 0.9f;

    // --- Side Effects ---
    applyHeatToNeighborsIfIgnited(world);
    spawnSparkIfIgnited(world);
    checkLifeSpan(world);
    takeEffectsDamage(world);
}

// --- INTERACTION LOGIC ---

bool Gas::actOnNeighbor(int targetX, int targetY, int& currentX, int& currentY, ParticleWorld& world, bool isFinal, bool isFirst, int depth) {
    
    if (!world.inBounds(targetX, targetY)) return false;
    Particle* neighbor = world.getParticleAt(targetX, targetY);

    // 1. actOnOther hook
    if(neighbor){
     bool acted = actOnOther(neighbor, world);
    if (acted) return true;   
    }

    // Logic for Empty or Particle (pass-through)
    if(!neighbor) {
        if (isFinal) {
            world.swapParticles(currentX, currentY, targetX, targetY);
            return true; 
        } 
        return false;
    } 
    // Logic for Gas (Density Check)
    else if (neighbor->getGroup() == MaterialGroup::Gas) {
        if (compareGasDensities(neighbor)) {
            swapGasForDensities(world, neighbor, targetX, targetY, currentX, currentY);
            return true; 
        }
        return false;
    }
    // Logic for Liquid (Gases rise through liquids)
    else if (neighbor->getGroup() == MaterialGroup::Liquid) {
        world.swapParticles(currentX, currentY, targetX, targetY);
        return true;
    }
    
    return false;
}

// --- HELPERS ---

bool Gas::compareGasDensities(Particle* neighbor) {
    // Standard logic: Heavier gas sinks. 
    return (density > neighbor->density && neighbor->position.y <= this->position.y);
}

void Gas::swapGasForDensities(ParticleWorld& world, Particle* neighbor, int neighborX, int neighborY, int& currentX, int& currentY) {
    this->velocity.y = 2.0f; // Force push down
    world.swapParticles(currentX, currentY, neighborX, neighborY);
}

// --- SPECIFIC CLASS IMPLEMENTATIONS ---

// STEAM
void Steam::checkLifeSpan(ParticleWorld& world) {
    if (lifeSpan > 0) {
        lifeSpan--;
        if (lifeSpan <= 0) {
            if (Random::randFloat(0, 1) > 0.5f) {
                die(world);
            } else {
                dieAndReplace(MaterialID::Water, world);
            }
        }
    }
}

// SPARK
bool Spark::actOnNeighbor(int targetX, int targetY, int& currentX, int& currentY, ParticleWorld& world, bool isFinal, bool isFirst, int depth) {
    
    if (!world.inBounds(targetX, targetY)) return false;
    Particle* neighbor = world.getParticleAt(targetX, targetY);
    if(neighbor){
     bool acted = actOnOther(neighbor, world);
    if (acted) return true;   
    }

    if (!neighbor) {
        if (isFinal) {
            world.swapParticles(currentX, currentY, targetX, targetY);
        }
        return true; 
    }
    else if (neighbor->id == MaterialID::Spark||neighbor->id==MaterialID::ExplosionSpark) {
        return false; 
    }
    else if (neighbor->id == MaterialID::Smoke) {
        neighbor->die(world);
        return false;
    }
    else if (neighbor->getGroup() == MaterialGroup::Liquid || neighbor->getGroup() == MaterialGroup::MovableSolid||neighbor->getGroup() == MaterialGroup::ImmovableSolid || neighbor->getGroup() == MaterialGroup::Gas) {
        neighbor->receiveHeat(this->heatFactor);
        this->die(world); 
        return true; 
    }
    return false;
}

// EXPLOSION SPARK
bool ExplosionSpark::actOnNeighbor(int targetX, int targetY, int& currentX, int& currentY, ParticleWorld& world, bool isFinal, bool isFirst, int depth) {
    
    if (!world.inBounds(targetX, targetY)) return false;
    Particle* neighbor = world.getParticleAt(targetX, targetY);

    if(neighbor){
     bool acted = actOnOther(neighbor, world);
    if (acted) return true;   
    }

    if (!neighbor) {
        if (isFinal) {
            world.swapParticles(currentX, currentY, targetX, targetY);
        }
        return true;
    }
    else if (neighbor->id == MaterialID::Spark||neighbor->id==MaterialID::ExplosionSpark) {
        return false;
    }
    else if (neighbor->id == MaterialID::Smoke) {
        neighbor->die(world);
        return false;
    }
    else if (neighbor->getGroup() == MaterialGroup::Liquid || neighbor->getGroup() == MaterialGroup::MovableSolid||neighbor->getGroup() == MaterialGroup::ImmovableSolid || neighbor->getGroup() == MaterialGroup::Gas) {
        neighbor->receiveHeat(this->heatFactor);
        this->die(world);
        return true;
    }
    return false;
}