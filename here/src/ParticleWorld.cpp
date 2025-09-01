#include "ParticleWorld.hpp"
#include <algorithm>
#include <cmath>

namespace SandSim {

void ParticleWorld::updateLiquidMovement(int x, int y, float dt, float horizontalChance, float velocityMultiplier) {
    auto& p = getParticleAt(x, y);
    
    // Apply gravity acceleration
    p.velocity.y = std::clamp(p.velocity.y + (GRAVITY * dt), -10.0f, 10.0f);
    
    // Use floating point movement with accumulation
    p.velocity.x *= 0.98f; // Add slight friction
    
    // Try to move with accumulated velocity
    int targetX = x + static_cast<int>(std::round(p.velocity.x));
    int targetY = y + static_cast<int>(std::round(p.velocity.y));
    
    if (inBounds(targetX, targetY) && isEmpty(targetX, targetY)) {
        swapParticles(x, y, targetX, targetY);
        return;
    }
    
    // Fall down with proper acceleration
    if (inBounds(x, y + 1) && isEmpty(x, y + 1)) {
        swapParticles(x, y, x, y + 1);
        return;
    }
    
    // Diagonal fall with velocity influence
    bool canFallLeft = inBounds(x - 1, y + 1) && isEmpty(x - 1, y + 1);
    bool canFallRight = inBounds(x + 1, y + 1) && isEmpty(x + 1, y + 1);
    
    if (canFallLeft && canFallRight) {
        // Choose based on horizontal velocity or random
        if (std::abs(p.velocity.x) > 0.1f) {
            if (p.velocity.x < 0) {
                swapParticles(x, y, x - 1, y + 1);
            } else {
                swapParticles(x, y, x + 1, y + 1);
            }
        } else {
            swapParticles(x, y, Random::randBool() ? x - 1 : x + 1, y + 1);
        }
        return;
    }
    
    if (canFallLeft) {
        p.velocity.x = std::clamp(p.velocity.x - velocityMultiplier, -5.0f, 5.0f);
        swapParticles(x, y, x - 1, y + 1);
        return;
    }
    
    if (canFallRight) {
        p.velocity.x = std::clamp(p.velocity.x + velocityMultiplier, -5.0f, 5.0f);
        swapParticles(x, y, x + 1, y + 1);
        return;
    }
    
    // Horizontal flow with pressure simulation
    bool canFlowLeft = inBounds(x - 1, y) && isEmpty(x - 1, y);
    bool canFlowRight = inBounds(x + 1, y) && isEmpty(x + 1, y);
    
    if (canFlowLeft || canFlowRight) {
        // Add pressure-based horizontal movement
        p.velocity.x += Random::randFloat(-0.5f, 0.5f);
        
        if (canFlowLeft && (p.velocity.x < 0 || !canFlowRight)) {
            if (Random::chance(static_cast<int>(horizontalChance))) {
                swapParticles(x, y, x - 1, y);
                return;
            }
        }
        
        if (canFlowRight && (p.velocity.x > 0 || !canFlowLeft)) {
            if (Random::chance(static_cast<int>(horizontalChance))) {
                swapParticles(x, y, x + 1, y);
                return;
            }
        }
    }
    
    // Reset velocity when blocked
    p.velocity.y *= 0.7f;
    p.velocity.x *= 0.8f;
}

void ParticleWorld::updateSolidMovement(int x, int y, float dt, bool canDisplaceLiquids) {
    auto& p = getParticleAt(x, y);
    
    // Apply gravity acceleration (key fix!)
    p.velocity.y = std::clamp(p.velocity.y + (GRAVITY * dt), -15.0f, 15.0f);
    
    // Add slight horizontal randomness when falling
    if (p.velocity.y > 2.0f) {
        p.velocity.x += Random::randFloat(-0.1f, 0.1f);
        p.velocity.x = std::clamp(p.velocity.x, -2.0f, 2.0f);
    }
    
    // Calculate movement based on accumulated velocity - REMOVED the artificial limit of 3
    int moveY = static_cast<int>(std::round(p.velocity.y));
    int moveX = static_cast<int>(std::round(p.velocity.x));
    
    // Try to move with the full calculated velocity (not limited to 3 pixels)
    int targetY = y + moveY;
    int targetX = x + moveX;
    
    // First try direct movement to target position
    if (inBounds(targetX, targetY)) {
        auto& target = getParticleAt(targetX, targetY);
        if (target.id == MaterialID::Empty || 
            (canDisplaceLiquids && (target.id == MaterialID::Water || target.id == MaterialID::Oil) && !target.hasBeenUpdatedThisFrame)) {
            
            if (target.id == MaterialID::Water || target.id == MaterialID::Oil) {
                // Displace liquid with splash effect
                target.velocity = {Random::randFloat(-3.0f, 3.0f), Random::randFloat(-2.0f, -5.0f)};
                target.hasBeenUpdatedThisFrame = true;
                
                // Find a spot for displaced liquid
                bool placed = false;
                for (int searchRadius = 1; searchRadius <= 5 && !placed; searchRadius++) {
                    for (int dy = -searchRadius; dy <= 0 && !placed; dy++) {
                        for (int dx = -searchRadius; dx <= searchRadius && !placed; dx++) {
                            if (isEmpty(x + dx, y + dy)) {
                                setParticleAt(x + dx, y + dy, target);
                                placed = true;
                            }
                        }
                    }
                }
            }
            
            swapParticles(x, y, targetX, targetY);
            return;
        }
    }
    
    // If direct movement failed, try step by step movement downward
    for (int step = 1; step <= std::abs(moveY); step++) {
        int testY = y + step;
        if (inBounds(x, testY)) {
            auto& below = getParticleAt(x, testY);
            if (below.id == MaterialID::Empty || 
                (canDisplaceLiquids && (below.id == MaterialID::Water || below.id == MaterialID::Oil) && !below.hasBeenUpdatedThisFrame)) {
                
                if (below.id == MaterialID::Water || below.id == MaterialID::Oil) {
                    // Displace liquid with splash effect
                    below.velocity = {Random::randFloat(-3.0f, 3.0f), Random::randFloat(-2.0f, -5.0f)};
                    below.hasBeenUpdatedThisFrame = true;
                    
                    // Find a spot for displaced liquid
                    bool placed = false;
                    for (int searchRadius = 1; searchRadius <= 5 && !placed; searchRadius++) {
                        for (int dy = -searchRadius; dy <= 0 && !placed; dy++) {
                            for (int dx = -searchRadius; dx <= searchRadius && !placed; dx++) {
                                if (isEmpty(x + dx, y + dy)) {
                                    setParticleAt(x + dx, y + dy, below);
                                    placed = true;
                                }
                            }
                        }
                    }
                }
                
                swapParticles(x, y, x, testY);
                return;
            } else {
                // Hit something - reduce velocity and try diagonal
                p.velocity.y *= 0.3f;
                break;
            }
        }
    }
    
    // Try diagonal movement if vertical movement failed
    if (moveY > 0) {
        int diagX = x + (moveX > 0 ? 1 : (moveX < 0 ? -1 : (Random::randBool() ? 1 : -1)));
        
        if (inBounds(diagX, y + 1)) {
            auto& diag = getParticleAt(diagX, y + 1);
            if (diag.id == MaterialID::Empty || 
                (canDisplaceLiquids && (diag.id == MaterialID::Water || diag.id == MaterialID::Oil))) {
                
                p.velocity.x = (diagX > x) ? std::abs(p.velocity.x) : -std::abs(p.velocity.x);
                swapParticles(x, y, diagX, y + 1);
                return;
            }
        }
    }
    
    // Settle in liquid if surrounded
    int lx, ly;
    if (isInLiquid(x, y, &lx, &ly) && Random::chance(15)) {
        swapParticles(x, y, lx, ly);
        p.velocity.y *= 0.5f;
    }
    
    // Apply friction when not moving
    p.velocity.x *= 0.9f;
    if (std::abs(p.velocity.x) < 0.1f) p.velocity.x = 0.0f;
}
void ParticleWorld::updateGasMovement(int x, int y, float dt, float buoyancy, float chaosLevel) {
    auto& p = getParticleAt(x, y);
    
    // Apply negative gravity (buoyancy) with chaos
    p.velocity.y = std::clamp(p.velocity.y - (GRAVITY * dt * buoyancy), -5.0f, 2.0f);
    
    // Add chaotic horizontal movement
    p.velocity.x += Random::randFloat(-chaosLevel, chaosLevel);
    p.velocity.x = std::clamp(p.velocity.x, -3.0f, 3.0f);
    
    // Apply random turbulence
    if (Random::chance(5)) {
        p.velocity.x += Random::randFloat(-1.0f, 1.0f);
        p.velocity.y += Random::randFloat(-0.5f, 0.5f);
    }
    
    // Calculate intended movement
    int targetX = x + static_cast<int>(std::round(p.velocity.x));
    int targetY = y + static_cast<int>(std::round(p.velocity.y));
    
    // Try direct movement first
    if (inBounds(targetX, targetY) && 
        (isEmpty(targetX, targetY) || 
         getParticleAt(targetX, targetY).id == MaterialID::Water ||
         getParticleAt(targetX, targetY).id == MaterialID::Oil)) {
        swapParticles(x, y, targetX, targetY);
        return;
    }
    
    // Try moving up (gases rise)
    if (inBounds(x, y - 1) && 
        (isEmpty(x, y - 1) || 
         getParticleAt(x, y - 1).id == MaterialID::Water ||
         getParticleAt(x, y - 1).id == MaterialID::Oil)) {
        swapParticles(x, y, x, y - 1);
        return;
    }
    
    // Try horizontal movement with random direction preference
    int direction = (Random::randFloat(-1.0f, 1.0f) > 0) ? 1 : -1;
    if (inBounds(x + direction, y) && 
        (isEmpty(x + direction, y) || 
         getParticleAt(x + direction, y).id == MaterialID::Water ||
         getParticleAt(x + direction, y).id == MaterialID::Oil)) {
        p.velocity.x += direction * 0.5f;
        swapParticles(x, y, x + direction, y);
        return;
    }
    
    // Try the opposite direction
    if (inBounds(x - direction, y) && 
        (isEmpty(x - direction, y) || 
         getParticleAt(x - direction, y).id == MaterialID::Water ||
         getParticleAt(x - direction, y).id == MaterialID::Oil)) {
        p.velocity.x -= direction * 0.5f;
        swapParticles(x, y, x - direction, y);
        return;
    }
    
    // Try diagonal up movement
    for (int dx = -1; dx <= 1; dx += 2) {
        if (inBounds(x + dx, y - 1) && 
            (isEmpty(x + dx, y - 1) || 
             getParticleAt(x + dx, y - 1).id == MaterialID::Water ||
             getParticleAt(x + dx, y - 1).id == MaterialID::Oil)) {
            swapParticles(x, y, x + dx, y - 1);
            return;
        }
    }
    
    // Apply friction when stuck
    p.velocity.x *= 0.8f;
    p.velocity.y *= 0.9f;
}

void ParticleWorld::updateSand(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Apply gravity acceleration like liquids for more realistic falling
    p.velocity.y = std::clamp(p.velocity.y + (GRAVITY * dt), -10.0f, 10.0f);
    
    updateSolidMovement(x, y, dt, true);
}

void ParticleWorld::updateWater(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    updateLiquidMovement(x, y, dt, 3.0f, 1.5f);
}

void ParticleWorld::updateSalt(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Dissolve in liquid
    int lx, ly;
    if (isInLiquid(x, y, &lx, &ly) && Random::chance(800)) {
        setParticleAt(x, y, Particle::createEmpty());
        return;
    }
    
    updateSolidMovement(x, y, dt, false);
}

void ParticleWorld::updateFire(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Die after some time or randomly
    if (p.lifeTime > 1.5f || (p.lifeTime > 0.3f && Random::chance(150))) {
        // Create embers when dying
        if (Random::chance(5)) {
            setParticleAt(x, y, Particle::createEmber());
        } else if (Random::chance(3)) {
            setParticleAt(x, y, Particle::createSmoke());
        } else {
            setParticleAt(x, y, Particle::createEmpty());
        }
        return;
    }
    
    // Update color BEFORE movement (fire stays in place)
    if (Random::chance(20)) {
        int colorVariant = Random::randInt(0, 3);
        switch (colorVariant) {
            case 0: p.color = sf::Color(255, 80, 20, 255); break;
            case 1: p.color = sf::Color(250, 150, 10, 255); break;
            case 2: p.color = sf::Color(200, 150, 0, 255); break;
            case 3: p.color = sf::Color(255, 200, 50, 255); break;
        }
    }
    
    // Fire stays in place and spreads
    // Ignite nearby materials
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny)) {
                auto& neighbor = getParticleAt(nx, ny);
                if ((neighbor.id == MaterialID::Wood || neighbor.id == MaterialID::Oil || 
                     neighbor.id == MaterialID::Gunpowder) && Random::chance(100)) {
                    setParticleAt(nx, ny, Particle::createFire());
                }
            }
        }
    }
    
    // Create steam when touching water
    int lx, ly;
    if (isInWater(x, y, &lx, &ly) && Random::chance(5)) {
        setParticleAt(x, y, Particle::createSteam());
        setParticleAt(lx, ly, Particle::createSteam());
        return;
    }
}

