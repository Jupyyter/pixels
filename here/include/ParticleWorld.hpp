#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <SFML/Graphics.hpp>
#include "Particle.hpp"
#include "Constants.hpp"
#include "Random.hpp"

namespace SandSim {
    class ParticleWorld {
    private:
        std::vector<std::unique_ptr<Particle>> particles;
        std::vector<std::uint8_t> pixelBuffer;
        int width, height;
        uint32_t frameCounter;
        bool currentStep = false; // For frame stepping like Java's stepped BitSet
        sf::Vector3f gravity{0.0f, GRAVITY, 0.0f}; // Match Java gravity
        
    public:
        ParticleWorld(int w, int h) : width(w), height(h), frameCounter(0) {
            particles.resize(width * height);
            pixelBuffer.resize(width * height * 4); // RGBA
            clear();
        }
        
        void clear() {
            for (int i = 0; i < width * height; i++) {
                particles[i] = std::make_unique<EmptyParticle>(i % width, i / width);
            }
            std::fill(pixelBuffer.begin(), pixelBuffer.end(), 0);
        }
        
        int computeIndex(int x, int y) const {
            return y * width + x;
        }
        
        bool inBounds(int x, int y) const {
            return x >= 0 && x < width && y >= 0 && y < height;
        }
        
        bool isWithinBounds(int x, int y) const {
            return inBounds(x, y);
        }
        
        bool isEmpty(int x, int y) const {
            return inBounds(x, y) && particles[computeIndex(x, y)]->id == MaterialID::Empty;
        }
        
        Particle* get(int x, int y) {
            if (!inBounds(x, y)) return nullptr;
            return particles[computeIndex(x, y)].get();
        }
        
        Particle* get(float x, float y) {
            return get(static_cast<int>(x), static_cast<int>(y));
        }
        
        const Particle* get(int x, int y) const {
            if (!inBounds(x, y)) return nullptr;
            return particles[computeIndex(x, y)].get();
        }
        
        void setElementAtIndex(int x, int y, std::unique_ptr<Particle> particle) {
            if (!inBounds(x, y)) return;
            
            particle->matrixX = x;
            particle->matrixY = y;
            particle->pixelX = x;
            particle->pixelY = y;
            
            int idx = computeIndex(x, y);
            particles[idx] = std::move(particle);
            
            // Update pixel buffer
            int pixelIdx = idx * 4;
            pixelBuffer[pixelIdx] = particles[idx]->color.r;
            pixelBuffer[pixelIdx + 1] = particles[idx]->color.g;
            pixelBuffer[pixelIdx + 2] = particles[idx]->color.b;
            pixelBuffer[pixelIdx + 3] = particles[idx]->color.a;
        }
        
        void setElementAtIndex(int x, int y, MaterialID type) {
            setElementAtIndex(x, y, Particle::createByType(type, x, y));
        }
        
        void spawnElementByMatrix(int x, int y, MaterialID type) {
            setElementAtIndex(x, y, type);
        }
        
        void swapParticles(int x1, int y1, int x2, int y2) {
            if (!inBounds(x1, y1) || !inBounds(x2, y2)) return;
            
            int idx1 = computeIndex(x1, y1);
            int idx2 = computeIndex(x2, y2);
            
            // Update matrix coordinates
            particles[idx1]->matrixX = x2;
            particles[idx1]->matrixY = y2;
            particles[idx1]->pixelX = x2;
            particles[idx1]->pixelY = y2;
            
            particles[idx2]->matrixX = x1;
            particles[idx2]->matrixY = y1;
            particles[idx2]->pixelX = x1;
            particles[idx2]->pixelY = y1;
            
            // Swap particles
            std::swap(particles[idx1], particles[idx2]);
            
            // Update pixel buffer for both positions
            updatePixelBuffer(x1, y1);
            updatePixelBuffer(x2, y2);
        }
        
        void updatePixelBuffer(int x, int y) {
            if (!inBounds(x, y)) return;
            int idx = computeIndex(x, y);
            int pixelIdx = idx * 4;
            pixelBuffer[pixelIdx] = particles[idx]->color.r;
            pixelBuffer[pixelIdx + 1] = particles[idx]->color.g;
            pixelBuffer[pixelIdx + 2] = particles[idx]->color.b;
            pixelBuffer[pixelIdx + 3] = particles[idx]->color.a;
        }
        
