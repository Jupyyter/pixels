#pragma once
#include <SFML/Graphics.hpp>
#include "Constants.hpp"
#include "Random.hpp"
#include <memory>
#include <algorithm>
namespace SandSim {
    class ParticleWorld; // Forward declaration
    
    // Abstract base Particle class (equivalent to Element in Java)
    class Particle {
    public:
        int pixelX, pixelY;
        int matrixX, matrixY;
        sf::Vector3f vel{0.0f, -124.0f, 0.0f}; // Changed to Vector3f to match Java
        
        float frictionFactor = 1.0f;
        bool isFreeFalling = true;
        float inertialResistance = 0.0f;
        int stoppedMovingCount = 0;
        int stoppedMovingThreshold = 1;
        int mass = 100;
        int health = 500;
        int flammabilityResistance = 100;
        int resetFlammabilityResistance = 50;
        bool isIgnited = false;
        int heatFactor = 10;
        int fireDamage = 3;
        bool heated = false;
        int temperature = 0;
        int coolingFactor = 5;
        float lifeSpan = -1.0f; // -1 means infinite
        int explosionResistance = 1;
        int explosionRadius = 0;
        bool discolored = false;
        
        float xThreshold = 0.0f;
        float yThreshold = 0.0f;
        bool isDead = false;
        bool stepped = false; // Frame stepping control
        
        sf::Color color;
        MaterialID id;
        
        Particle(int x, int y, MaterialID materialId) : matrixX(x), matrixY(y), id(materialId) {
            pixelX = x; // Assuming 1:1 pixel mapping for simplicity
            pixelY = y;
            color = getColorForMaterial(materialId);
        }
        
        virtual ~Particle() = default;
        
        // Pure virtual methods that must be implemented by derived classes
        virtual void step(ParticleWorld* world) = 0;
        virtual bool actOnOther(Particle* other, ParticleWorld* world) { return false; }
        
    protected:
        virtual bool actOnNeighboringParticle(Particle* neighbor, int modifiedX, int modifiedY, 
                                            ParticleWorld* world, bool isFinal, bool isFirst, 
                                            sf::Vector3f lastValidLocation, int depth) = 0;
        
        sf::Color getColorForMaterial(MaterialID materialId) {
            switch (materialId) {
                case MaterialID::Empty: return MAT_COL_EMPTY;
                case MaterialID::Sand: return MAT_COL_SAND;
                case MaterialID::Water: return MAT_COL_WATER;
                case MaterialID::Salt: return MAT_COL_SALT;
                case MaterialID::Wood: return MAT_COL_WOOD;
                case MaterialID::Fire: return MAT_COL_FIRE;
                case MaterialID::Smoke: return MAT_COL_SMOKE;
                case MaterialID::Ember: return MAT_COL_EMBER;
                case MaterialID::Steam: return MAT_COL_STEAM;
                case MaterialID::Gunpowder: return MAT_COL_GUNPOWDER;
                case MaterialID::Oil: return MAT_COL_OIL;
                case MaterialID::Lava: return MAT_COL_LAVA;
                case MaterialID::Stone: return MAT_COL_STONE;
                case MaterialID::Acid: return MAT_COL_ACID;
                default: return MAT_COL_EMPTY;
            }
        }
        
        // Java-style movement helpers
        int getAdditional(float val) {
            if (val < -0.1f) {
                return static_cast<int>(std::floor(val));
            } else if (val > 0.1f) {
                return static_cast<int>(std::ceil(val));
            } else {
                return 0;
            }
        }
        
        float getAverageVelOrGravity(float vel, float otherVel) {
            if (otherVel > -125.0f) {
                return -124.0f;
            }
            float avg = (vel + otherVel) / 2.0f;
            if (avg > 0) {
                return avg;
            } else {
                return std::min(avg, -124.0f);
            }
        }
        
        void setAdjacentNeighborsFreeFalling(ParticleWorld* world, int depth, sf::Vector3f lastValidLocation);
        bool setElementFreeFalling(Particle* element);
        
