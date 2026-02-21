#include "Particles/Particle.hpp"
#include "ParticleWorld.hpp"
#include "Random.hpp"

void Particle::die(ParticleWorld& world) {
    this->isDead = true;
    
    // Use the internal position variable
    // WARNING: 'setParticleAt' will destroy 'this' object immediately!
    // We capture coordinates in local vars just to be safe (though usually unnecessary if order is strict)
    int myX = this->position.x;
    int myY = this->position.y;

    world.setParticleAt(myX, myY, nullptr);
}
bool Particle::applyHeatToNeighborsIfIgnited(ParticleWorld& world) {
        // Java: if (!isEffectsFrame() || !shouldApplyHeat()) return false;
        // (Assuming isEffectsFrame() logic is handled externally or by world)
        if (!shouldApplyHeat()) return false;

        // Loop through 3x3 neighbors
        for (int x = position.x - 1; x <= position.x + 1; x++) {
            for (int y = position.y - 1; y <= position.y + 1; y++) {
                // Skip self
                if (x == position.x && y == position.y) continue;

                if (world.inBounds(x, y)) {
                    Particle* neighbor = world.getParticleAt(x, y);
                    if (neighbor != nullptr) {
                        neighbor->receiveHeat(heatFactor);
                    }
                }
            }
        }
        return true;
    }
    void Particle::spawnSparkIfIgnited(ParticleWorld& world) {
    if (!isIgnited) return;

    // SFML uses Y-Down, so "Up" is y - 1.
    int upX = position.x;
    int upY = position.y - 1;

    // 1. Check bounds first
    if (world.inBounds(upX, upY)) {
        
        // 2. Check if the spot is empty (NULL)
        // We use the new isEmpty check which simply looks for nullptr
        if (world.isEmpty(upX, upY)) {
            
            // 3. Determine what to spawn
            MaterialID elementToSpawn = (Random::randFloat(0.0f, 1.0f) > 0.1f) 
                                        ? MaterialID::Spark 
                                        : MaterialID::Smoke;

            // 4. Create the new particle using the factory
            auto newParticle = world.createParticleByType(elementToSpawn);

            // 5. Place it in the world
            if (newParticle) {
                // This will update the pointer grid AND the pixel buffer
                world.setParticleAt(upX, upY, std::move(newParticle));
            }
        }
    }
}
void Particle::dieAndReplace(MaterialID newType, ParticleWorld& world) {
    this->isDead = true;
    
    int myX = this->position.x;
    int myY = this->position.y;

    std::unique_ptr<Particle> newPart = world.createParticleByType(newType);
    
    if (newPart) {
        // Optional: Inherit velocity or temperature
        // newPart->velocity = this->velocity; 
        
        world.setParticleAt(myX, myY, std::move(newPart));
    } else {
        world.setParticleAt(myX, myY, nullptr);
    }
}