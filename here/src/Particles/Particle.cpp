#include "Particles/Particle.hpp"
#include "ParticleWorld.hpp"
#include "Random.hpp"

// Initialize the global registry
Particle* MaterialRegistry[256] = { nullptr };

// --- SPAWN LOGIC ---
void Particle::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    // 1. Always add Base Component (Color, ID, Flags)
    BaseComponent base;
    base.id = this->id;
    base.color = getRandomColor(this->id);
    
    // Reset flags
    base.flags.hasBeenUpdatedThisFrame = false;
    base.flags.isDead = false;
    base.flags.didColorChange = false;
    base.flags.discolored = false;
    base.flags.heated = false;
    base.flags.isIgnited = false;
    
    world.baseManager.add(index, base);
    
    // NOTE: Subclasses (like Liquid) will call this, then add Kinematics/Fluid components.
}

// --- CORE ACTIONS ---

void Particle::die(uint32_t index, ParticleWorld& world) {
    // In the new system, dying means being removed from the ComponentManagers
    world.removeParticle(index); // This handles the bitmask and vector removal
}

void Particle::dieAndReplace(uint32_t index, int x, int y, MaterialID newType, ParticleWorld& world) {
    // 1. Remove current
    world.removeParticle(index);
    
    // 2. Spawn new (This will look up the new type in MaterialRegistry and call onSpawn)
    world.spawnParticle(newType, x, y);
}

// --- LOGIC IMPLEMENTATIONS ---

void Particle::checkIfDead(uint32_t index, ParticleWorld& world) {
    auto* dur = world.durabilityManager.get(index);
    if (!dur) return;

    // Check if health <= 0
    // Note: We check flags via base manager
    auto* base = world.baseManager.get(index);
    if (dur->health <= 0 && base && !base->flags.isDead) {
        die(index, world);
    }
}

bool Particle::corrode(uint32_t index, ParticleWorld& world) {
    auto* dur = world.durabilityManager.get(index);
    if (dur) {
        dur->health -= 170;
        checkIfDead(index, world);
        return true;
    }
    return false; // Cannot corrode something without health
}

void Particle::takeEffectsDamage(uint32_t index, ParticleWorld& world) {
    auto* base = world.baseManager.get(index);
    auto* dur = world.durabilityManager.get(index);
    auto* therm = world.thermalManager.get(index);

    if (base && base->flags.isIgnited && dur && therm) {
        dur->health -= therm->fireDamage; // Assuming fireDamage is stored in Thermal now
    }
    checkIfDead(index, world);
}

bool Particle::didNotMove(uint32_t index, int x, int y, ParticleWorld& world) {
    // Logic: Compare current x,y to stored prevX, prevY? 
    // Or we can check if velocity was zero.
    // For now, let's assume we store the previous position in Kinematics or we use 
    // stoppedMovingCount logic inside the specific Update functions.
    
    // Return false by default in base, simpler implementations usually handle this in Update.
    return false; 
}

bool Particle::shouldApplyHeat(uint32_t index, ParticleWorld& world) {
    auto* base = world.baseManager.get(index);
    if (!base) return false;
    return base->flags.isIgnited || base->flags.heated;
}

void Particle::checkLifeSpan(uint32_t index, ParticleWorld& world) {
    // Assuming LifeSpan is stored in StateManager or Durability
    // Let's assume Durability component has 'lifeSpan' for now, or you have a StateComponent
    // If you used the 'int state' approach:
    
    // Example using Durability for lifespan (common optimization):
    /*
    auto* state = world.stateManager.get(index); 
    if (state && state->lifeSpan > 0) {
        state->lifeSpan--;
        if (state->lifeSpan <= 0) die(index, world);
    }
    */
}

bool Particle::receiveHeat(uint32_t index, int heat, ParticleWorld& world) {
    auto* base = world.baseManager.get(index);
    auto* therm = world.thermalManager.get(index);

    if (!base || !therm) return false;
    if (base->flags.isIgnited) return false;

    therm->flammabilityResistance -= (Random::randInt(0, heat));
    if (therm->flammabilityResistance <= 0) {
        base->flags.isIgnited = true;
        base->flags.didColorChange = true;
    }
    return true;
}