    public:
        // Common utility methods from Java
        void swapPositions(ParticleWorld* world, Particle* toSwap, int toSwapX, int toSwapY);
        void moveToLastValid(ParticleWorld* world, sf::Vector3f moveToLocation);
        void moveToLastValidDieAndReplace(ParticleWorld* world, sf::Vector3f moveToLocation, MaterialID elementType);
        void moveToLastValidAndSwap(ParticleWorld* world, Particle* toSwap, int toSwapX, int toSwapY, sf::Vector3f moveToLocation);
        bool didNotMove(sf::Vector3f formerLocation);
        bool hasNotMovedBeyondThreshold();
        
        virtual bool receiveHeat(ParticleWorld* world, int heat) {
            if (isIgnited) return false;
            flammabilityResistance -= static_cast<int>(Random::randFloat(0.0f, 1.0f) * heat);
            checkIfIgnited();
            return true;
        }
        
        virtual bool receiveCooling(ParticleWorld* world, int cooling) {
            if (isIgnited) {
                flammabilityResistance += cooling;
                checkIfIgnited();
                return true;
            }
            return false;
        }
        
        virtual void checkIfIgnited() {
            if (flammabilityResistance <= 0) {
                isIgnited = true;
                modifyColor();
            } else {
                isIgnited = false;
                color = getColorForMaterial(id);
            }
        }
        
        virtual void modifyColor() {
            if (isIgnited) {
                // Random fire colors
                int variant = Random::randInt(0, 3);
                switch (variant) {
                    case 0: color = sf::Color(255, 80, 20, 255); break;
                    case 1: color = sf::Color(250, 150, 10, 255); break;
                    case 2: color = sf::Color(200, 150, 0, 255); break;
                    case 3: color = sf::Color(255, 200, 50, 255); break;
                }
            }
        }
        
        virtual bool corrode(ParticleWorld* world) {
            health -= 170;
            checkIfDead(world);
            return true;
        }
        
        virtual void checkIfDead(ParticleWorld* world);
        virtual void die(ParticleWorld* world);
        virtual void dieAndReplace(ParticleWorld* world, MaterialID newType);
        
        virtual bool stain(sf::Color stainColor) {
            if (Random::randFloat(0.0f, 1.0f) > 0.2f || isIgnited) return false;
            color = stainColor;
            discolored = true;
            return true;
        }
        
        virtual bool stain(float r, float g, float b, float a) {
            if (Random::randFloat(0.0f, 1.0f) > 0.2f || isIgnited) return false;
            color = sf::Color(
                std::clamp(static_cast<int>(color.r + r * 255), 0, 255),
                std::clamp(static_cast<int>(color.g + g * 255), 0, 255),
                std::clamp(static_cast<int>(color.b + b * 255), 0, 255),
                std::clamp(static_cast<int>(color.a + a * 255), 0, 255)
            );
            discolored = true;
            return true;
        }
        
        virtual bool cleanColor() {
            if (!discolored || Random::randFloat(0.0f, 1.0f) > 0.2f) return false;
            color = getColorForMaterial(id);
            discolored = false;
            return true;
        }
        
        virtual bool explode(ParticleWorld* world, int strength) {
            if (explosionResistance < strength) {
                if (Random::randFloat(0.0f, 1.0f) > 0.3f) {
                    dieAndReplace(world, MaterialID::Fire); // Using Fire instead of ExplosionSpark
                } else {
                    die(world);
                }
                return true;
            } else {
                darkenColor();
                return false;
            }
        }
        
        virtual void darkenColor() {
            color = sf::Color(
                static_cast<uint8_t>(color.r * 0.85f),
                static_cast<uint8_t>(color.g * 0.85f), 
                static_cast<uint8_t>(color.b * 0.85f),
                color.a
            );
            discolored = true;
        }
        