        // In particleworld.hpp, inside the ParticleWorld class

void update(float deltaTime) {
    frameCounter++;
    for (auto& p : particles) {
        if (p) p->stepped = false;
    }
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            auto* particle = get(x, y);
            if (particle && particle->id != MaterialID::Empty && !particle->stepped) {
                particle->step(this);
            }
        }
    }
}
        
        const std::uint8_t* getPixelBuffer() const {
            return pixelBuffer.data();
        }
        
        int getWidth() const { return width; }
        int getHeight() const { return height; }
        
        sf::Vector3f getGravity() const { return gravity; }
        bool getCurrentStep() const { return currentStep; }
        uint32_t getFrameCount() const { return frameCounter; }
        
        void addParticleCircle(int centerX, int centerY, float radius, MaterialID materialType) {
            // Iterate over a square bounding box around the circle
            for (int dy = -static_cast<int>(radius); dy <= static_cast<int>(radius); ++dy) {
                for (int dx = -static_cast<int>(radius); dx <= static_cast<int>(radius); ++dx) {
                    int x = centerX + dx;
                    int y = centerY + dy;

                    // Check if the current (x, y) coordinate is within the circle's radius
                    float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    if (distance <= radius && isEmpty(x, y)) {
                        setElementAtIndex(x, y, materialType);
                    }
                }
            }
        }
        
        void eraseCircle(int centerX, int centerY, float radius) {
            for (int dy = -static_cast<int>(radius); dy <= static_cast<int>(radius); ++dy) {
                for (int dx = -static_cast<int>(radius); dx <= static_cast<int>(radius); ++dx) {
                    int x = centerX + dx;
                    int y = centerY + dy;
                    
                    if (inBounds(x, y)) {
                        float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                        if (distance <= radius) {
                            setElementAtIndex(x, y, MaterialID::Empty);
                        }
                    }
                }
            }
        }
        
        // Chunk system placeholders (not implemented but matches Java interface)
        bool useChunks = false;
        bool shouldElementInChunkStep(Particle* element) { return true; }
        void reportToChunkActive(Particle* element) {}
        void reportToChunkActive(int x, int y) {}
        
        // Explosion system placeholder
        void addExplosion(int radius, int strength, Particle* source) {
            // Simple explosion implementation
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int explosionX = source->matrixX + dx;
                    int explosionY = source->matrixY + dy;
                    if (inBounds(explosionX, explosionY)) {
                        float distance = std::sqrt(dx * dx + dy * dy);
                        if (distance <= radius) {
                            auto* target = get(explosionX, explosionY);
                            if (target && target->id != MaterialID::Empty) {
                                target->explode(this, strength);
                            }
                        }
                    }
                }
            }
        }
        
        // Frame checking methods to match Java
        bool isReactionFrame() const { return (frameCounter % 3) == 0; }
        bool isEffectsFrame() const { return (frameCounter % 1) == 0; }
        
        // Compatibility methods for old code
        Particle& getParticleAt(int x, int y) {
            return *get(x, y);
        }
        
        void setParticleAt(int x, int y, const Particle& particle) {
            // This is a compatibility shim - create new particle from the template
            setElementAtIndex(x, y, particle.id);
        }
        
        bool isInLiquid(int x, int y, int* lx, int* ly) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (inBounds(nx, ny)) {
                        auto* neighbor = get(nx, ny);
                        if (neighbor && (neighbor->id == MaterialID::Water || neighbor->id == MaterialID::Oil || neighbor->id == MaterialID::Acid)) {
                            if (lx) *lx = nx;
                            if (ly) *ly = ny;
                            return true;
                        }
                    }
                }
            }
            return false;
        }
        
        bool isInWater(int x, int y, int* lx, int* ly) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (inBounds(nx, ny)) {
                        auto* neighbor = get(nx, ny);
                        if (neighbor && neighbor->id == MaterialID::Water) {
                            if (lx) *lx = nx;
                            if (ly) *ly = ny;
                            return true;
                        }
                    }
                }
            }
            return false;
        }
    };
    
} // namespace SandSim