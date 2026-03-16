#include "Particles/Particle.hpp"
#include "ParticleWorld.hpp"
#include "Random.hpp"

// Initialize the global registry
Particle* MaterialRegistry[256] = { nullptr };

// --- SPAWN LOGIC ---
void Particle::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    world.add<BaseComponent>(x, y, BaseComponent(
        this->id, 
        getRandomColor(this->id),
        ParticleFlags(false, false, false, false, false, false, false, false, false)
    ));
}

// --- CORE ACTIONS ---

void Particle::die(int x, int y, ParticleWorld& world) {
    world.removeParticle(x, y);
}

void Particle::dieAndReplace(int x, int y, MaterialID newType, ParticleWorld& world) {
    world.removeParticle(x, y);
    world.spawnParticle(newType, x, y);
}

// --- LOGIC IMPLEMENTATIONS ---

void Particle::checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    if (dur && base && dur->health <= 0 && !base->flags.isDead) {
        die(x, y, world);
    }
}

bool Particle::corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    if (dur) {
        dur->health -= 170;
        checkIfDead(base, dur, x, y, world);
        return true;
    }
    return false;
}

void Particle::takeEffectsDamage(BaseComponent* base, DurabilityComponent* dur, ThermalComponent* therm, int x, int y, ParticleWorld& world) {
    if (base && base->flags.isIgnited && dur && therm) {
        dur->health -= therm->fireDamage;
    }
    checkIfDead(base, dur, x, y, world);
}

bool Particle::didNotMove(int currentX, int currentY, int originalX, int originalY) {
    return (currentX == originalX && currentY == originalY);
}

bool Particle::shouldApplyHeat(BaseComponent* base) {
    return base && (base->flags.isIgnited || base->flags.heated);
}

void Particle::checkLifeSpan(BaseComponent* base, DurabilityComponent* dur,int x, int y, ParticleWorld& world) {
    // Implement if LifeSpanComponent is added
}

bool Particle::receiveHeat(BaseComponent* base, ThermalComponent* therm,  int x, int y,int heat, ParticleWorld& world) {
    if (!base || !therm || base->flags.isIgnited) return false;

    therm->flammabilityResistance -= Random::randInt(0, heat);
    if (therm->flammabilityResistance <= 0) {
        base->flags.isIgnited = true;
        base->flags.didColorChange = true;
    }
    return true;
}

bool Particle::receiveCooling(BaseComponent* base, ThermalComponent* therm,int x, int y, int cooling, ParticleWorld& world) {
    if (base && therm && base->flags.isIgnited) {
        therm->flammabilityResistance += cooling;
        if (therm->flammabilityResistance > 0) {
            base->flags.isIgnited = false;
            base->flags.didColorChange = true;
        }
        return true;
    }
    return false;
}

bool Particle::magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) {
    if (dur) {
        dur->health -= damage;
        checkIfDead(base, dur, x, y, world);
        return true;
    }
    return false;
}

bool Particle::explode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int strength, ParticleWorld& world) {
    if (!dur) return false;

    if (dur->explosionResistance < strength) {
        die(x, y, world);
        return true;
    }
    return false;
}

bool Particle::infect(int x, int y, ParticleWorld& world) {
    return false;
}

bool Particle::stain(BaseComponent* base, int x, int y, sf::Color newColor, ParticleWorld& world) {
    if (!base || base->flags.isIgnited) return false;

    if (Random::randFloat(0.0f, 1.0f) < 0.2f) {
        world.setParticleColor(x, y, newColor);
        return true;
    }
    return false;
}

bool Particle::cleanColor(BaseComponent* base, int x, int y, ParticleWorld& world) {
    if (!base || !base->flags.discolored) return false;

    if (Random::randFloat(0.0f, 1.0f) < 0.2f) {
        base->flags.didColorChange = true;
        base->flags.discolored = false;
        return true;
    }
    return false;
}

bool Particle::applyHeatToNeighborsIfIgnited(BaseComponent* base, ThermalComponent* therm, int x, int y, ParticleWorld& world) {
    if (!shouldApplyHeat(base)) return false;

    int heatAmt = therm ? therm->heatFactor : 10;

    for (int nx = x - 1; nx <= x + 1; nx++) {
        for (int ny = y - 1; ny <= y + 1; ny++) {
            if (nx == x && ny == y) continue;

            if (world.inBounds(nx, ny)) {
                // Since this acts on completely unknown neighbor particles, we DO fetch here.
                if (auto* nBase = world.get<BaseComponent>(nx, ny)) {
                    Particle* neighborLogic = MaterialRegistry[static_cast<int>(nBase->id)];
                    if (neighborLogic) {
                        auto* nTherm = world.get<ThermalComponent>(nx, ny);
                        neighborLogic->receiveHeat(nBase, nTherm,   x,  y,heatAmt, world);
                    }
                }
            }
        }
    }
    return true;
}

void Particle::spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world) {
    if (!base || !base->flags.isIgnited) return;

    int upX = x;
    int upY = y - 1;

    if (world.inBounds(upX, upY) && world.isEmpty(upX, upY)) {
        MaterialID elementToSpawn = (Random::randFloat(0.0f, 1.0f) > 0.1f) 
                                    ? MaterialID::Spark 
                                    : MaterialID::Smoke;
        world.spawnParticle(elementToSpawn, upX, upY);
    }
}