        virtual void darkenColor(float factor) {
            color = sf::Color(
                static_cast<uint8_t>(color.r * factor),
                static_cast<uint8_t>(color.g * factor), 
                static_cast<uint8_t>(color.b * factor),
                color.a
            );
            discolored = true;
        }
        
        virtual void checkLifeSpan(ParticleWorld* world) {
            if (lifeSpan > 0) {
                lifeSpan -= 1.0f/60.0f; // Assuming 60 FPS
                if (lifeSpan <= 0) {
                    die(world);
                }
            }
        }
        
        virtual void magmatize(ParticleWorld* world, int damage) {
            health -= damage;
            checkIfDead(world);
        }
        
        virtual bool infect(ParticleWorld* world) {
            if (Random::randFloat(0.0f, 1.0f) > 0.95f) {
                dieAndReplace(world, MaterialID::Smoke); // Using Smoke instead of SlimeMold
                return true;
            }
            return false;
        }
        
        bool applyHeatToNeighborsIfIgnited(ParticleWorld* world);
        bool shouldApplyHeat() { return isIgnited || heated; }
        void takeEffectsDamage(ParticleWorld* world);
        void spawnSparkIfIgnited(ParticleWorld* world);
        
        // Factory method for creating particles by type
        static std::unique_ptr<Particle> createByType(MaterialID type, int x, int y);
    };
    
    // Empty "particle" (equivalent to EmptyCell)
    class EmptyParticle : public Particle {
    public:
        EmptyParticle(int x, int y) : Particle(x, y, MaterialID::Empty) {}
        bool corrode(ParticleWorld* world) override {
        return false;
    }
        void step(ParticleWorld* world) override {}
        
    protected:
        bool actOnNeighboringParticle(Particle* neighbor, int modifiedX, int modifiedY, 
                                    ParticleWorld* world, bool isFinal, bool isFirst, 
                                    sf::Vector3f lastValidLocation, int depth) override {
            return false;
        }
    };
    
    // Base Solid class
    class Solid : public Particle {
    public:
        Solid(int x, int y, MaterialID materialId) : Particle(x, y, materialId) {}
    };
    
    // MovableSolid class (matches Java implementation)
    class MovableSolid : public Solid {
    public:
        MovableSolid(int x, int y, MaterialID materialId) : Solid(x, y, materialId) {
            stoppedMovingThreshold = 5;
            vel = sf::Vector3f(0.0f, -124.0f, 0.0f);
        }
        
        void step(ParticleWorld* world) override;
        
    protected:
        bool actOnNeighboringParticle(Particle* neighbor, int modifiedX, int modifiedY, 
                                    ParticleWorld* world, bool isFinal, bool isFirst, 
                                    sf::Vector3f lastValidLocation, int depth) override;
    };
    
    // ImmovableSolid class
    class ImmovableSolid : public Solid {
    public:
        ImmovableSolid(int x, int y, MaterialID materialId) : Solid(x, y, materialId) {
            isFreeFalling = false;
            vel = sf::Vector3f(0.0f, 0.0f, 0.0f);
        }
        
        void step(ParticleWorld* world) override;
        
    protected:
        bool actOnNeighboringParticle(Particle* neighbor, int modifiedX, int modifiedY, 
                                    ParticleWorld* world, bool isFinal, bool isFirst, 
                                    sf::Vector3f lastValidLocation, int depth) override {
            return true; // Immovable solids don't move
        }
    };
    
    // Base Liquid class (matches Java implementation)
    class Liquid : public Particle {
    public:
        int density = 5;
        int dispersionRate = 5;
        
        Liquid(int x, int y, MaterialID materialId) : Particle(x, y, materialId) {
            stoppedMovingThreshold = 10;
            vel = sf::Vector3f(0.0f, 124.0f, 0.0f);
        }
        
        void step(ParticleWorld* world) override;
        
        // Liquids don't get stained or discolored (matching Java)
        bool stain(sf::Color stainColor) override { return false; }
        bool stain(float r, float g, float b, float a) override { return false; }
        void darkenColor() override {}
        void darkenColor(float factor) override {}
        bool infect(ParticleWorld* world) override { return false; }
        
