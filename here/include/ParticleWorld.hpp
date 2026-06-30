#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <string>
#include <iostream>
#include <type_traits>
#include "Constants.hpp"
#include "Random.hpp"
#include "Particles/Particle.hpp" 

class Particle; 
class RigidBodySystem;
enum class RigidBodyShape;
class EntitySystem; // Forward declare for binding hook correctly!

const uint32_t INVALID_INDEX = 0xFFFFFFFF;

// --- COMPONENT MASKS ---
constexpr uint8_t COMP_BASE       = 1 << 0; 
constexpr uint8_t COMP_KINEMATICS = 1 << 1; 
constexpr uint8_t COMP_DURABILITY = 1 << 2; 
constexpr uint8_t COMP_THERMAL    = 1 << 3; 
constexpr uint8_t COMP_FLUID      = 1 << 4; 
constexpr uint8_t COMP_PLATFORM   = 1 << 5;

struct Chunk {
    BaseComponent base[CHUNK_AREA];

    std::unique_ptr<KinematicsComponent[]> kinematics;
    std::unique_ptr<DurabilityComponent[]> durability;
    std::unique_ptr<ThermalComponent[]> thermal;
    std::unique_ptr<FluidComponent[]> fluid;

    bool isSleeping = false;
    bool isActive = true; 
    mutable bool visualDirty = true;
    bool needsSave = false;

    int activeMinX = 0, activeMinY = 0;
    int activeMaxX = CHUNK_SIZE - 1, activeMaxY = CHUNK_SIZE - 1;
    int nextMinX = 0, nextMinY = 0;
    int nextMaxX = CHUNK_SIZE - 1, nextMaxY = CHUNK_SIZE - 1;

    std::vector<std::uint8_t> pixelData;

    Chunk() : pixelData(CHUNK_AREA * 4, 0) {}

    void clear() {
        for(int i = 0; i < CHUNK_AREA; ++i) base[i] = BaseComponent();
        
        kinematics.reset();
        durability.reset();
        thermal.reset();
        fluid.reset();
        
        std::fill(pixelData.begin(), pixelData.end(), 0);
        visualDirty = true;
    }
};

struct ParticleContext {
    Chunk* chunk;
    uint32_t index;
    int x, y; 
    
    BaseComponent* base;
    KinematicsComponent* kinematics;
    FluidComponent* fluid;
    ThermalComponent* thermal;
    DurabilityComponent* durability;
};

struct ExplosionEvent {
    int x, y, radius, strength;
};

class ParticleWorld {
private:
    sf::FloatRect simulationBounds;
    sf::FloatRect renderBounds;
    std::vector<std::uint8_t> pixelBuffer; 
    std::vector<ExplosionEvent> pendingExplosions;
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;

    mutable Chunk* cacheChunk[64] = {nullptr};
    mutable int cacheCx[64];
    mutable int cacheCy[64];

    sf::Vector2i cameraPos;
    int viewWidth, viewHeight;
    uint32_t frameCounter;

    std::unique_ptr<RigidBodySystem> rigidBodySystem;
    EntitySystem* entitySystem = nullptr; 

public:
    ParticleWorld(unsigned int w, unsigned int h);
    ~ParticleWorld();
    
    // Extensibility hook providing sequence independent binding!
    void setEntitySystem(EntitySystem* sys) { entitySystem = sys; }
    EntitySystem* getEntitySystem() const { return entitySystem; } 

    RigidBodySystem* getRigidBodySystem() const { return rigidBodySystem.get(); }
    void renderDebugColliders(sf::RenderTarget& target) const;
    void addRigidBodyFromSprite(const sf::Image& img, int startX, int startY, MaterialID mat, bool glue = false);
    void addRigidBody(int cx, int cy, float sz, RigidBodyShape sh, MaterialID mat, bool glue = false);
    
    void addStructureFromSprite(const sf::Image& img, int startX, int startY, MaterialID mat);

    void addWeapon(const sf::Image& img, int startX, int startY, const std::string& name);
    void renderWeaponsOutline(sf::RenderTarget& target, sf::Vector2f playerPos) const;

    void updateChunkPixel(Chunk* c, uint32_t localIdx, sf::Color color);
    void removeParticleInternal(Chunk* chunk, uint32_t localIndex);
    Chunk* getOrCreateChunk(int x, int y);
    Chunk* getChunk(int x, int y) const;
    std::unordered_map<uint32_t, MaterialID> containerPayloads;
    void notifyTerrainChanged(float x, float y, float radius);
    void generateWorldFromImage(const std::string& imagePath, const std::string& mapPath);

    inline uint32_t computeLocalIndex(int x, int y) const {
        return ((y & 63) << 6) | (x & 63); 
    }
    
    inline bool isEmpty(int x, int y) { 
        Chunk* c = getChunk(x, y);
        return !c || c->base[computeLocalIndex(x, y)].compMask == 0;
    }

    inline bool isEmptyFast(const ParticleContext& ctx, int targetX, int targetY) {
        if ((((ctx.x ^ targetX) | (ctx.y ^ targetY)) & ~63) == 0) {
            uint32_t idx = ((targetY & 63) << 6) | (targetX & 63);
            return ctx.base[idx].compMask == 0;
        }
        return isEmpty(targetX, targetY);
    }

