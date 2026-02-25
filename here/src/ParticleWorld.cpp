#include "ParticleWorld.hpp"
#include "Particles/Particle.hpp" // For MaterialRegistry
#include <algorithm>
#include <cmath>
#include <string>
#include <fstream>
#include <filesystem>
#include "RigidBody.hpp"
#include "Particles/Explosion.hpp"
#include <cstring>

ParticleWorld::~ParticleWorld() = default;

// A unique 4-byte header to identify your save files
const char MAGIC_HEADER[4] = {'S', 'A', 'N', 'D'};

ParticleWorld::ParticleWorld(unsigned int w, unsigned int h, const std::string& worldFile)
    : width(w), height(h), frameCounter(0) {
    
    // Resize pixel buffer
    pixelBuffer.resize(width * height * 4);
    
    // Initialize Managers
    size_t area = width * height;
    baseManager.init(area);
    kinematicsManager.init(area);
    durabilityManager.init(area);
    thermalManager.init(area);
    fluidManager.init(area);

    rigidBodySystem = std::make_unique<RigidBodySystem>(width, height);

    if (!worldFile.empty() && std::filesystem::exists(worldFile)) {
        loadWorld(worldFile); 
    } else {
        clear();
    }
}

void ParticleWorld::clear() {
    baseManager.clear();
    kinematicsManager.clear();
    durabilityManager.clear();
    thermalManager.clear();
    fluidManager.clear();

    std::fill(pixelBuffer.begin(), pixelBuffer.end(), 0); // Black/Transparent screen
    if (rigidBodySystem) rigidBodySystem->clear();
}

void ParticleWorld::spawnParticle(MaterialID id, int x, int y) {
    if (!inBounds(x, y)) return;

    uint32_t index = computeIndex(x, y);

    // Remove existing if any
    removeParticle(index);

    // Look up the Logic Singleton
    Particle* pLogic = MaterialRegistry[static_cast<int>(id)];
    if (pLogic) {
        // Let the singleton populate the managers
        pLogic->onSpawn(index, x, y, *this);
        
        // Update pixel buffer immediately
        BaseComponent* base = baseManager.get(index);
        if (base) {
            updatePixelColor(x, y, base->color);
        }
    }
}

void ParticleWorld::removeParticle(int x, int y) {
    if (inBounds(x, y)) {
        removeParticle(computeIndex(x, y));
    }
}

void ParticleWorld::removeParticle(uint32_t index) {
    // Only remove if it exists (check base)
    if (baseManager.get(index) == nullptr) return;

    // Remove from all managers
    baseManager.remove(index);
    kinematicsManager.remove(index);
    durabilityManager.remove(index);
    thermalManager.remove(index);
    fluidManager.remove(index);

    // Clear pixel (Convert index back to X/Y for pixel buffer)
    int pIdx = index * 4;
    pixelBuffer[pIdx] = 0; pixelBuffer[pIdx+1] = 0; 
    pixelBuffer[pIdx+2] = 0; pixelBuffer[pIdx+3] = 0;
}

void ParticleWorld::moveParticle(int oldX, int oldY, int newX, int newY) {
    if (!inBounds(oldX, oldY) || !inBounds(newX, newY)) return;
    if (oldX == newX && oldY == newY) return;

    uint32_t oldIdx = computeIndex(oldX, oldY);
    uint32_t newIdx = computeIndex(newX, newY);

    if (baseManager.get(oldIdx) == nullptr) return; // Source empty

    // Move logic: Update indices in all managers
    baseManager.move(oldIdx, newIdx);
    kinematicsManager.move(oldIdx, newIdx);
    durabilityManager.move(oldIdx, newIdx);
    thermalManager.move(oldIdx, newIdx);
    fluidManager.move(oldIdx, newIdx);

    // Update Pixel Buffer
    int pOld = oldIdx * 4;
    int pNew = newIdx * 4;
    
    // Copy color to new spot
    std::memcpy(&pixelBuffer[pNew], &pixelBuffer[pOld], 4);
    // Clear old spot
    std::memset(&pixelBuffer[pOld], 0, 4);
}

