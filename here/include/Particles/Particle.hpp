#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "Constants.hpp"
#include "Random.hpp"
#include "ParticleWorld.hpp"
#include <iostream>
#include <cstdlib>
class ParticleWorld;

class Particle {
public:
    MaterialID id;
    sf::Vector2f velocity{0.0f, 0.0f};
    sf::Color color;
    const sf::Color defaultColor; 
    bool didColorChange = false;
    sf::Vector2i position{0, 0}; 

    bool hasBeenUpdatedThisFrame = false;
    bool isDead = false;
    bool discolored = false;
    
    // --- Java Ported Attributes ---
    int lifeSpan = 0; // Changed to int to match Java logic (frames)
    
    // Physics & State
    bool isFreeFalling = true;
    float frictionFactor = 0.5f;
    float inertialResistance = 0.1f;
    int mass = 100;
    int health = 500;
    
    // Density logic (from Gas.java)
    int density = 0; 
    int dispersionRate = 0;

    // Movement Tracking
    int stoppedMovingCount = 0;
    int stoppedMovingThreshold = 1;

    // Heat/Fire/Temperature system
    bool isIgnited = false;
    bool heated = false; 
    int temperature = 0; 
    int flammabilityResistance = 100;
    int resetFlammabilityResistance = 50;
    int heatFactor = 10;
    int coolingFactor = 5;
    int fireDamage = 3;
    int explosionResistance = 1;

    // Sub-pixel movement
    float xThreshold = 0.0f;
    float yThreshold = 0.0f;

    // --- Core Functions ---
static sf::Color getRandomColor(MaterialID id) {
    const auto& props = GetProps(id);
    return props.palette[std::rand() % props.palette.size()];
}
    Particle(MaterialID matId) : id(matId), color(getRandomColor(matId)),defaultColor(getRandomColor(matId)) {}
    virtual ~Particle() = default;
    virtual MaterialGroup getGroup() const = 0;
    
    virtual void die(ParticleWorld& world);
    virtual void dieAndReplace(MaterialID newType, ParticleWorld& world);

    virtual void update(int x, int y, float dt, ParticleWorld& world) = 0;
    virtual std::unique_ptr<Particle> clone() const = 0;

    // --- Interaction Logic (Ported from Element.java) ---

    virtual bool actOnNeighbor(int targetX, int targetY, int& currentX, int& currentY, ParticleWorld& world, bool isFinal, bool isFirst, int depth){return false;};
    // Specific interaction logic between two particles
    virtual bool actOnOther(Particle* other, ParticleWorld& world) {
        return false;
    }

    virtual bool corrode(ParticleWorld& world) {
        this->health -= 170;
        checkIfDead(world);
        return true;
    }

    virtual void checkIfDead(ParticleWorld& world) {
        if (this->health <= 0 && !isDead) {
            die(world);
        }
    }

    virtual bool applyHeatToNeighborsIfIgnited(ParticleWorld& world);
    virtual void spawnSparkIfIgnited(ParticleWorld& world);

    virtual void takeEffectsDamage(ParticleWorld& world) {
        if (isIgnited) {
            health -= fireDamage;
            // logic for surrounding check omitted for brevity, but variable exists
        }
        checkIfDead(world);
    }

    virtual bool didNotMove(sf::Vector2i formerLocation) {
        return formerLocation.x == this->position.x && formerLocation.y == this->position.y;
    }

    virtual bool shouldApplyHeat() {
        return isIgnited || heated;
    }

    virtual void checkLifeSpan(ParticleWorld& world) {
        if (lifeSpan > 0) {
            lifeSpan--;
            if (lifeSpan <= 0) {
                die(world);
            }
        }
    }

    virtual bool receiveHeat(int heat) {
        if (isIgnited) return false;
        flammabilityResistance -= (Random::randInt(0, heat));
        if (flammabilityResistance <= 0) {
            isIgnited = true;
            didColorChange = true;
        }
        return true;
    }

    virtual bool receiveCooling(int cooling) {
        if (isIgnited) {
            flammabilityResistance += cooling;
            if (flammabilityResistance > 0) {
                isIgnited = false;
                this->color = this->defaultColor;
                didColorChange = true;
            }
            return true;
        }
        return false;
    }

    virtual void magmatize(ParticleWorld& world, int damage) {
        this->health -= damage;
        checkIfDead(world);
    }

    virtual bool explode(ParticleWorld& world, int strength) {
        if (explosionResistance < strength) {
            if (Random::randFloat(0, 1) > 0.3f) {
                // dieAndReplace(MaterialID::ExplosionSpark, world); 
                die(world);
            } else {
                die(world);
            }
            return true;
        }
        return false;
    }

    virtual bool infect(ParticleWorld& world) {
        // Default from Element.java
        if (Random::randFloat(0, 1) > 0.95f) {
            dieAndReplace(MaterialID::SlimeMold, world); // if defined
            return true;
        }
        return false;
    }

    // Color Logic
    virtual bool stain(sf::Color newColor) {
        if (Random::randFloat(0, 1) > 0.2f || isIgnited) return false;
        this->color = newColor;
        this->discolored = true;
        this->didColorChange = true;
        return true;
        
    }

    virtual bool cleanColor() {
        if (!discolored || Random::randFloat(0, 1) > 0.2f) return false;
        this->color = this->defaultColor;
        this->didColorChange = true;
        this->discolored = false;
        return true;
    }
};