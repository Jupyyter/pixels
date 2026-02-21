#include "ParticleWorld.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <fstream>
#include <filesystem>
#include "RigidBody.hpp"
#include <cstring>

ParticleWorld::~ParticleWorld() = default;

// A unique 4-byte header to identify your save files
const char MAGIC_HEADER[4] = {'S', 'A', 'N', 'D'};

ParticleWorld::ParticleWorld(unsigned int w, unsigned int h, const std::string& worldFile)
    : width(w), height(h), frameCounter(0) {
    particles.resize(width * height);
    pixelBuffer.resize(width * height * 4);
    rigidBodySystem = std::make_unique<RigidBodySystem>(width, height);

    if (!worldFile.empty() && std::filesystem::exists(worldFile)) {
        loadWorld(worldFile); // loadWorld calls clear() internally if it fails
    } else {
        clear();
    }
}

void ParticleWorld::clear() {
    for (auto& p : particles) {
        p.reset(); // Clear all unique_ptrs (set to nullptr)
    }
    std::fill(pixelBuffer.begin(), pixelBuffer.end(), 0); // Black/Transparent screen
    if (rigidBodySystem) rigidBodySystem->clear();
}

void ParticleWorld::setParticleAt(int x, int y, std::unique_ptr<Particle> particle) {
    if (!inBounds(x, y)) return;

    int idx = computeIndex(x, y);
    int pIdx = idx * 4;

    if (particle) {
        pixelBuffer[pIdx]     = particle->color.r;
        pixelBuffer[pIdx + 1] = particle->color.g;
        pixelBuffer[pIdx + 2] = particle->color.b;
        pixelBuffer[pIdx + 3] = particle->color.a;
        
        particle->position = sf::Vector2i(x, y);
        particles[idx] = std::move(particle);
    } else {
        // Clearing a particle spot
        pixelBuffer[pIdx] = 0; pixelBuffer[pIdx+1] = 0; 
        pixelBuffer[pIdx+2] = 0; pixelBuffer[pIdx+3] = 0;
        particles[idx].reset();
    }
}


void ParticleWorld::moveParticle(int oldX, int oldY, int newX, int newY) {
    if (!inBounds(oldX, oldY) || !inBounds(newX, newY)) return;
    if (oldX == newX && oldY == newY) return;

    int oldIdx = computeIndex(oldX, oldY);
    int newIdx = computeIndex(newX, newY);

    if (!particles[oldIdx]) return; // Source is empty

    // Move the unique_ptr ownership
    particles[newIdx] = std::move(particles[oldIdx]);
    particles[newIdx]->position = sf::Vector2i(newX, newY);

    // Update the visual pixel buffer
    int pOld = oldIdx * 4;
    int pNew = newIdx * 4;
    for (int i = 0; i < 4; ++i) {
        pixelBuffer[pNew + i] = pixelBuffer[pOld + i];
        pixelBuffer[pOld + i] = 0; 
    }
}

void ParticleWorld::swapParticles(int x1, int y1, int x2, int y2) {
    if (!inBounds(x1, y1) || !inBounds(x2, y2)) return;

    int idx1 = computeIndex(x1, y1);
    int idx2 = computeIndex(x2, y2);

    // 1. Swap the unique_ptrs
    std::swap(particles[idx1], particles[idx2]);

    // 2. Update positions for whichever particles exist after swap
    if (particles[idx1]) particles[idx1]->position = sf::Vector2i(x1, y1);
    if (particles[idx2]) particles[idx2]->position = sf::Vector2i(x2, y2);

    // 3. Swap the pixel colors
    int pIdx1 = idx1 * 4;
    int pIdx2 = idx2 * 4;
    for (int i = 0; i < 4; i++) {
        std::swap(pixelBuffer[pIdx1 + i], pixelBuffer[pIdx2 + i]);
    }
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
            Particle* p = getParticleAt(x, y);
            if (!p || p->hasBeenUpdatedThisFrame || p->lifeSpan == -1.0f) continue;
            
            p->lifeSpan += deltaTime;
            p->update(x, y, deltaTime, *this);
        }
    }

    for (auto& p : particles) {
        if (p) p->hasBeenUpdatedThisFrame = false;
    }
}

void ParticleWorld::addParticleCircle(int centerX, int centerY, float radius, MaterialID materialType) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int x = centerX + dx, y = centerY + dy;
            if (inBounds(x, y) && isEmpty(x, y)) {
                if (std::sqrt(dx*dx + dy*dy) <= radius) {
                    auto p = createParticleByType(materialType);
                    if (p) {
                        p->velocity = { Random::randFloat(-0.5f, 0.5f), Random::randFloat(-0.5f, 0.5f) };
                        setParticleAt(x, y, std::move(p));
                    }
                }
            }
        }
    }
}

void ParticleWorld::eraseCircle(int centerX, int centerY, float radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (std::sqrt(dx*dx + dy*dy) <= radius) {
                setParticleAt(centerX + dx, centerY + dy, nullptr);
            }
        }
    }
}

