#include "ParticleWorld.hpp"
#include <algorithm>
#include <cmath>

namespace SandSim {

// Implementation of utility methods from Particle class
void Particle::swapPositions(ParticleWorld* world, Particle* toSwap, int toSwapX, int toSwapY) {
    if (matrixX == toSwapX && matrixY == toSwapY) return;
    world->swapParticles(matrixX, matrixY, toSwapX, toSwapY);
}

void Particle::moveToLastValid(ParticleWorld* world, sf::Vector3f moveToLocation) {
    if (static_cast<int>(moveToLocation.x) == matrixX && static_cast<int>(moveToLocation.y) == matrixY) return;
    Particle* toSwap = world->get(moveToLocation.x, moveToLocation.y);
    if (toSwap) {
        swapPositions(world, toSwap, static_cast<int>(moveToLocation.x), static_cast<int>(moveToLocation.y));
    }
}

void Particle::moveToLastValidDieAndReplace(ParticleWorld* world, sf::Vector3f moveToLocation, MaterialID elementType) {
    world->setElementAtIndex(static_cast<int>(moveToLocation.x), static_cast<int>(moveToLocation.y), elementType);
    die(world);
}

void Particle::moveToLastValidAndSwap(ParticleWorld* world, Particle* toSwap, int toSwapX, int toSwapY, sf::Vector3f moveToLocation) {
    int moveToLocationX = static_cast<int>(moveToLocation.x);
    int moveToLocationY = static_cast<int>(moveToLocation.y);
    Particle* thirdNeighbor = world->get(moveToLocationX, moveToLocationY);
    
    if (this == thirdNeighbor || thirdNeighbor == toSwap) {
        swapPositions(world, toSwap, toSwapX, toSwapY);
        return;
    }
    
    if (this == toSwap) {
        swapPositions(world, thirdNeighbor, moveToLocationX, moveToLocationY);
        return;
    }
    
    // Three-way swap
    world->setElementAtIndex(matrixX, matrixY, thirdNeighbor->id);
    world->setElementAtIndex(toSwapX, toSwapY, id);
    world->setElementAtIndex(moveToLocationX, moveToLocationY, toSwap->id);
}

bool Particle::didNotMove(sf::Vector3f formerLocation) {
    return formerLocation.x == matrixX && formerLocation.y == matrixY;
}

bool Particle::hasNotMovedBeyondThreshold() {
    return stoppedMovingCount >= stoppedMovingThreshold;
}

void Particle::checkIfDead(ParticleWorld* world) {
    if (health <= 0) {
        die(world);
    }
}

void Particle::die(ParticleWorld* world) {
    isDead = true;
    world->setElementAtIndex(matrixX, matrixY, MaterialID::Empty);
}

void Particle::dieAndReplace(ParticleWorld* world, MaterialID newType) {
    isDead = true;
    world->setElementAtIndex(matrixX, matrixY, newType);
}

bool Particle::applyHeatToNeighborsIfIgnited(ParticleWorld* world) {
    if (!world->isEffectsFrame() || !shouldApplyHeat()) return false;
    
    for (int x = matrixX - 1; x <= matrixX + 1; x++) {
        for (int y = matrixY - 1; y <= matrixY + 1; y++) {
            if (!(x == 0 && y == 0)) {
                Particle* neighbor = world->get(x, y);
                if (neighbor != nullptr) {
                    neighbor->receiveHeat(world, heatFactor);
                }
            }
        }
    }
    return true;
}

void Particle::takeEffectsDamage(ParticleWorld* world) {
    if (!world->isEffectsFrame()) return;
    
    if (isIgnited) {
        health -= fireDamage;
        // Check if surrounded (simplified version)
        bool surrounded = true;
        for (int dx = -1; dx <= 1 && surrounded; dx++) {
            for (int dy = -1; dy <= 1 && surrounded; dy++) {
                if (dx == 0 && dy == 0) continue;
                Particle* neighbor = world->get(matrixX + dx, matrixY + dy);
                if (neighbor && neighbor->id == MaterialID::Empty) {
                    surrounded = false;
                }
            }
        }
        if (surrounded) {
            flammabilityResistance = resetFlammabilityResistance;
        }
        checkIfIgnited();
    }
    checkIfDead(world);
}

void Particle::spawnSparkIfIgnited(ParticleWorld* world) {
    if (!world->isEffectsFrame() || !isIgnited) return;
    
    Particle* upNeighbor = world->get(matrixX, matrixY + 1);
    if (upNeighbor != nullptr && upNeighbor->id == MaterialID::Empty) {
        MaterialID elementToSpawn = Random::randFloat(0.0f, 1.0f) > 0.1f ? MaterialID::Fire : MaterialID::Smoke;
        world->spawnElementByMatrix(matrixX, matrixY + 1, elementToSpawn);
    }
}

void Particle::setAdjacentNeighborsFreeFalling(ParticleWorld* world, int depth, sf::Vector3f lastValidLocation) {
    if (depth > 0) return;
    
    Particle* adjacentNeighbor1 = world->get(static_cast<int>(lastValidLocation.x) + 1, static_cast<int>(lastValidLocation.y));
    if (adjacentNeighbor1 && dynamic_cast<Solid*>(adjacentNeighbor1)) {
        bool wasSet = setElementFreeFalling(adjacentNeighbor1);
        if (wasSet) {
            world->reportToChunkActive(adjacentNeighbor1);
        }
    }
    
    Particle* adjacentNeighbor2 = world->get(static_cast<int>(lastValidLocation.x) - 1, static_cast<int>(lastValidLocation.y));
    if (adjacentNeighbor2 && dynamic_cast<Solid*>(adjacentNeighbor2)) {
        bool wasSet = setElementFreeFalling(adjacentNeighbor2);
        if (wasSet) {
            world->reportToChunkActive(adjacentNeighbor2);
        }
    }
}

bool Particle::setElementFreeFalling(Particle* element) {
    element->isFreeFalling = Random::randFloat(0.0f, 1.0f) > element->inertialResistance || element->isFreeFalling;
    return element->isFreeFalling;
}

std::unique_ptr<Particle> Particle::createByType(MaterialID type, int x, int y) {
    switch (type) {
        case MaterialID::Empty: return std::make_unique<EmptyParticle>(x, y);
        case MaterialID::Sand: return std::make_unique<SandParticle>(x, y);
        case MaterialID::Water: return std::make_unique<WaterParticle>(x, y);
        case MaterialID::Salt: return std::make_unique<SaltParticle>(x, y);
        case MaterialID::Wood: return std::make_unique<WoodParticle>(x, y);
        case MaterialID::Fire: return std::make_unique<FireParticle>(x, y);
        case MaterialID::Smoke: return std::make_unique<SmokeParticle>(x, y);
        case MaterialID::Ember: return std::make_unique<EmberParticle>(x, y);
        case MaterialID::Steam: return std::make_unique<SteamParticle>(x, y);
        case MaterialID::Gunpowder: return std::make_unique<GunpowderParticle>(x, y);
        case MaterialID::Oil: return std::make_unique<OilParticle>(x, y);
        case MaterialID::Lava: return std::make_unique<LavaParticle>(x, y);
        case MaterialID::Stone: return std::make_unique<StoneParticle>(x, y);
        case MaterialID::Acid: return std::make_unique<AcidParticle>(x, y);
        default: return std::make_unique<EmptyParticle>(x, y);
    }
}

// MovableSolid implementation (matches Java MovableSolid.step)
void MovableSolid::step(ParticleWorld* world) {
    stepped = true;
    
    if (!world->shouldElementInChunkStep(this)) return;
    
    // Apply gravity - this will gradually accelerate particles from rest
    vel = vel + world->getGravity();
    if (isFreeFalling) vel.x *= 0.9f;
    
    int yModifier = vel.y < 0 ? -1 : 1;
    int xModifier = vel.x < 0 ? -1 : 1;
    
    // Apply delta time scaling (60 FPS assumed)
    float deltaTime = 1.0f / 60.0f;
    float velYDeltaTimeFloat = std::abs(vel.y) * deltaTime;
    float velXDeltaTimeFloat = std::abs(vel.x) * deltaTime;
    
    int velXDeltaTime, velYDeltaTime;
    
    // Handle sub-pixel movement with thresholds
    if (velXDeltaTimeFloat < 1) {
        xThreshold += velXDeltaTimeFloat;
        velXDeltaTime = static_cast<int>(xThreshold);
        if (std::abs(velXDeltaTime) > 0) {
            xThreshold = 0;
        }
    } else {
        xThreshold = 0;
        velXDeltaTime = static_cast<int>(velXDeltaTimeFloat);
    }
    
    if (velYDeltaTimeFloat < 1) {
        yThreshold += velYDeltaTimeFloat;
        velYDeltaTime = static_cast<int>(yThreshold);
        if (std::abs(velYDeltaTime) > 0) {
            yThreshold = 0;
        }
    } else {
        yThreshold = 0;
        velYDeltaTime = static_cast<int>(velYDeltaTimeFloat);
    }
    
    // If no movement is calculated (common for newly spawned particles), return early
    if (velXDeltaTime == 0 && velYDeltaTime == 0) {
        // Still apply effects even if not moving
        applyHeatToNeighborsIfIgnited(world);
        takeEffectsDamage(world);
        spawnSparkIfIgnited(world);
        checkLifeSpan(world);
        modifyColor();
        return;
    }
    
    bool xDiffIsLarger = std::abs(velXDeltaTime) > std::abs(velYDeltaTime);
    
    int upperBound = std::max(std::abs(velXDeltaTime), std::abs(velYDeltaTime));
    int min = std::min(std::abs(velXDeltaTime), std::abs(velYDeltaTime));
    float slope = (min == 0 || upperBound == 0) ? 0 : (static_cast<float>(min + 1) / (upperBound + 1));
    
    sf::Vector3f formerLocation(matrixX, matrixY, 0);
    sf::Vector3f lastValidLocation(matrixX, matrixY, 0);
    
    for (int i = 1; i <= upperBound; i++) {
        int smallerCount = static_cast<int>(std::floor(i * slope));
        
        int yIncrease, xIncrease;
        if (xDiffIsLarger) {
            xIncrease = i;
            yIncrease = smallerCount;
        } else {
            yIncrease = i;
            xIncrease = smallerCount;
        }
        
        int modifiedMatrixY = matrixY + (yIncrease * yModifier);
        int modifiedMatrixX = matrixX + (xIncrease * xModifier);
        
        if (world->isWithinBounds(modifiedMatrixX, modifiedMatrixY)) {
            Particle* neighbor = world->get(modifiedMatrixX, modifiedMatrixY);
            if (neighbor == this) continue;
            
            bool stopped = actOnNeighboringParticle(neighbor, modifiedMatrixX, modifiedMatrixY, world, 
                                                  i == upperBound, i == 1, lastValidLocation, 0);
            if (stopped) break;
            
            lastValidLocation.x = modifiedMatrixX;
            lastValidLocation.y = modifiedMatrixY;
        } else {
            world->setElementAtIndex(matrixX, matrixY, MaterialID::Empty);
            return;
        }
    }
    
    applyHeatToNeighborsIfIgnited(world);
    takeEffectsDamage(world);
    spawnSparkIfIgnited(world);
    checkLifeSpan(world);
    modifyColor();
    
    stoppedMovingCount = didNotMove(formerLocation) && !isIgnited ? stoppedMovingCount + 1 : 0;
    if (stoppedMovingCount > stoppedMovingThreshold) {
        stoppedMovingCount = stoppedMovingThreshold;
    }
    
    if (world->useChunks) {
        if (isFreeFalling || isIgnited || !hasNotMovedBeyondThreshold()) {
            world->reportToChunkActive(this);
            world->reportToChunkActive(static_cast<int>(formerLocation.x), static_cast<int>(formerLocation.y));
        }
    }
}

bool MovableSolid::actOnNeighboringParticle(Particle* neighbor, int modifiedX, int modifiedY, 
                                          ParticleWorld* world, bool isFinal, bool isFirst, 
                                          sf::Vector3f lastValidLocation, int depth) {
    if (!neighbor || neighbor->id == MaterialID::Empty) {
        setAdjacentNeighborsFreeFalling(world, depth, lastValidLocation);
        if (isFinal) {
            isFreeFalling = true;
            swapPositions(world, neighbor, modifiedX, modifiedY);
        } else {
            return false;
        }
    } else if (dynamic_cast<Liquid*>(neighbor)) {
        if (depth > 0) {
            isFreeFalling = true;
            setAdjacentNeighborsFreeFalling(world, depth, lastValidLocation);
            swapPositions(world, neighbor, modifiedX, modifiedY);
        } else {
            isFreeFalling = true;
            moveToLastValidAndSwap(world, neighbor, modifiedX, modifiedY, lastValidLocation);
            return true;
        }
    } else if (dynamic_cast<Solid*>(neighbor)) {
        if (depth > 0) return true;
        if (isFinal) {
            moveToLastValid(world, lastValidLocation);
            return true;
        }
        if (isFreeFalling) {
            float absY = std::max(std::abs(vel.y) / 31.0f, 105.0f);
            vel.x = vel.x < 0 ? -absY : absY;
        }
        sf::Vector3f normalizedVel = vel;
        float length = std::sqrt(normalizedVel.x * normalizedVel.x + normalizedVel.y * normalizedVel.y + normalizedVel.z * normalizedVel.z);
        if (length > 0) {
            normalizedVel.x /= length;
            normalizedVel.y /= length;
            normalizedVel.z /= length;
        }
        
        int additionalX = getAdditional(normalizedVel.x);
        int additionalY = getAdditional(normalizedVel.y);
        
        Particle* diagonalNeighbor = world->get(matrixX + additionalX, matrixY + additionalY);
        if (isFirst) {
            vel.y = getAverageVelOrGravity(vel.y, neighbor->vel.y);
        } else {
            vel.y = -124;
        }
        
        neighbor->vel.y = vel.y;
        vel.x *= frictionFactor * neighbor->frictionFactor;
        
        if (diagonalNeighbor != nullptr) {
            bool stoppedDiagonally = actOnNeighboringParticle(diagonalNeighbor, matrixX + additionalX, matrixY + additionalY, 
                                                            world, true, false, lastValidLocation, depth + 1);
            if (!stoppedDiagonally) {
                isFreeFalling = true;
                return true;
            }
        }
        
        Particle* adjacentNeighbor = world->get(matrixX + additionalX, matrixY);
        if (adjacentNeighbor != nullptr && adjacentNeighbor != diagonalNeighbor) {
            bool stoppedAdjacently = actOnNeighboringParticle(adjacentNeighbor, matrixX + additionalX, matrixY, 
                                                            world, true, false, lastValidLocation, depth + 1);
            if (stoppedAdjacently) vel.x *= -1;
            if (!stoppedAdjacently) {
                isFreeFalling = false;
                return true;
            }
        }
        
        isFreeFalling = false;
        moveToLastValid(world, lastValidLocation);
        return true;
    } else if (dynamic_cast<Gas*>(neighbor)) {
        if (isFinal) {
            moveToLastValidAndSwap(world, neighbor, modifiedX, modifiedY, lastValidLocation);
            return true;
        }
        return false;
    }
    return false;
}

// ImmovableSolid implementation
void ImmovableSolid::step(ParticleWorld* world) {
    applyHeatToNeighborsIfIgnited(world);
    takeEffectsDamage(world);
    spawnSparkIfIgnited(world);
    modifyColor();
    // customElementFunctions would go here
}

// Liquid implementation (matches Java Liquid.step)
void Liquid::step(ParticleWorld* world) {
    stepped = true;
    
    if (!world->shouldElementInChunkStep(this)) return;
    
    // Apply gravity - this will gradually accelerate liquids from rest
    vel = vel + world->getGravity();
    if (isFreeFalling) vel.x *= 0.8f;
    
    int yModifier = vel.y < 0 ? -1 : 1;
    int xModifier = vel.x < 0 ? -1 : 1;
    
    float deltaTime = 1.0f / 60.0f;
    float velYDeltaTimeFloat = std::abs(vel.y) * deltaTime;
    float velXDeltaTimeFloat = std::abs(vel.x) * deltaTime;
    
    int velXDeltaTime, velYDeltaTime;
    
    if (velXDeltaTimeFloat < 1) {
        xThreshold += velXDeltaTimeFloat;
        velXDeltaTime = static_cast<int>(xThreshold);
        if (std::abs(velXDeltaTime) > 0) {
            xThreshold = 0;
        }
    } else {
        xThreshold = 0;
        velXDeltaTime = static_cast<int>(velXDeltaTimeFloat);
    }
    
    if (velYDeltaTimeFloat < 1) {
        yThreshold += velYDeltaTimeFloat;
        velYDeltaTime = static_cast<int>(yThreshold);
        if (std::abs(velYDeltaTime) > 0) {
            yThreshold = 0;
        }
    } else {
        yThreshold = 0;
        velYDeltaTime = static_cast<int>(velYDeltaTimeFloat);
    }
    
    // Early return if no movement calculated
    if (velXDeltaTime == 0 && velYDeltaTime == 0) {
        applyHeatToNeighborsIfIgnited(world);
        modifyColor();
        spawnSparkIfIgnited(world);
        checkLifeSpan(world);
        takeEffectsDamage(world);
        return;
    }
    
    bool xDiffIsLarger = std::abs(velXDeltaTime) > std::abs(velYDeltaTime);
    
    int upperBound = std::max(std::abs(velXDeltaTime), std::abs(velYDeltaTime));
    int min = std::min(std::abs(velXDeltaTime), std::abs(velYDeltaTime));
    float slope = (min == 0 || upperBound == 0) ? 0 : (static_cast<float>(min + 1) / (upperBound + 1));
    
    sf::Vector3f formerLocation(matrixX, matrixY, 0);
    sf::Vector3f lastValidLocation(matrixX, matrixY, 0);
    
    for (int i = 1; i <= upperBound; i++) {
        int smallerCount = static_cast<int>(std::floor(i * slope));
        
        int yIncrease, xIncrease;
        if (xDiffIsLarger) {
            xIncrease = i;
            yIncrease = smallerCount;
        } else {
            yIncrease = i;
            xIncrease = smallerCount;
        }
        
        int modifiedMatrixY = matrixY + (yIncrease * yModifier);
        int modifiedMatrixX = matrixX + (xIncrease * xModifier);
        
        if (world->isWithinBounds(modifiedMatrixX, modifiedMatrixY)) {
            Particle* neighbor = world->get(modifiedMatrixX, modifiedMatrixY);
            if (neighbor == this) continue;
            
            bool stopped = actOnNeighboringParticle(neighbor, modifiedMatrixX, modifiedMatrixY, world,
                                                  i == upperBound, i == 1, lastValidLocation, 0);
            if (stopped) break;
            
            lastValidLocation.x = modifiedMatrixX;
            lastValidLocation.y = modifiedMatrixY;
        } else {
            world->setElementAtIndex(matrixX, matrixY, MaterialID::Empty);
            return;
        }
    }
    
    applyHeatToNeighborsIfIgnited(world);
    modifyColor();
    spawnSparkIfIgnited(world);
    checkLifeSpan(world);
    takeEffectsDamage(world);
    
    stoppedMovingCount = didNotMove(formerLocation) ? stoppedMovingCount + 1 : 0;
    if (stoppedMovingCount > stoppedMovingThreshold) {
        stoppedMovingCount = stoppedMovingThreshold;
    }
    
    if (world->useChunks) {
        if (isIgnited || !hasNotMovedBeyondThreshold()) {
            world->reportToChunkActive(this);
            world->reportToChunkActive(static_cast<int>(formerLocation.x), static_cast<int>(formerLocation.y));
        }
    }
}

bool Liquid::actOnNeighboringParticle(Particle* neighbor, int modifiedX, int modifiedY, 
                                     ParticleWorld* world, bool isFinal, bool isFirst, 
                                     sf::Vector3f lastValidLocation, int depth) {
    bool acted = actOnOther(neighbor, world);
    if (acted) return true;
    
    if (!neighbor || neighbor->id == MaterialID::Empty) {
        setAdjacentNeighborsFreeFalling(world, depth, lastValidLocation);
        if (isFinal) {
            isFreeFalling = true;
            swapPositions(world, neighbor, modifiedX, modifiedY);
        } else {
            return false;
        }
    } else if (Liquid* liquidNeighbor = dynamic_cast<Liquid*>(neighbor)) {
        if (compareDensities(liquidNeighbor)) {
            if (isFinal) {
                swapLiquidForDensities(world, liquidNeighbor, modifiedX, modifiedY, lastValidLocation);
                return true;
            } else {
                lastValidLocation.x = modifiedX;
                lastValidLocation.y = modifiedY;
                return false;
            }
        }
        if (depth > 0) return true;
        if (isFinal) {
            moveToLastValid(world, lastValidLocation);
            return true;
        }
        if (isFreeFalling) {
            float absY = std::max(std::abs(vel.y) / 31.0f, 105.0f);
            vel.x = vel.x < 0 ? -absY : absY;
        }
        
        sf::Vector3f normalizedVel = vel;
        float length = std::sqrt(normalizedVel.x * normalizedVel.x + normalizedVel.y * normalizedVel.y);
        if (length > 0) {
            normalizedVel.x /= length;
            normalizedVel.y /= length;
        }
        
        int additionalX = getAdditional(normalizedVel.x);
        int additionalY = getAdditional(normalizedVel.y);
        
        int distance = additionalX * (Random::randFloat(0.0f, 1.0f) > 0.5f ? dispersionRate + 2 : dispersionRate - 1);
        
        Particle* diagonalNeighbor = world->get(matrixX + additionalX, matrixY + additionalY);
        if (isFirst) {
            vel.y = getAverageVelOrGravity(vel.y, neighbor->vel.y);
        } else {
            vel.y = -124;
        }
        
        neighbor->vel.y = vel.y;
        vel.x *= frictionFactor;
        
        if (diagonalNeighbor != nullptr) {
            bool stoppedDiagonally = iterateToAdditional(world, matrixX + additionalX, matrixY + additionalY, distance, lastValidLocation);
            if (!stoppedDiagonally) {
                isFreeFalling = true;
                return true;
            }
        }
        
        Particle* adjacentNeighbor = world->get(matrixX + additionalX, matrixY);
        if (adjacentNeighbor != nullptr && adjacentNeighbor != diagonalNeighbor) {
            bool stoppedAdjacently = iterateToAdditional(world, matrixX + additionalX, matrixY, distance, lastValidLocation);
            if (stoppedAdjacently) vel.x *= -1;
            if (!stoppedAdjacently) {
                isFreeFalling = false;
                return true;
            }
        }
        
        isFreeFalling = false;
        moveToLastValid(world, lastValidLocation);
        return true;
    } else if (dynamic_cast<Solid*>(neighbor)) {
        if (depth > 0) return true;
        if (isFinal) {
            moveToLastValid(world, lastValidLocation);
            return true;
        }
        if (isFreeFalling) {
            float absY = std::max(std::abs(vel.y) / 31.0f, 105.0f);
            vel.x = vel.x < 0 ? -absY : absY;
        }
        
        sf::Vector3f normalizedVel = vel;
        float length = std::sqrt(normalizedVel.x * normalizedVel.x + normalizedVel.y * normalizedVel.y);
        if (length > 0) {
            normalizedVel.x /= length;
            normalizedVel.y /= length;
        }
        
        int additionalX = getAdditional(normalizedVel.x);
        int additionalY = getAdditional(normalizedVel.y);
        
        int distance = additionalX * (Random::randFloat(0.0f, 1.0f) > 0.5f ? dispersionRate + 2 : dispersionRate - 1);
        
        Particle* diagonalNeighbor = world->get(matrixX + additionalX, matrixY + additionalY);
        if (isFirst) {
            vel.y = getAverageVelOrGravity(vel.y, neighbor->vel.y);
        } else {
            vel.y = -124;
        }
        
        neighbor->vel.y = vel.y;
        vel.x *= frictionFactor;
        
        if (diagonalNeighbor != nullptr) {
            bool stoppedDiagonally = iterateToAdditional(world, matrixX + additionalX, matrixY + additionalY, distance, lastValidLocation);
            if (!stoppedDiagonally) {
                isFreeFalling = true;
                return true;
            }
        }
        
        Particle* adjacentNeighbor = world->get(matrixX + additionalX, matrixY);
        if (adjacentNeighbor != nullptr) {
            bool stoppedAdjacently = iterateToAdditional(world, matrixX + additionalX, matrixY, distance, lastValidLocation);
            if (stoppedAdjacently) vel.x *= -1;
            if (!stoppedAdjacently) {
                isFreeFalling = false;
                return true;
            }
        }
        
        isFreeFalling = false;
        moveToLastValid(world, lastValidLocation);
        return true;
    } else if (dynamic_cast<Gas*>(neighbor)) {
        if (isFinal) {
            moveToLastValidAndSwap(world, neighbor, modifiedX, modifiedY, lastValidLocation);
            return true;
        }
        return false;
    }
    return false;
}

void Liquid::swapLiquidForDensities(ParticleWorld* world, Liquid* neighbor, int neighborX, int neighborY, sf::Vector3f lastValidLocation) {
    vel.y = -62;
    if (Random::randFloat(0.0f, 1.0f) > 0.8f) {
        vel.x *= -1;
    }
    moveToLastValidAndSwap(world, neighbor, neighborX, neighborY, lastValidLocation);
}

bool Liquid::iterateToAdditional(ParticleWorld* world, int startingX, int startingY, int distance, sf::Vector3f lastValid) {
    int distanceModifier = distance > 0 ? 1 : -1;
    sf::Vector3f lastValidLocation = lastValid;
    
    for (int i = 0; i <= std::abs(distance); i++) {
        int modifiedX = startingX + i * distanceModifier;
        Particle* neighbor = world->get(modifiedX, startingY);
        if (neighbor == nullptr) return true;
        
        bool acted = actOnOther(neighbor, world);
        if (acted) return false;
        
        bool isFirst = i == 0;
        bool isFinal = i == std::abs(distance);
        
        if (!neighbor || neighbor->id == MaterialID::Empty) {
            if (isFinal) {
                swapPositions(world, neighbor, modifiedX, startingY);
                return false;
            }
            lastValidLocation.x = modifiedX;
            lastValidLocation.y = startingY;
        } else if (Liquid* liquidNeighbor = dynamic_cast<Liquid*>(neighbor)) {
            if (isFinal && compareDensities(liquidNeighbor)) {
                swapLiquidForDensities(world, liquidNeighbor, modifiedX, startingY, lastValidLocation);
                return false;
            }
        } else if (dynamic_cast<Solid*>(neighbor)) {
            if (isFirst) return true;
            moveToLastValid(world, lastValidLocation);
            return false;
        }
    }
    return true;
}

// Gas implementation (matches Java Gas.step)
void Gas::step(ParticleWorld* world) {
    stepped = true;
    
    // Gases rise (subtract gravity instead of add)
    vel = vel - world->getGravity(); 
    vel.y = std::min(vel.y, 124.0f);
    if (vel.y == 124 && Random::randFloat(0.0f, 1.0f) > 0.7f) {
        vel.y = 64;
    }
    vel.x *= 0.9f;
    
    int yModifier = vel.y < 0 ? -1 : 1;
    int xModifier = vel.x < 0 ? -1 : 1;
    
    float deltaTime = 1.0f / 60.0f;
    float velYDeltaTimeFloat = std::abs(vel.y) * deltaTime;
    float velXDeltaTimeFloat = std::abs(vel.x) * deltaTime;
    
    int velXDeltaTime, velYDeltaTime;
    
    if (velXDeltaTimeFloat < 1) {
        xThreshold += velXDeltaTimeFloat;
        velXDeltaTime = static_cast<int>(xThreshold);
        if (std::abs(velXDeltaTime) > 0) {
            xThreshold = 0;
        }
    } else {
        xThreshold = 0;
        velXDeltaTime = static_cast<int>(velXDeltaTimeFloat);
    }
    
    if (velYDeltaTimeFloat < 1) {
        yThreshold += velYDeltaTimeFloat;
        velYDeltaTime = static_cast<int>(yThreshold);
        if (std::abs(velYDeltaTime) > 0) {
            yThreshold = 0;
        }
    } else {
        yThreshold = 0;
        velYDeltaTime = static_cast<int>(velYDeltaTimeFloat);
    }
    
    // Early return if no movement calculated
    if (velXDeltaTime == 0 && velYDeltaTime == 0) {
        applyHeatToNeighborsIfIgnited(world);
        modifyColor();
        spawnSparkIfIgnited(world);
        checkLifeSpan(world);
        takeEffectsDamage(world);
        
        if (world->useChunks && isIgnited) {
            world->reportToChunkActive(this);
        }
        return;
    }
    
    bool xDiffIsLarger = std::abs(velXDeltaTime) > std::abs(velYDeltaTime);
    
    int upperBound = std::max(std::abs(velXDeltaTime), std::abs(velYDeltaTime));
    int min = std::min(std::abs(velXDeltaTime), std::abs(velYDeltaTime));
    float slope = (min == 0 || upperBound == 0) ? 0 : (static_cast<float>(min + 1) / (upperBound + 1));
    
    sf::Vector3f lastValidLocation(matrixX, matrixY, 0);
    
    for (int i = 1; i <= upperBound; i++) {
        int smallerCount = static_cast<int>(std::floor(i * slope));
        
        int yIncrease, xIncrease;
        if (xDiffIsLarger) {
            xIncrease = i;
            yIncrease = smallerCount;
        } else {
            yIncrease = i;
            xIncrease = smallerCount;
        }
        
        int modifiedMatrixY = matrixY + (yIncrease * yModifier);
        int modifiedMatrixX = matrixX + (xIncrease * xModifier);
        
        if (world->isWithinBounds(modifiedMatrixX, modifiedMatrixY)) {
            Particle* neighbor = world->get(modifiedMatrixX, modifiedMatrixY);
            if (neighbor == this) continue;
            
            bool stopped = actOnNeighboringParticle(neighbor, modifiedMatrixX, modifiedMatrixY, world,
                                                  i == upperBound, i == 1, lastValidLocation, 0);
            if (stopped) break;
            
            lastValidLocation.x = modifiedMatrixX;
            lastValidLocation.y = modifiedMatrixY;
        } else {
            world->setElementAtIndex(matrixX, matrixY, MaterialID::Empty);
            return;
        }
    }
    
    applyHeatToNeighborsIfIgnited(world);
    modifyColor();
    spawnSparkIfIgnited(world);
    checkLifeSpan(world);
    takeEffectsDamage(world);
    
    if (world->useChunks && isIgnited) {
        world->reportToChunkActive(this);
    }
}

bool Gas::actOnNeighboringParticle(Particle* neighbor, int modifiedX, int modifiedY, 
                                  ParticleWorld* world, bool isFinal, bool isFirst, 
                                  sf::Vector3f lastValidLocation, int depth) {
    bool acted = actOnOther(neighbor, world);
    if (acted) return true;
    
    if (!neighbor || neighbor->id == MaterialID::Empty) {
        if (isFinal) {
            swapPositions(world, neighbor, modifiedX, modifiedY);
        } else {
            return false;
        }
    } else if (Gas* gasNeighbor = dynamic_cast<Gas*>(neighbor)) {
        if (compareGasDensities(gasNeighbor)) {
            swapGasForDensities(world, gasNeighbor, modifiedX, modifiedY, lastValidLocation);
            return false;
        }
        if (depth > 0) return true;
        if (isFinal) {
            moveToLastValid(world, lastValidLocation);
            return true;
        }
        
        vel.x = vel.x < 0 ? -62 : 62;
        
        sf::Vector3f normalizedVel = vel;
        float length = std::sqrt(normalizedVel.x * normalizedVel.x + normalizedVel.y * normalizedVel.y);
        if (length > 0) {
            normalizedVel.x /= length;
            normalizedVel.y /= length;
        }
        
        int additionalX = getAdditional(normalizedVel.x);
        int additionalY = getAdditional(normalizedVel.y);
        
        int distance = additionalX * (Random::randFloat(0.0f, 1.0f) > 0.5f ? dispersionRate + 2 : dispersionRate - 1);
        
        Particle* diagonalNeighbor = world->get(matrixX + additionalX, matrixY + additionalY);
        if (isFirst) {
            vel.y = getAverageVelOrGravity(vel.y, neighbor->vel.y);
        } else {
            vel.y = 124;
        }
        
        neighbor->vel.y = vel.y;
        vel.x *= frictionFactor;
        
        if (diagonalNeighbor != nullptr) {
            bool stoppedDiagonally = iterateToAdditional(world, matrixX + additionalX, matrixY, distance);
            if (!stoppedDiagonally) return true;
        }
        
        Particle* adjacentNeighbor = world->get(matrixX + additionalX, matrixY);
        if (adjacentNeighbor != nullptr && adjacentNeighbor != diagonalNeighbor) {
            bool stoppedAdjacently = iterateToAdditional(world, matrixX + additionalX, matrixY, distance);
            if (stoppedAdjacently) vel.x *= -1;
            if (!stoppedAdjacently) return true;
        }
        
        moveToLastValid(world, lastValidLocation);
        return true;
    } else if (dynamic_cast<Liquid*>(neighbor)) {
        // Gas vs Liquid interaction (similar to Gas vs Gas but simpler)
        if (depth > 0) return true;
        if (isFinal) {
            moveToLastValid(world, lastValidLocation);
            return true;
        }
        if (neighbor->isFreeFalling) return true;
        
        float absY = std::max(std::abs(vel.y) / 31.0f, 105.0f);
        vel.x = vel.x < 0 ? -absY : absY;
        
        sf::Vector3f normalizedVel = vel;
        float length = std::sqrt(normalizedVel.x * normalizedVel.x + normalizedVel.y * normalizedVel.y);
        if (length > 0) {
            normalizedVel.x /= length;
            normalizedVel.y /= length;
        }
        
        int additionalX = getAdditional(normalizedVel.x);
        int additionalY = getAdditional(normalizedVel.y);
        
        int distance = additionalX * (Random::randFloat(0.0f, 1.0f) > 0.5f ? dispersionRate + 2 : dispersionRate - 1);
        
        Particle* diagonalNeighbor = world->get(matrixX + additionalX, matrixY + additionalY);
        if (isFirst) {
            vel.y = getAverageVelOrGravity(vel.y, neighbor->vel.y);
        } else {
            vel.y = 124;
        }
        
        neighbor->vel.y = vel.y;
        vel.x *= frictionFactor;
        
        if (diagonalNeighbor != nullptr) {
            bool stoppedDiagonally = iterateToAdditional(world, matrixX + additionalX, matrixY + additionalY, distance);
            if (!stoppedDiagonally) return true;
        }
        
        Particle* adjacentNeighbor = world->get(matrixX + additionalX, matrixY);
        if (adjacentNeighbor != nullptr && adjacentNeighbor != diagonalNeighbor) {
            bool stoppedAdjacently = iterateToAdditional(world, matrixX + additionalX, matrixY, distance);
            if (stoppedAdjacently) vel.x *= -1;
            if (!stoppedAdjacently) return true;
        }
        
        moveToLastValid(world, lastValidLocation);
        return true;
    } else if (dynamic_cast<Solid*>(neighbor)) {
        // Gas vs Solid interaction
        if (depth > 0) return true;
        if (isFinal) {
            moveToLastValid(world, lastValidLocation);
            return true;
        }
        if (neighbor->isFreeFalling) return true;
        
        float absY = std::max(std::abs(vel.y) / 31.0f, 105.0f);
        vel.x = vel.x < 0 ? -absY : absY;
        
        sf::Vector3f normalizedVel = vel;
        float length = std::sqrt(normalizedVel.x * normalizedVel.x + normalizedVel.y * normalizedVel.y);
        if (length > 0) {
            normalizedVel.x /= length;
            normalizedVel.y /= length;
        }
        
        int additionalX = getAdditional(normalizedVel.x);
        int additionalY = getAdditional(normalizedVel.y);
        
        int distance = additionalX * (Random::randFloat(0.0f, 1.0f) > 0.5f ? dispersionRate + 2 : dispersionRate - 1);
        
        Particle* diagonalNeighbor = world->get(matrixX + additionalX, matrixY + additionalY);
        if (isFirst) {
            vel.y = getAverageVelOrGravity(vel.y, neighbor->vel.y);
        } else {
            vel.y = GRAVITY;
        }
        
        neighbor->vel.y = vel.y;
        vel.x *= frictionFactor;
        
        if (diagonalNeighbor != nullptr) {
            bool stoppedDiagonally = iterateToAdditional(world, matrixX + additionalX, matrixY + additionalY, distance);
            if (!stoppedDiagonally) return true;
        }
        
        Particle* adjacentNeighbor = world->get(matrixX + additionalX, matrixY);
        if (adjacentNeighbor != nullptr) {
            bool stoppedAdjacently = iterateToAdditional(world, matrixX + additionalX, matrixY, distance);
            if (stoppedAdjacently) vel.x *= -1;
            if (!stoppedAdjacently) return true;
        }
        
        moveToLastValid(world, lastValidLocation);
        return true;
    }
    return false;
}

void Gas::swapGasForDensities(ParticleWorld* world, Gas* neighbor, int neighborX, int neighborY, sf::Vector3f lastValidLocation) {
    vel.y = 62;
    moveToLastValidAndSwap(world, neighbor, neighborX, neighborY, lastValidLocation);
}

bool Gas::iterateToAdditional(ParticleWorld* world, int startingX, int startingY, int distance) {
    int distanceModifier = distance > 0 ? 1 : -1;
    sf::Vector3f lastValidLocation(matrixX, matrixY, 0);
    
    for (int i = 0; i <= std::abs(distance); i++) {
        Particle* neighbor = world->get(startingX + i * distanceModifier, startingY);
        bool acted = actOnOther(neighbor, world);
        if (acted) return false;
        
        bool isFirst = i == 0;
        bool isFinal = i == std::abs(distance);
        if (neighbor == nullptr) continue;
        
        if (!neighbor || neighbor->id == MaterialID::Empty) {
            if (isFinal) {
                swapPositions(world, neighbor, startingX + i * distanceModifier, startingY);
                return false;
            }
            lastValidLocation.x = startingX + i * distanceModifier;
            lastValidLocation.y = startingY;
            continue;
        } else if (Gas* gasNeighbor = dynamic_cast<Gas*>(neighbor)) {
            if (compareGasDensities(gasNeighbor)) {
                swapGasForDensities(world, gasNeighbor, startingX + i * distanceModifier, startingY, lastValidLocation);
                return false;
            }
        } else if (dynamic_cast<Solid*>(neighbor) || dynamic_cast<Liquid*>(neighbor)) {
            if (isFirst) return true;
            moveToLastValid(world, lastValidLocation);
            return false;
        }
    }
    return true;
}

// FireParticle step implementation
void FireParticle::step(ParticleWorld* world) {
    stepped = true; // Mark as stepped for this frame

    // The Gas::step logic isn't called, so we manually handle effects and lifespan
    applyHeatToNeighborsIfIgnited(world);
    takeEffectsDamage(world);
    checkLifeSpan(world); // This will call die() when lifeSpan runs out
    
    if (isDead) return;

    // Fire spreading logic (your version was good, this is just slightly refined)
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            
            Particle* neighbor = world->get(matrixX + dx, matrixY + dy);
            if (neighbor) {
                // Fire can be put out by water
                if (neighbor->id == MaterialID::Water) {
                    dieAndReplace(world, MaterialID::Smoke);
                    return; 
                }
                // Spread heat/fire to flammable neighbors
                neighbor->receiveHeat(world, heatFactor * 2);
            }
        }
    }

    // Randomly die and turn into smoke
    if (Random::randFloat(0.0f, 1.0f) < 0.05f) { // 5% chance per frame
        dieAndReplace(world, MaterialID::Smoke);
        return;
    }

    // Update color to flicker
    if (Random::randFloat(0.0f, 1.0f) > 0.5f) {
        modifyColor();
        world->updatePixelBuffer(matrixX, matrixY);
    }
}

} // namespace SandSim