    protected:
        bool actOnNeighboringParticle(Particle* neighbor, int modifiedX, int modifiedY, 
                                    ParticleWorld* world, bool isFinal, bool isFirst, 
                                    sf::Vector3f lastValidLocation, int depth) override;
        
        bool compareDensities(Liquid* neighbor) {
            return density > neighbor->density && neighbor->matrixY <= matrixY;
        }
        
        void swapLiquidForDensities(ParticleWorld* world, Liquid* neighbor, int neighborX, int neighborY, sf::Vector3f lastValidLocation);
        bool iterateToAdditional(ParticleWorld* world, int startingX, int startingY, int distance, sf::Vector3f lastValid);
    };
    
    // Base Gas class (matches Java implementation)
    class Gas : public Particle {
    public:
        int density = 3;
        int dispersionRate = 2;
        
        Gas(int x, int y, MaterialID materialId) : Particle(x, y, materialId) {vel = sf::Vector3f(0.0f, -124.0f, 0.0f);}
        
        void step(ParticleWorld* world) override;
        
        // Gases don't corrode, stain, darken, or spawn sparks (matching Java)
        bool corrode(ParticleWorld* world) override { return false; }
        bool stain(sf::Color stainColor) override { return false; }
        bool stain(float r, float g, float b, float a) override { return false; }
        void darkenColor() override {}
        void darkenColor(float factor) override {}
        //void spawnSparkIfIgnited(ParticleWorld* world) override {}
        bool infect(ParticleWorld* world) override { return false; }
        
    protected:
        bool actOnNeighboringParticle(Particle* neighbor, int modifiedX, int modifiedY, 
                                    ParticleWorld* world, bool isFinal, bool isFirst, 
                                    sf::Vector3f lastValidLocation, int depth) override;
        
        bool compareGasDensities(Gas* neighbor) {
            return density > neighbor->density && neighbor->matrixY <= matrixY;
        }
        
        void swapGasForDensities(ParticleWorld* world, Gas* neighbor, int neighborX, int neighborY, sf::Vector3f lastValidLocation);
        bool iterateToAdditional(ParticleWorld* world, int startingX, int startingY, int distance);
    };
    
    // Concrete particle implementations
    class SandParticle : public MovableSolid {
    public:
        SandParticle(int x, int y) : MovableSolid(x, y, MaterialID::Sand) {
            frictionFactor = 0.9f;
            inertialResistance = 0.1f;
            mass = 150;
            color.r += Random::randInt(-20, 20);
            color.g += Random::randInt(-20, 20);
            color.b += Random::randInt(-20, 20);
        }
        
        bool receiveHeat(ParticleWorld* world, int heat) override { return false; }
    };
    
    class WaterParticle : public Liquid {
    public:
        WaterParticle(int x, int y) : Liquid(x, y, MaterialID::Water) {
            inertialResistance = 0.0f;
            mass = 100;
            frictionFactor = 1.0f;
            density = 5;
            dispersionRate = 5;
            coolingFactor = 5;
            explosionResistance = 0;
            color.b += Random::randInt(-30, 30);
        }
        
        bool receiveHeat(ParticleWorld* world, int heat) override {
            dieAndReplace(world, MaterialID::Steam);
            return true;
        }
        
        bool actOnOther(Particle* other, ParticleWorld* world) override {
            other->cleanColor();
            if (other->shouldApplyHeat()) {
                other->receiveCooling(world, coolingFactor);
                coolingFactor--;
                if (coolingFactor <= 0) {
                    dieAndReplace(world, MaterialID::Steam);
                    return true;
                }
                return false;
            }
            return false;
        }
        
        bool explode(ParticleWorld* world, int strength) override {
            if (explosionResistance < strength) {
                dieAndReplace(world, MaterialID::Steam);
                return true;
            } else {
                return false;
            }
        }
    };
    