bool ParticleWorld::saveWorld(const std::string& baseFilename) {
    std::string filename = getNextAvailableFilename("worlds/" + baseFilename);
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    // Write Magic Number
    file.write(MAGIC_HEADER, 4);
    file.write(reinterpret_cast<const char*>(&width), sizeof(width));
    file.write(reinterpret_cast<const char*>(&height), sizeof(height));

    for (int i = 0; i < width * height; ++i) {
        Particle* p = particles[i].get();
        // Use static_cast<MaterialID>(0) to represent empty instead of named constant
        MaterialID id = p ? p->id : static_cast<MaterialID>(0);
        file.write(reinterpret_cast<const char*>(&id), sizeof(id));
        if (p) {
            file.write(reinterpret_cast<const char*>(&p->velocity), sizeof(sf::Vector2f));
            file.write(reinterpret_cast<const char*>(&p->lifeSpan), sizeof(p->lifeSpan));
            file.write(reinterpret_cast<const char*>(&p->color), sizeof(sf::Color));
        } else {
            // Padding to maintain binary structure
            sf::Vector2f zero(0,0); float fzero = 0; sf::Color czero(0,0,0,0);
            file.write(reinterpret_cast<const char*>(&zero), sizeof(sf::Vector2f));
            file.write(reinterpret_cast<const char*>(&fzero), sizeof(float));
            file.write(reinterpret_cast<const char*>(&czero), sizeof(sf::Color));
        }
    }
    return true;
}

bool ParticleWorld::loadWorld(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    // 1. Check Magic Header
    char header[4];
    file.read(header, 4);
    if (std::memcmp(header, MAGIC_HEADER, 4) != 0) {
        std::cerr << "Error: File is not a valid SandWorld save (Magic Header mismatch).\n";
        return false; 
    }

    // 2. Check Dimensions
    int fW, fH;
    file.read(reinterpret_cast<char*>(&fW), sizeof(fW));
    file.read(reinterpret_cast<char*>(&fH), sizeof(fH));
    if (file.fail() || fW != width || fH != height) {
        std::cerr << "Error: Dimension mismatch or corrupted header.\n";
        return false;
    }

    file.read(reinterpret_cast<char*>(&frameCounter), sizeof(frameCounter));

    // 3. Robust Particle Loading
    // Size of data per particle: ID (int) + Velocity (2f) + Lifespan (f) + Color (4b)
    const size_t dataSizePerParticle = sizeof(sf::Vector2f) + sizeof(float) + sizeof(sf::Color);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            MaterialID id;
            file.read(reinterpret_cast<char*>(&id), sizeof(id));

            if (file.eof() || file.fail()) break;

            // Check if ID is 0 (representing empty)
            if (static_cast<int>(id) == 0) {
                // If it's an empty particle in the file, we skip the data bytes
                file.seekg(dataSizePerParticle, std::ios::cur);
                setParticleAt(x, y, nullptr);
            } else {
                auto p = createParticleByType(id);
                if (p) {
                    file.read(reinterpret_cast<char*>(&p->velocity), sizeof(sf::Vector2f));
                    file.read(reinterpret_cast<char*>(&p->lifeSpan), sizeof(p->lifeSpan));
                    file.read(reinterpret_cast<char*>(&p->color), sizeof(sf::Color));
                    setParticleAt(x, y, std::move(p));
                } else {
                    // Unknown ID (corrupted data or valid ID but creation failed)
                    // Skip the bytes for this "ghost" particle
                    file.seekg(dataSizePerParticle, std::ios::cur);
                    setParticleAt(x, y, nullptr);
                }
            }
        }
    }
    return true;
}

void ParticleWorld::updateParticleColor(Particle* particle, ParticleWorld& world) 
{
    if (!particle) return;

    bool visualChanged = false;

    if (particle->isIgnited) 
    {
        // 1. Slow down the flicker frequency
        // Only change the color if a random check passes (e.g., 20% chance per frame)
        // This makes the flicker "stick" long enough for the eye to register it.
        if (Random::randInt(0, 100) < 20) 
        {
            // 2. Use a "Heat Level" system for high contrast
            // We'll pick between three distinct states: Red-Hot, Orange-Fire, and Yellow-Flash
            int roll = Random::randInt(0, 100);
            
            int r, g, b;
            if (roll < 10) { 
                // Rare "White/Yellow Hot" flash (Brightest)
                r = 255; g = 255; b = 150; 
            } else if (roll < 60) {
                // Standard "Orange" flame
                r = 255; g = Random::randInt(120, 180); b = 20;
            } else {
                // Deep "Red" embers (Darkest)
                r = Random::randInt(180, 220); g = 40; b = 10;
            }

            particle->color = sf::Color(r, g, b, 255);
            visualChanged = true;
        }
    }
    else if (particle->didColorChange) 
    {
        if(!particle->discolored){
            particle->color = particle->defaultColor;
        }
        particle->didColorChange = false;
        visualChanged = true;
    }

    if (visualChanged) 
    {
        world.updatePixelColor(particle->position.x, particle->position.y, particle->color);
    }
}

void ParticleWorld::addRigidBody(int centerX, int centerY, float size, RigidBodyShape shape, MaterialID materialType)
{
    if (!rigidBodySystem) return;
    
    // Create rigid body based on shape
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

std::unique_ptr<Particle> ParticleWorld::createParticleByType(MaterialID type) {
    // Check removed here; loop implies failure if type is not found (e.g. type 0)
    for (const auto& props : ALL_MATERIALS) {
        if (props.id == type) return props.create();
    }
    return nullptr;
}