    template <typename T>
    T* getFast(const ParticleContext& ctx, int targetX, int targetY) {
        if ((((ctx.x ^ targetX) | (ctx.y ^ targetY)) & ~63) == 0) {
            uint32_t idx = ((targetY & 63) << 6) | (targetX & 63);
            
            if constexpr (std::is_same_v<T, BaseComponent>) {
                if (ctx.base[idx].compMask == 0) return nullptr;
                return &ctx.base[idx];
            } 
            else if constexpr (std::is_same_v<T, KinematicsComponent>) {
                if (!(ctx.base[idx].compMask & COMP_KINEMATICS)) return nullptr;
                return &ctx.kinematics[idx];
            }
            else if constexpr (std::is_same_v<T, DurabilityComponent>) {
                if (!(ctx.base[idx].compMask & COMP_DURABILITY)) return nullptr;
                return &ctx.durability[idx];
            }
            else if constexpr (std::is_same_v<T, ThermalComponent>) {
                if (!(ctx.base[idx].compMask & COMP_THERMAL)) return nullptr;
                return &ctx.thermal[idx];
            }
            else if constexpr (std::is_same_v<T, FluidComponent>) {
                if (!(ctx.base[idx].compMask & COMP_FLUID)) return nullptr;
                return &ctx.fluid[idx];
            }
        }
        return get<T>(targetX, targetY);
    }

    bool inBounds(int x, int y);

    template <typename T>
    T* getByIndex(uint32_t localIndex, int x, int y) {
        return getByLocalIndex<T>(getChunk(x, y), localIndex);
    }

    template <typename T>
    T* get(int x, int y) {
        Chunk* c = getChunk(x, y);
        if (!c) return nullptr;
        return getByLocalIndex<T>(c, computeLocalIndex(x, y));
    }

    template <typename T> 
    T* getByLocalIndex(Chunk* c, uint32_t idx) {
        if (!c) return nullptr;

        if constexpr (std::is_same_v<T, BaseComponent>) {
            if (c->base[idx].compMask == 0) return nullptr;
            return &c->base[idx];
        } 
        else if constexpr (std::is_same_v<T, KinematicsComponent>) {
            if (!(c->base[idx].compMask & COMP_KINEMATICS)) return nullptr;
            return &c->kinematics[idx];
        }
        else if constexpr (std::is_same_v<T, DurabilityComponent>) {
            if (!(c->base[idx].compMask & COMP_DURABILITY)) return nullptr;
            return &c->durability[idx];
        }
        else if constexpr (std::is_same_v<T, ThermalComponent>) {
            if (!(c->base[idx].compMask & COMP_THERMAL)) return nullptr;
            return &c->thermal[idx];
        }
        else if constexpr (std::is_same_v<T, FluidComponent>) {
            if (!(c->base[idx].compMask & COMP_FLUID)) return nullptr;
            return &c->fluid[idx];
        }
        return nullptr;
    }

    template <typename T> 
    void add(int x, int y, const T& comp) {
        Chunk* c = getOrCreateChunk(x, y);
        uint32_t idx = computeLocalIndex(x, y);

        if constexpr (std::is_same_v<T, BaseComponent>) {
            c->base[idx] = comp;
            c->base[idx].compMask |= COMP_BASE; 
        } 
        else if constexpr (std::is_same_v<T, KinematicsComponent>) {
            if (!c->kinematics) c->kinematics = std::make_unique<KinematicsComponent[]>(CHUNK_AREA);
            c->kinematics[idx] = comp;
            c->base[idx].compMask |= COMP_KINEMATICS;
        }
        else if constexpr (std::is_same_v<T, DurabilityComponent>) {
            if (!c->durability) c->durability = std::make_unique<DurabilityComponent[]>(CHUNK_AREA);
            c->durability[idx] = comp;
            c->base[idx].compMask |= COMP_DURABILITY;
        }
        else if constexpr (std::is_same_v<T, ThermalComponent>) {
            if (!c->thermal) c->thermal = std::make_unique<ThermalComponent[]>(CHUNK_AREA);
            c->thermal[idx] = comp;
            c->base[idx].compMask |= COMP_THERMAL;
        }
        else if constexpr (std::is_same_v<T, FluidComponent>) {
            if (!c->fluid) c->fluid = std::make_unique<FluidComponent[]>(CHUNK_AREA);
            c->fluid[idx] = comp;
            c->base[idx].compMask |= COMP_FLUID;
        }
    }

    void update(float deltaTime);
    void spawnParticle(MaterialID id, int x, int y);
    void removeParticle(int x, int y);
    
    void moveParticle(int oldX, int oldY, int newX, int newY);
    void swapParticles(int x1, int y1, int x2, int y2);
    void wakeParticle(int globalX, int globalY);

    void updateParticleColor(uint32_t localIndex, int x, int y, Chunk* c = nullptr);
    void setParticleColor(int x, int y, const sf::Color& newColor);

    void updateCameraBounds(float centerX, float centerY, float viewWidth, float viewHeight);
    void setCameraPos(int x, int y);
    void renderToBuffer();
    
    void clear();
    void triggerExplosion(int x, int y, int radius, int strength);
    void addParticleCircle(int centerX, int centerY, float radius, MaterialID materialType);
    void addParticleSquare(int centerX, int centerY, float radius, MaterialID materialType);
    void eraseCircle(int centerX, int centerY, float radius);
    void eraseSquare(int centerX, int centerY, float radius);
    void updatePixelColor(int x, int y, const sf::Color& color); 

    bool saveWorld(const std::string& baseFilename = "world");
    bool loadWorld(const std::string& filename);
    std::string getNextAvailableFilename(const std::string& baseName);

    const std::uint8_t* getPixelBuffer() const { return pixelBuffer.data(); }
    int getWidth() const { return viewWidth; }
    int getHeight() const { return viewHeight; }
    inline uint32_t computeIndex(int x, int y) const { return computeLocalIndex(x, y); }
    const std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash>& getActiveChunks() const { return chunks; }
    sf::Vector2i getCameraPos() const { return cameraPos; }
    const sf::FloatRect& getRenderBounds() const { return renderBounds; }
};