    class OilParticle : public Liquid {
    public:
        OilParticle(int x, int y) : Liquid(x, y, MaterialID::Oil) {
            inertialResistance = 0.0f;
            mass = 75;
            frictionFactor = 1.0f;
            density = 4;
            dispersionRate = 4;
            flammabilityResistance = 5;
            resetFlammabilityResistance = 2;
            fireDamage = 10;
            temperature = 10;
            health = 1000;
        }
    };
    
    class LavaParticle : public Liquid {
    public:
        int magmatizeDamage;
        
        LavaParticle(int x, int y) : Liquid(x, y, MaterialID::Lava) {
            inertialResistance = 0.0f;
            mass = 100;
            frictionFactor = 1.0f;
            density = 10;
            dispersionRate = 1;
            temperature = 10;
            heated = true;
            magmatizeDamage = Random::randInt(1, 10);
            color.r += Random::randInt(-30, 30);
            color.g += Random::randInt(-10, 10);
        }
        
        bool receiveHeat(ParticleWorld* world, int heat) override { return false; }
        
        void checkIfDead(ParticleWorld* world) override {
            if (temperature <= 0) {
                dieAndReplace(world, MaterialID::Stone);
                // TODO: Convert nearby liquids to stone
            }
            if (health <= 0) {
                die(world);
            }
        }
        
        bool actOnOther(Particle* other, ParticleWorld* world) override {
            other->magmatize(world, magmatizeDamage);
            return false;
        }
        
        void magmatize(ParticleWorld* world, int damage) override {}
        
        bool receiveCooling(ParticleWorld* world, int cooling) override {
            temperature -= cooling;
            checkIfDead(world);
            return true;
        }
    };
    
    class AcidParticle : public Liquid {
    public:
        int corrosionCount = 3;
        
        AcidParticle(int x, int y) : Liquid(x, y, MaterialID::Acid) {
            inertialResistance = 0.0f;
            mass = 50;
            frictionFactor = 1.0f;
            density = 2;
            dispersionRate = 2;
            color.g += Random::randInt(-30, 30);
        }
        
        bool actOnOther(Particle* other, ParticleWorld* world) override {
            other->stain(-1.0f, 1.0f, -1.0f, 0.0f);
            bool corroded = other->corrode(world);
            if (corroded) {
                corrosionCount--;
                if (corrosionCount <= 0) {
                    dieAndReplace(world, MaterialID::Smoke); // Using Smoke instead of FlammableGas
                    return true;
                }
            }
            return false;
        }
        
        bool corrode(ParticleWorld* world) override { return false; }
        bool receiveHeat(ParticleWorld* world, int heat) override { return false; }
    };
    
    class GunpowderParticle : public MovableSolid {
    public:
        int ignitedCount = 0;
        int ignitedThreshold = 7;
        
        GunpowderParticle(int x, int y) : MovableSolid(x, y, MaterialID::Gunpowder) {
            frictionFactor = 0.4f;
            inertialResistance = 0.8f;
            mass = 200;
            flammabilityResistance = 10;
            resetFlammabilityResistance = 35;
            explosionRadius = 15;
            fireDamage = 3;
        }
        
        void step(ParticleWorld* world) override {
            MovableSolid::step(world);
            if (isIgnited) {
                ignitedCount++;
                if (ignitedCount >= ignitedThreshold) {
                    // TODO: Add explosion to world - matrix.addExplosion(15, 10, this);
                    dieAndReplace(world, MaterialID::Fire);
                }
            }
        }
    };
    
    class EmberParticle : public MovableSolid {
    public:
        EmberParticle(int x, int y) : MovableSolid(x, y, MaterialID::Ember) {
            frictionFactor = 0.9f;
            inertialResistance = 0.99f;
            mass = 200;
            isIgnited = true;
            health = Random::randInt(250, 350);
            temperature = 5;
            flammabilityResistance = 0;
            resetFlammabilityResistance = 20;
        }
        
