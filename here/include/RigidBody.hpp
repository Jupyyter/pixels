#pragma once

#include <box2d/box2d.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <SFML/Graphics.hpp>
#include "ParticleWorld.hpp"
#include "Particles/Particle.hpp"

constexpr float P2M = 1.0f / 10.0f; 
constexpr float M2P = 10.0f;        

enum class RigidBodyShape {
    Box,
    Circle,
    FromSprite
};

struct LocalParticle {
    BaseComponent base;
    DurabilityComponent dur;
    ThermalComponent therm;
    bool active;
    int localX;
    int localY;
    int lastWorldX;
    int lastWorldY;
};

struct DrawnPixel {
    int wx, wy;
    int localIdx;
};

class RigidBody {
public:
    b2BodyId bodyId;
    b2WorldId worldId;
    std::vector<DrawnPixel> drawnPixels;
    std::vector<LocalParticle> particles;
    int width, height;
    bool needsFixtureRebuild;

    bool hasCustomRendering = false;
    bool isWeapon = false;
    bool isGun = false;
    bool isEquipped = false;
    bool isIndestructible = false;
    
    // Gives custom entities (like Wheels) a safe way to completely delete themselves
    bool isDestroyed = false; 
    
    sf::Vector2f pivot;
    float visualAngleOffset = 0.0f;
    
    bool isGlued = false;
    int startX = 0;
    int startY = 0;

    RigidBody(b2WorldId worldId, const sf::Image& img, int startX, int startY, MaterialID mat, bool weapon, bool glued, sf::Vector2f customPivot = {-1, -1}, float angleOffset = 0.0f);
    RigidBody(b2WorldId worldId, int w, int h, const std::vector<LocalParticle>& parts, b2Vec2 pos, float angle, b2Vec2 linVel, float angVel, bool weapon, bool glued = false, int sX = 0, int sY = 0, sf::Vector2f customPivot = {-1, -1}, float angleOffset = 0.0f);
    
    virtual ~RigidBody() = default;

    virtual void update(float dt, ParticleWorld& world) {} 
    
    void rebuildFixtures();
    std::vector<std::vector<LocalParticle>> findIslands();

    void clearFromWorld(ParticleWorld& world);
    
    virtual void renderPixelated(sf::RenderTarget& target, sf::Vector2f pos, float angleDeg, bool flipX, sf::Color overrideColor = sf::Color::Transparent, bool applyVisualOffset = true, float scale = 1.0f);
    
    virtual void renderEffects(sf::RenderTarget& target) {}
};

struct ChunkTerrain {
    b2BodyId bodyId;
    uint64_t hash;
};

class RigidBodySystem {
private:
    std::vector<DrawnPixel> orphanedPixels; 
    b2WorldId worldId;
    std::vector<std::unique_ptr<RigidBody>> bodies;
    std::unordered_map<ChunkCoord, ChunkTerrain, ChunkCoordHash> chunkBodies;

public:
    void save(std::ostream& out) const;
    void load(std::istream& in);
    void clearAll();
    
    void eraseInRadius(sf::Vector2f center, float radius);
    void eraseInSquare(sf::Vector2f center, float radius);
    
    RigidBodySystem();
    ~RigidBodySystem();

    void renderDebug(sf::RenderTarget& target) const;
    void addRigidBodyFromSprite(const sf::Image& img, int x, int y, MaterialID mat, bool glue, ParticleWorld& world);
    void addBody(std::unique_ptr<RigidBody> rb);
    
    void addWeapon(const sf::Image& img, int x, int y, const std::string& name);
    RigidBody* getNearestWeapon(sf::Vector2f pos, float radius);
    void renderWeaponsOutline(sf::RenderTarget& target, sf::Vector2f playerPos);
    void renderGluedOutlines(sf::RenderTarget& target, ParticleWorld& world) const;
    
    void renderEffects(sf::RenderTarget& target) const;

    void applyMeleeHit(sf::Vector2f pos, sf::Vector2f dir, float range, float force, bool shatter, ParticleWorld& world);
    void applyBlastImpulse(float x, float y, float radius, float strength);

    void clearFromWorld(ParticleWorld& world);
    void stepPhysics(float dt, ParticleWorld& world);
    void rasterizeToWorld(ParticleWorld& world);
    void syncFromWorld(ParticleWorld& world);
    b2WorldId getWorldId() const { return worldId; }
    void rebuildChunkTerrain(ChunkCoord coord, Chunk* chunk, ParticleWorld& world);
};