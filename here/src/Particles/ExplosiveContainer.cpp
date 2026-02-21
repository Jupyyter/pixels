#include "particles/ExplosiveContainer.hpp"
#include "ParticleWorld.hpp" // <--- This provides the full definition of 'world'
#include <cmath>
#include <algorithm>

ExplosiveContainer::ExplosiveContainer(MaterialID contained, sf::Vector2f initialVelocity) 
    : Particle(MaterialID::ExplosiveContainer) 
{
    this->containedElementType = contained;
    this->velocity = initialVelocity;
    this->color = Particle::getRandomColor(contained);
    this->isFreeFalling = true; 
    this->frictionFactor = 0.0f;
}

MaterialGroup ExplosiveContainer::getGroup() const {
    return MaterialGroup::Gas;
}

std::unique_ptr<Particle> ExplosiveContainer::clone() const {
    return std::make_unique<ExplosiveContainer>(this->containedElementType, this->velocity);
}

void ExplosiveContainer::update(int x, int y, float dt, ParticleWorld& world) {
    if (hasBeenUpdatedThisFrame) return;
    hasBeenUpdatedThisFrame = true;

    // Gravity
    velocity.y += 9.81f * 20.0f * dt; 
    
    float maxVel = 500.0f;
    if (velocity.y < -maxVel) velocity.y = -maxVel;
    if (velocity.y > maxVel) velocity.y = maxVel;

    float deltaX = velocity.x * dt;
    float deltaY = velocity.y * dt;

    int steps = static_cast<int>(std::max(std::abs(deltaX), std::abs(deltaY)));
    
    if (steps == 0) {
        xThreshold += deltaX;
        yThreshold += deltaY;
        if (std::abs(xThreshold) >= 1.0f) {
            deltaX = xThreshold;
            xThreshold = 0;
            steps = 1;
        } else if (std::abs(yThreshold) >= 1.0f) {
            deltaY = yThreshold;
            yThreshold = 0;
            steps = 1;
        } else {
            return; 
        }
    }

    float stepX = deltaX / steps;
    float stepY = deltaY / steps;

    float currX = static_cast<float>(x);
    float currY = static_cast<float>(y);

    int lastValidX = x;
    int lastValidY = y;
    bool collided = false;

    for (int i = 0; i < steps; i++) {
        currX += stepX;
        currY += stepY;

        int nextX = static_cast<int>(std::round(currX));
        int nextY = static_cast<int>(std::round(currY));

        if (!world.inBounds(nextX, nextY)) {
            die(world); 
            return;
        }

        if (nextX == x && nextY == y) continue;

        Particle* neighbor = world.getParticleAt(nextX, nextY);

        if (neighbor == nullptr) {
            lastValidX = nextX;
            lastValidY = nextY;
        } else {
            MaterialGroup nGroup = neighbor->getGroup();
            if (nGroup == MaterialGroup::MovableSolid ||nGroup == MaterialGroup::ImmovableSolid || nGroup == MaterialGroup::Liquid) {
                collided = true;
                if (lastValidX != x || lastValidY != y) {
                    world.moveParticle(x, y, lastValidX, lastValidY);
                }
                detonate(lastValidX, lastValidY, world);
                return; 
            }
        }
    }

    if (!collided && (lastValidX != x || lastValidY != y)) {
        world.moveParticle(x, y, lastValidX, lastValidY);
    }
}

void ExplosiveContainer::detonate(int x, int y, ParticleWorld& world) {
    die(world); 

    if (world.isEmpty(x, y)) {
        spawnPayload(x, y, world);
    } else {
        int yIndex = 0;
        while (true) {
            int checkY = y + yIndex; 
            
            if (!world.inBounds(x, checkY) || std::abs(yIndex) > 10) break;

            if (world.isEmpty(x, checkY)) {
                spawnPayload(x, checkY, world);
                break;
            }
            
            if (yIndex <= 0) yIndex = -yIndex + 1;
            else yIndex = -yIndex;
        }
    }
}

void ExplosiveContainer::spawnPayload(int x, int y, ParticleWorld& world) {
    auto newPart = world.createParticleByType(containedElementType);
    if (newPart) {
        newPart->color = this->color;
        newPart->isIgnited = this->isIgnited;
        newPart->velocity = this->velocity; 
        newPart->velocity.x *= 0.3f; 
        newPart->velocity.y *= 0.3f;

        world.setParticleAt(x, y, std::move(newPart));
    }
}