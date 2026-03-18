#pragma once

#include <box2d/box2d.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <SFML/Graphics.hpp>
#include "ParticleWorld.hpp"
#include "Particles/Particle.hpp"

// Box2D Scale Factors (10 pixels = 1 meter)
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

    RigidBody(b2WorldId worldId, const sf::Image& img, int startX, int startY, MaterialID mat);
    RigidBody(b2WorldId worldId, int w, int h, const std::vector<LocalParticle>& parts, b2Vec2 pos, float angle, b2Vec2 linVel, float angVel);
    void rebuildFixtures();
    std::vector<std::vector<LocalParticle>> findIslands();
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
    RigidBodySystem();
    ~RigidBodySystem();

    void renderDebug(sf::RenderTarget& target) const;
    void addRigidBodyFromSprite(const sf::Image& img, int x, int y, MaterialID mat);
    
    // Core Pipeline
    void clearFromWorld(ParticleWorld& world);
    void stepPhysics(float dt, ParticleWorld& world);
    void rasterizeToWorld(ParticleWorld& world);
    void syncFromWorld(ParticleWorld& world);
    
    void rebuildChunkTerrain(ChunkCoord coord, Chunk* chunk);
};