void ParticleWorld::swapParticles(int x1, int y1, int x2, int y2) {
    if (!inBounds(x1, y1) || !inBounds(x2, y2)) return;

    // This is expensive in ECS, but necessary for fluids.
    // 1. Copy particle 1 to temp buffers
    // 2. Move particle 2 to 1
    // 3. Re-spawn temp at 2
    // Since we don't have deep copy helpers yet, a naive swap is messy.
    // Optimization: Use a temporary holding variables.
    
    uint32_t idx1 = computeIndex(x1, y1);
    uint32_t idx2 = computeIndex(x2, y2);

    BaseComponent* b1 = baseManager.get(idx1);
    BaseComponent* b2 = baseManager.get(idx2);

    // If both empty, do nothing
    if (!b1 && !b2) return;

    // If one empty, just move
    if (b1 && !b2) { moveParticle(x1, y1, x2, y2); return; }
    if (!b1 && b2) { moveParticle(x2, y2, x1, y1); return; }

    // If both exist, we need to swap data. 
    // In strict ECS, we might remove and respawn, or manually swap every component pointer.
    // For now, let's implement a simplified "Respawn Swap" to ensure data integrity
    
    // 1. Capture Data 1
    MaterialID id1 = b1->id;
    // (We lose custom velocity/heat here unless we write a full Copy helper. 
    // For a simple swap, preserving ID is often enough, but let's try to preserve Velocity)
    sf::Vector2f vel1 = {0,0};
    if (auto* k = kinematicsManager.get(idx1)) vel1 = k->velocity;
    
    // 2. Capture Data 2
    MaterialID id2 = b2->id;
    sf::Vector2f vel2 = {0,0};
    if (auto* k = kinematicsManager.get(idx2)) vel2 = k->velocity;

    // 3. Perform Swap by respawning (safest way without complex move logic)
    spawnParticle(id1, x2, y2);
    if(auto* k = kinematicsManager.get(idx2)) k->velocity = vel1;

    spawnParticle(id2, x1, y1);
    if(auto* k = kinematicsManager.get(idx1)) k->velocity = vel2;
}

void ParticleWorld::updatePixelColor(int x, int y, const sf::Color& color) {
    if (!inBounds(x, y)) return;
    int idx = computeIndex(x, y) * 4;
    pixelBuffer[idx]     = color.r;
    pixelBuffer[idx + 1] = color.g;
    pixelBuffer[idx + 2] = color.b;
    pixelBuffer[idx + 3] = color.a;
}

void ParticleWorld::update(float deltaTime) {
    frameCounter++;
    bool dir = (frameCounter % 2) == 0;

    if (rigidBodySystem) {
        rigidBodySystem->update(deltaTime);
        rigidBodySystem->renderToParticleWorld(this);
    }

    for (int y = height - 1; y >= 0; --y) {
        for (int x = dir ? 0 : width - 1; dir ? x < width : x >= 0; dir ? ++x : --x) {
            uint32_t idx = computeIndex(x, y);
            
            // 1. Get Base Component
            BaseComponent* base = baseManager.get(idx);
            
            // 2. Skip if empty or already updated
            if (!base || base->flags.hasBeenUpdatedThisFrame) continue;

            // 3. Mark updated
            base->flags.hasBeenUpdatedThisFrame = true;

            // 4. Update Logic via Singleton
            if (MaterialRegistry[static_cast<int>(base->id)]) {
                MaterialRegistry[static_cast<int>(base->id)]->update(x, y, idx, deltaTime, *this);
            }
        }
    }

    // Reset flags (Iterate dense vector for speed!)
    for (auto& base : baseManager.dense) {
        base.flags.hasBeenUpdatedThisFrame = false;
    }
}

void ParticleWorld::addParticleCircle(int centerX, int centerY, float radius, MaterialID materialType) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int x = centerX + dx, y = centerY + dy;
            if (inBounds(x, y) && isEmpty(x, y)) {
                if (std::sqrt(dx*dx + dy*dy) <= radius) {
                    spawnParticle(materialType, x, y);
                    // Add random velocity if needed via kinematicsManager.get(...)
                }
            }
        }
    }
}

void ParticleWorld::eraseCircle(int centerX, int centerY, float radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (std::sqrt(dx*dx + dy*dy) <= radius) {
                removeParticle(centerX + dx, centerY + dy);
            }
        }
    }
}

void ParticleWorld::triggerExplosion(int x, int y, int radius, int strength) {
    Explosion boom(*this, x, y, radius, strength);
    boom.enact();
}