void ParticleWorld::updateSmoke(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Die after some time
    if (p.lifeTime > 15.0f) {
        setParticleAt(x, y, Particle::createEmpty());
        return;
    }
    
    // Update color BEFORE movement
    float lifeFactor = std::clamp((15.0f - p.lifeTime) / 15.0f, 0.1f, 1.0f);
    p.color.r = static_cast<uint8_t>(lifeFactor * 80);
    p.color.g = static_cast<uint8_t>(lifeFactor * 70);
    p.color.b = static_cast<uint8_t>(lifeFactor * 60);
    p.color.a = static_cast<uint8_t>(lifeFactor * 255);
    
    // Use gas movement for chaotic behavior
    updateGasMovement(x, y, dt, 0.8f, 1.2f);
}

void ParticleWorld::updateEmber(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Die after short time
    if (p.lifeTime > 0.2f && Random::chance(100)) {
        setParticleAt(x, y, Particle::createEmpty());
        return;
    }
    
    // Ember rises (using old fire movement logic)
    p.velocity.y = std::clamp(p.velocity.y - (GRAVITY * dt) * 0.2f, -5.0f, 0.0f);
    p.velocity.x = std::clamp(p.velocity.x + Random::randFloat(-0.01f, 0.01f), -0.5f, 0.5f);
    
    // Color variation for embers
    if (Random::chance(static_cast<int>(p.lifeTime * 100.0f + 1)) && Random::chance(200)) {
        int colorVariant = Random::randInt(0, 3);
        switch (colorVariant) {
            case 0: p.color = sf::Color(255, 80, 20, 255); break;
            case 1: p.color = sf::Color(250, 150, 10, 255); break;
            case 2: p.color = sf::Color(200, 150, 0, 255); break;
            case 3: p.color = sf::Color(100, 50, 2, 255); break;
        }
        int idx = computeIndex(x, y);
        int pixelIdx = idx * 4;
        pixelBuffer[pixelIdx] = p.color.r;
        pixelBuffer[pixelIdx + 1] = p.color.g;
        pixelBuffer[pixelIdx + 2] = p.color.b;
        pixelBuffer[pixelIdx + 3] = p.color.a;
    }
    
    // Ignite materials (embers can still ignite wood)
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny) && getParticleAt(nx, ny).id == MaterialID::Wood && Random::chance(150)) {
                setParticleAt(nx, ny, Particle::createFire());
            }
        }
    }
    
    // Create steam when touching water
    int lx, ly;
    if (isInWater(x, y, &lx, &ly)) {
        if (Random::randBool()) {
            // Create steam
            for (int i = -5; i > -5; --i) {
                for (int j = -5; j < 5; ++j) {
                    if (inBounds(x + j, y + i) && isEmpty(x + j, y + i)) {
                        setParticleAt(x + j, y + i, Particle::createSteam());
                        break;
                    }
                }
            }
            setParticleAt(lx, ly, Particle::createEmpty()); // Remove water
            setParticleAt(x, y, Particle::createEmpty()); // Remove ember
            return;
        }
    }
    
    // Movement (ember rises)
    int vx = static_cast<int>(p.velocity.x);
    int vy = static_cast<int>(p.velocity.y);
    
    if (inBounds(x + vx, y + vy) && (isEmpty(x + vx, y + vy) || 
        getParticleAt(x + vx, y + vy).id == MaterialID::Water ||
        getParticleAt(x + vx, y + vy).id == MaterialID::Smoke)) {
        swapParticles(x, y, x + vx, y + vy);
    }
    else if (inBounds(x, y - 1) && (isEmpty(x, y - 1) || 
             getParticleAt(x, y - 1).id == MaterialID::Water ||
             getParticleAt(x, y - 1).id == MaterialID::Smoke)) {
        swapParticles(x, y, x, y - 1);
    }
}