        bool infect(ParticleWorld* world) override { return false; }
    };
    
    class WoodParticle : public ImmovableSolid {
    public:
        WoodParticle(int x, int y) : ImmovableSolid(x, y, MaterialID::Wood) {
            frictionFactor = 0.5f;
            inertialResistance = 1.1f;
            mass = 500;
            health = Random::randInt(100, 200);
            flammabilityResistance = 40;
            resetFlammabilityResistance = 25;
            color.r += Random::randInt(-10, 10);
            color.g += Random::randInt(-10, 10);
        }
        
        void checkIfDead(ParticleWorld* world) override {
            if (health <= 0) {
                if (isIgnited && Random::randFloat(0.0f, 1.0f) > 0.95f) {
                    dieAndReplace(world, MaterialID::Ember);
                } else {
                    die(world);
                }
            }
        }
    };
    
    class StoneParticle : public ImmovableSolid {
    public:
        StoneParticle(int x, int y) : ImmovableSolid(x, y, MaterialID::Stone) {
            frictionFactor = 0.5f;
            inertialResistance = 1.1f;
            mass = 500;
            explosionResistance = 4;
            color.r += Random::randInt(-20, 20);
            color.g += Random::randInt(-20, 20);
            color.b += Random::randInt(-20, 20);
        }
        
        bool receiveHeat(ParticleWorld* world, int heat) override { return false; }
    };
    
    class SmokeParticle : public Gas {
    public:
        SmokeParticle(int x, int y) : Gas(x, y, MaterialID::Smoke) {
            inertialResistance = 0.0f;
            mass = 1;
            frictionFactor = 1.0f;
            density = 3;
            dispersionRate = 2;
            lifeSpan = (Random::randInt(450, 700)) / 60.0f; // Convert frames to seconds
        }
        
        bool receiveHeat(ParticleWorld* world, int heat) override { return false; }
    };
    
    class SteamParticle : public Gas {
    public:
        SteamParticle(int x, int y) : Gas(x, y, MaterialID::Steam) {
            inertialResistance = 0.0f;
            mass = 1;
            frictionFactor = 1.0f;
            density = 5;
            dispersionRate = 2;
            lifeSpan = (Random::randInt(1000, 3000)) / 60.0f; // Convert frames to seconds
        }
        
        void checkLifeSpan(ParticleWorld* world) override {
            if (lifeSpan > 0) {
                lifeSpan -= 1.0f/60.0f;
                if (lifeSpan <= 0) {
                    if (Random::randBool()) {
                        die(world);
                    } else {
                        dieAndReplace(world, MaterialID::Water);
                    }
                }
            }
        }
        
        bool receiveHeat(ParticleWorld* world, int heat) override { return false; }
    };
    
    class SaltParticle : public MovableSolid {
    public:
        SaltParticle(int x, int y) : MovableSolid(x, y, MaterialID::Salt) {
            frictionFactor = 0.8f;
            inertialResistance = 0.2f;
            mass = 120;
        }
    };
    
    class FireParticle : public Gas {
    public:float lifeTime = 0.0f;
        FireParticle(int x, int y) : Gas(x, y, MaterialID::Fire) {
            vel = sf::Vector3f(0.0f, 0.0f, 0.0f); // Fire doesn't move much
            inertialResistance = 0.0f;
            mass = 1;
            frictionFactor = 1.0f;
            density = 1;
            dispersionRate = 1;
            lifeSpan = Random::randFloat(1.0f, 3.0f);
            isIgnited = true;
            
            int colorVariant = Random::randInt(0, 3);
            switch (colorVariant) {
                case 0: color = sf::Color(255, 80, 20, 255); break;
                case 1: color = sf::Color(250, 150, 10, 255); break;
                case 2: color = sf::Color(200, 150, 0, 255); break;
                case 3: color = sf::Color(255, 200, 50, 255); break;
            }
        }
        
        void step(ParticleWorld* world) override;
    };
}