bool ParticleWorld::saveWorld(const std::string& baseFilename) {
    std::string filename = getNextAvailableFilename("worlds/" + baseFilename);
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    // Write Magic Number & Header
    file.write(MAGIC_HEADER, 4);
    file.write(reinterpret_cast<const char*>(&width), sizeof(width));
    file.write(reinterpret_cast<const char*>(&height), sizeof(height));

    // Save Logic: We loop through the grid. 
    // If empty, write Empty ID. If exists, write ID + essential state.
    
    for (int i = 0; i < width * height; ++i) {
        BaseComponent* base = baseManager.get(i);
        MaterialID id = base ? base->id : static_cast<MaterialID>(0);
        
        file.write(reinterpret_cast<const char*>(&id), sizeof(id));
        
        if (base) {
            // Retrieve optional data
            KinematicsComponent* kin = kinematicsManager.get(i);
            sf::Vector2f vel = kin ? kin->velocity : sf::Vector2f(0,0);
            
            // Write core data needed to reconstruct
            file.write(reinterpret_cast<const char*>(&vel), sizeof(sf::Vector2f));
            file.write(reinterpret_cast<const char*>(&base->color), sizeof(sf::Color));
            
            // Flags
            uint8_t flags = 0;
            if(base->flags.isIgnited) flags |= 1;
            file.write(reinterpret_cast<const char*>(&flags), sizeof(uint8_t));
        } else {
            // Padding
            sf::Vector2f zero(0,0); sf::Color czero(0,0,0,0); uint8_t fzero = 0;
            file.write(reinterpret_cast<const char*>(&zero), sizeof(sf::Vector2f));
            file.write(reinterpret_cast<const char*>(&czero), sizeof(sf::Color));
            file.write(reinterpret_cast<const char*>(&fzero), sizeof(uint8_t));
        }
    }
    return true;
}

bool ParticleWorld::loadWorld(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    char header[4];
    file.read(header, 4);
    if (std::memcmp(header, MAGIC_HEADER, 4) != 0) return false;

    int fW, fH;
    file.read(reinterpret_cast<char*>(&fW), sizeof(fW));
    file.read(reinterpret_cast<char*>(&fH), sizeof(fH));
    if (fW != width || fH != height) return false;

    file.read(reinterpret_cast<char*>(&frameCounter), sizeof(frameCounter));

    clear(); // Reset managers

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            MaterialID id;
            file.read(reinterpret_cast<char*>(&id), sizeof(id));
            
            // Data buffers
            sf::Vector2f vel;
            sf::Color col;
            uint8_t flags;

            file.read(reinterpret_cast<char*>(&vel), sizeof(vel));
            file.read(reinterpret_cast<char*>(&col), sizeof(col));
            file.read(reinterpret_cast<char*>(&flags), sizeof(flags));

            if (static_cast<int>(id) != 0) {
                spawnParticle(id, x, y);
                uint32_t idx = computeIndex(x, y);
                
                // Restore state
                if (auto* base = baseManager.get(idx)) {
                    base->color = col;
                    base->flags.isIgnited = (flags & 1);
                }
                if (auto* kin = kinematicsManager.get(idx)) {
                    kin->velocity = vel;
                }
            }
        }
    }
    return true;
}

void ParticleWorld::updateParticleColor(uint32_t index, int x, int y) 
{
    BaseComponent* base = baseManager.get(index);
    if (!base) return;

    bool visualChanged = false;

    if (base->flags.isIgnited) 
    {
        if (Random::randInt(0, 100) < 20) 
        {
            int roll = Random::randInt(0, 100);
            int r, g, b;
            if (roll < 10) { r = 255; g = 255; b = 150; } 
            else if (roll < 60) { r = 255; g = Random::randInt(120, 180); b = 20; } 
            else { r = Random::randInt(180, 220); g = 40; b = 10; }

            base->color = sf::Color(r, g, b, 255);
            visualChanged = true;
        }
    }
    else if (base->flags.didColorChange) 
    {
        if(!base->flags.discolored){
            // Restore default (would require props lookup, leaving as current color for now)
        }
        base->flags.didColorChange = false;
        visualChanged = true;
    }

    if (visualChanged) 
    {
        updatePixelColor(x, y, base->color);
    }
}

void ParticleWorld::addRigidBody(int centerX, int centerY, float size, RigidBodyShape shape, MaterialID materialType)
{
    if (!rigidBodySystem) return;
    
    switch (shape)
    {
        case RigidBodyShape::Circle:
            rigidBodySystem->createCircle(static_cast<float>(centerX), static_cast<float>(centerY), size, materialType);
            break;
        case RigidBodyShape::Square:
            rigidBodySystem->createSquare(static_cast<float>(centerX), static_cast<float>(centerY), size, materialType);
            break;
        case RigidBodyShape::Triangle:
            rigidBodySystem->createTriangle(static_cast<float>(centerX), static_cast<float>(centerY), size, materialType);
            break;
    }
}

std::string ParticleWorld::getNextAvailableFilename(const std::string& baseName) 
{
    std::string filename;
    int counter = 0;
    do {
        filename = baseName + std::to_string(counter) + ".rrr";
        counter++;
    } while (std::filesystem::exists(filename));
    return filename;
}