void ParticleWorld::updateSteam(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Die after some time
    if (p.lifeTime > 12.0f) {
        setParticleAt(x, y, Particle::createEmpty());
        return;
    }
    
    // Update color BEFORE movement
    float lifeFactor = std::clamp((12.0f - p.lifeTime) / 12.0f, 0.1f, 1.0f);
    p.color.a = static_cast<uint8_t>(lifeFactor * 255 * 0.8f);
    
    // Steam rises chaotically
    updateGasMovement(x, y, dt, 1.0f, 1.8f);
    
    // Condense into water when cooled
    if (p.lifeTime > 8.0f && Random::chance(200)) {
        setParticleAt(x, y, Particle::createWater());
        return;
    }
}

void ParticleWorld::updateGunpowder(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Check for fire first
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny) && getParticleAt(nx, ny).id == MaterialID::Fire) {
                // Explosion
                for (int ey = -4; ey <= 4; ey++) {
                    for (int ex = -4; ex <= 4; ex++) {
                        int explosionX = x + ex, explosionY = y + ey;
                        if (inBounds(explosionX, explosionY)) {
                            float distance = std::sqrt(ex * ex + ey * ey);
                            if (distance <= 4.0f && Random::chance(3)) {
                                setParticleAt(explosionX, explosionY, Particle::createFire());
                            }
                        }
                    }
                }
                return;
            }
        }
    }
    
    updateSolidMovement(x, y, dt, true);
}