bool Particle::receiveCooling(uint32_t index, int cooling, ParticleWorld& world) {
    auto* base = world.baseManager.get(index);
    auto* therm = world.thermalManager.get(index);

    if (base && therm && base->flags.isIgnited) {
        therm->flammabilityResistance += cooling;
        if (therm->flammabilityResistance > 0) {
            base->flags.isIgnited = false;
            // Restore default color logic would go here
            base->flags.didColorChange = true;
        }
        return true;
    }
    return false;
}

void Particle::magmatize(uint32_t index, int damage, ParticleWorld& world) {
    auto* dur = world.durabilityManager.get(index);
    if (dur) {
        dur->health -= damage;
        checkIfDead(index, world);
    }
}

bool Particle::explode(uint32_t index, int strength, ParticleWorld& world) {
    auto* dur = world.durabilityManager.get(index);
    if (!dur) return false;

    if (dur->explosionResistance < strength) {
        if (Random::randFloat(0, 1) > 0.3f) {
            die(index, world);
        } else {
            die(index, world);
        }
        return true;
    }
    return false;
}

bool Particle::infect(uint32_t index, ParticleWorld& world) {
    // Need current position. Since we don't pass X/Y here, we might need lookup or pass it.
    // For now assuming we can't spawn without coords. 
    // Update signature of infect to take x,y if needed, or handle in Update.
    return false;
}

bool Particle::stain(uint32_t index, sf::Color newColor, ParticleWorld& world) {
    auto* base = world.baseManager.get(index);
    if (!base) return false;

    if (Random::randFloat(0, 1) > 0.2f || base->flags.isIgnited) return false;
    
    base->color = newColor;
    base->flags.discolored = true;
    base->flags.didColorChange = true;
    return true;
}

bool Particle::cleanColor(uint32_t index, ParticleWorld& world) {
    auto* base = world.baseManager.get(index);
    if (!base) return false;

    if (!base->flags.discolored || Random::randFloat(0, 1) > 0.2f) return false;
    
    // Restore default color (requires looking up default color from Props or storing it)
    // base->color = GetProps(base->id).defaultColor; 
    base->flags.didColorChange = true;
    base->flags.discolored = false;
    return true;
}

bool Particle::applyHeatToNeighborsIfIgnited(uint32_t index, int x, int y, ParticleWorld& world) {
    if (!shouldApplyHeat(index, world)) return false;

    auto* therm = world.thermalManager.get(index);
    int heatAmt = therm ? therm->heatFactor : 10;

    for (int nx = x - 1; nx <= x + 1; nx++) {
        for (int ny = y - 1; ny <= y + 1; ny++) {
            if (nx == x && ny == y) continue;

            if (world.inBounds(nx, ny)) {
                uint32_t neighborIdx = world.getIndex(nx, ny);
                if (!world.isEmpty(nx, ny)) {
                    // Logic dispatch:
                    auto* nBase = world.baseManager.get(neighborIdx);
                    if (nBase) {
                        // Call the neighbor's logic class
                        MaterialRegistry[static_cast<int>(nBase->id)]->receiveHeat(neighborIdx, heatAmt, world);
                    }
                }
            }
        }
    }
    return true;
}

void Particle::spawnSparkIfIgnited(uint32_t index, int x, int y, ParticleWorld& world) {
    auto* base = world.baseManager.get(index);
    if (!base || !base->flags.isIgnited) return;

    int upX = x;
    int upY = y - 1;

    if (world.inBounds(upX, upY)) {
        if (world.isEmpty(upX, upY)) {
            MaterialID elementToSpawn = (Random::randFloat(0.0f, 1.0f) > 0.1f) 
                                        ? MaterialID::Spark 
                                        : MaterialID::Smoke;
            
            world.spawnParticle(elementToSpawn, upX, upY);
        }
    }
}