void ParticleWorld::updateOil(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Check for fire
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny) && getParticleAt(nx, ny).id == MaterialID::Fire && Random::chance(30)) {
                setParticleAt(x, y, Particle::createFire());
                return;
            }
        }
    }
    
    // Oil flows like thick liquid
    updateLiquidMovement(x, y, dt, 5.0f, 1.0f);
}

void ParticleWorld::updateLava(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Apply gravity acceleration like other liquids but slower
    p.velocity.y = std::clamp(p.velocity.y + (GRAVITY * dt * 0.7f), -8.0f, 8.0f);
    
    // Ignite nearby materials
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny)) {
                auto& neighbor = getParticleAt(nx, ny);
                if ((neighbor.id == MaterialID::Wood || neighbor.id == MaterialID::Oil) && Random::chance(80)) {
                    setParticleAt(nx, ny, Particle::createFire());
                }
                else if (neighbor.id == MaterialID::Water && Random::chance(15)) {
                    setParticleAt(nx, ny, Particle::createSteam());
                }
            }
        }
    }
    
    // Lava movement (slower than water)
    int targetY = y + static_cast<int>(std::round(p.velocity.y * 0.5f));
    
    if (inBounds(x, targetY) && isEmpty(x, targetY)) {
        swapParticles(x, y, x, targetY);
        return;
    }
    
    if (inBounds(x, y + 1) && isEmpty(x, y + 1)) {
        swapParticles(x, y, x, y + 1);
        return;
    }
    
    // Horizontal flow
    if (Random::chance(8)) {
        int direction = Random::randBool() ? -1 : 1;
        if (inBounds(x + direction, y) && isEmpty(x + direction, y)) {
            swapParticles(x, y, x + direction, y);
            return;
        }
    }
    
    // Friction
    p.velocity.y *= 0.9f;
}

void ParticleWorld::updateAcid(int x, int y, float dt) {
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Dissolve nearby materials
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny)) {
                auto& neighbor = getParticleAt(nx, ny);
                if (neighbor.id != MaterialID::Empty && neighbor.id != MaterialID::Acid && 
                    neighbor.id != MaterialID::Stone && Random::chance(300)) {
                    setParticleAt(nx, ny, Particle::createEmpty());
                }
            }
        }
    }
    
    updateLiquidMovement(x, y, dt, 3.0f, 1.3f);
}

} // namespace SandSim