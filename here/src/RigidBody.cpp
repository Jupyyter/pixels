#include "RigidBody.hpp"
#include "ParticleWorld.hpp"
#include "Particles/Particle.hpp"
#include "Random.hpp"
#include <cmath>

RigidBodySystem::RigidBodySystem(int width, int height)
    : worldWidth(width), worldHeight(height)
{
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, 9.8f}; 
    worldId = b2CreateWorld(&worldDef);
    createWorldBoundaries();
}

RigidBodySystem::~RigidBodySystem() {
    clear();
    if (b2World_IsValid(worldId)) {
        b2DestroyWorld(worldId);
    }
}

void RigidBodySystem::createWorldBoundaries() {
    b2BodyDef groundBodyDef = b2DefaultBodyDef();
    groundBodyDef.type = b2_staticBody;

    b2BodyId groundId = b2CreateBody(worldId, &groundBodyDef);
    boundaryBodies.push_back(groundId);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    
    // Bottom
    b2Segment groundSegment = {{0.0f, worldHeight * PHYSICS_SCALE}, {worldWidth * PHYSICS_SCALE, worldHeight * PHYSICS_SCALE}};
    b2CreateSegmentShape(groundId, &shapeDef, &groundSegment);
    
    // Walls
    b2Segment leftWall = {{0.0f, 0.0f}, {0.0f, worldHeight * PHYSICS_SCALE}};
    b2CreateSegmentShape(groundId, &shapeDef, &leftWall);
    
    b2Segment rightWall = {{worldWidth * PHYSICS_SCALE, 0.0f}, {worldWidth * PHYSICS_SCALE, worldHeight * PHYSICS_SCALE}};
    b2CreateSegmentShape(groundId, &shapeDef, &rightWall);
}

RigidBodyData* RigidBodySystem::createCircle(float x, float y, float radius, MaterialID material) {
    auto rbData = std::make_unique<RigidBodyData>();
    rbData->shape = RigidBodyShape::Circle;
    rbData->radius = radius;
    rbData->materialType = material;
    rbData->color = Particle::getRandomColor(material);

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {x * PHYSICS_SCALE, y * PHYSICS_SCALE};
    rbData->bodyId = b2CreateBody(worldId, &bodyDef);

    b2Circle circle = {{0.0f, 0.0f}, radius * PHYSICS_SCALE};
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = getMaterialDensity(material);
    shapeDef.material.friction = getMaterialFriction(material);
    shapeDef.material.restitution = getMaterialRestitution(material);

    b2CreateCircleShape(rbData->bodyId, &shapeDef, &circle);

    RigidBodyData* result = rbData.get();
    rigidBodies.push_back(std::move(rbData));
    return result;
}

RigidBodyData* RigidBodySystem::createSquare(float x, float y, float size, MaterialID material) {
    auto rbData = std::make_unique<RigidBodyData>();
    rbData->shape = RigidBodyShape::Square;
    rbData->size = size;
    rbData->materialType = material;
    rbData->color = Particle::getRandomColor(material);

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {x * PHYSICS_SCALE, y * PHYSICS_SCALE};
    rbData->bodyId = b2CreateBody(worldId, &bodyDef);

    float h = size * 0.5f * PHYSICS_SCALE;
    b2Polygon box = b2MakeBox(h, h);
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = getMaterialDensity(material);
    b2CreatePolygonShape(rbData->bodyId, &shapeDef, &box);

    setupRigidBodyVertices(rbData.get());
    RigidBodyData* result = rbData.get();
    rigidBodies.push_back(std::move(rbData));
    return result;
}

RigidBodyData* RigidBodySystem::createTriangle(float x, float y, float size, MaterialID material) {
    auto rbData = std::make_unique<RigidBodyData>();
    rbData->shape = RigidBodyShape::Triangle;
    rbData->size = size;
    rbData->materialType = material;
    rbData->color = Particle::getRandomColor(material);

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {x * PHYSICS_SCALE, y * PHYSICS_SCALE};
    rbData->bodyId = b2CreateBody(worldId, &bodyDef);

    float h = size * 0.5f * PHYSICS_SCALE;
    b2Vec2 verts[3] = {{0.0f, -h}, {-h, h}, {h, h}};
    b2Hull hull = b2ComputeHull(verts, 3);
    b2Polygon tri = b2MakePolygon(&hull, 0.0f);
    
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = getMaterialDensity(material);
    b2CreatePolygonShape(rbData->bodyId, &shapeDef, &tri);

    setupRigidBodyVertices(rbData.get());
    RigidBodyData* result = rbData.get();
    rigidBodies.push_back(std::move(rbData));
    return result;
}

void RigidBodySystem::setupRigidBodyVertices(RigidBodyData* rbData) {
    rbData->vertices.clear();
    float h = rbData->size * 0.5f;
    if (rbData->shape == RigidBodyShape::Square) {
        rbData->vertices = {{-h, -h}, {h, -h}, {h, h}, {-h, h}};
    } else if (rbData->shape == RigidBodyShape::Triangle) {
        rbData->vertices = {{0.0f, -h}, {-h, h}, {h, h}};
    }
}

void RigidBodySystem::update(float deltaTime) {
    b2World_Step(worldId, deltaTime, 4);
    removeInactiveBodies();
}

void RigidBodySystem::renderToParticleWorld(ParticleWorld* world) {
    if (!world) return;

    for (auto& rb : rigidBodies) {
        if (!rb->isActive || B2_IS_NULL(rb->bodyId)) continue;

        // 1. Clear old pixels
        for (const auto& pix : rb->previousPixels) {
            if (world->inBounds(pix.x, pix.y)) {
                BaseComponent* base = world->baseManager.get(world->getIndex(pix.x, pix.y));
                // Only clear if it's a rigid body pixel
                if (base && base->flags.isRigidBodyPart) {
                    world->removeParticle(pix.x, pix.y);
                }
            }
        }
        rb->previousPixels.clear();

        // 2. Draw new pixels
        b2Vec2 pos = b2Body_GetPosition(rb->bodyId);
        float angle = b2Rot_GetAngle(b2Body_GetRotation(rb->bodyId));
        sf::Vector2f center = box2DToSFML(pos);

        // This is a simplified rasterizer. 
        // For performance, you'd calculate the bounding box first.
        float range = (rb->shape == RigidBodyShape::Circle) ? rb->radius : rb->size;
        int minX = (int)(center.x - range - 1);
        int maxX = (int)(center.x + range + 1);
        int minY = (int)(center.y - range - 1);
        int maxY = (int)(center.y + range + 1);

        for (int py = minY; py <= maxY; py++) {
            for (int px = minX; px <= maxX; px++) {
                if (!world->inBounds(px, py)) continue;

                bool inside = false;
                if (rb->shape == RigidBodyShape::Circle) {
                    float dx = px - center.x;
                    float dy = py - center.y;
                    inside = (dx*dx + dy*dy) <= (rb->radius * rb->radius);
                } else {
                    // Polygon test
                    sf::Vector2f p((float)px, (float)py);
                    // (Rotate point into local space for easier check)
                    float s = std::sin(-angle);
                    float c = std::cos(-angle);
                    sf::Vector2f lp = { (p.x-center.x)*c - (p.y-center.y)*s, (p.x-center.x)*s + (p.y-center.y)*c };
                    
                    if (rb->shape == RigidBodyShape::Square) {
                        float h = rb->size * 0.5f;
                        inside = (lp.x >= -h && lp.x <= h && lp.y >= -h && lp.y <= h);
                    } else {
                        // Triangle check
                        float h = rb->size * 0.5f;
                        // Local triangle points: (0, -h), (-h, h), (h, h)
                        auto sign = [](sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3) {
                            return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
                        };
                        bool b1 = sign(lp, {0, -h}, {-h, h}) < 0.0f;
                        bool b2 = sign(lp, {-h, h}, {h, h}) < 0.0f;
                        bool b3 = sign(lp, {h, h}, {0, -h}) < 0.0f;
                        inside = ((b1 == b2) && (b2 == b3));
                    }
                }

                if (inside && world->isEmpty(px, py)) {
                    world->spawnParticle(rb->materialType, px, py);
                    uint32_t idx = world->getIndex(px, py);
                    if (auto* base = world->baseManager.get(idx)) {
                        base->color = rb->color;
                        base->flags.isRigidBodyPart = true; // Mark as owned by physics
                    }
                    rb->previousPixels.push_back({px, py});
                }
            }
        }
    }
}

sf::Vector2f RigidBodySystem::box2DToSFML(const b2Vec2& vec) const {
    return {vec.x * INV_PHYSICS_SCALE, vec.y * INV_PHYSICS_SCALE};
}

b2Vec2 RigidBodySystem::sfmlToBox2D(const sf::Vector2f& vec) const {
    return {vec.x * PHYSICS_SCALE, vec.y * PHYSICS_SCALE};
}

void RigidBodySystem::removeInactiveBodies() {
    auto it = std::remove_if(rigidBodies.begin(), rigidBodies.end(), [this](auto& rb) {
        if (!rb->isActive) {
            b2DestroyBody(rb->bodyId);
            return true;
        }
        return false;
    });
    rigidBodies.erase(it, rigidBodies.end());
}

void RigidBodySystem::clear() {
    for (auto& rb : rigidBodies) b2DestroyBody(rb->bodyId);
    rigidBodies.clear();
    for (auto& b : boundaryBodies) b2DestroyBody(b);
    boundaryBodies.clear();
    createWorldBoundaries();
}

float RigidBodySystem::getMaterialDensity(MaterialID material) const {
    switch (material) {
        case MaterialID::Stone: return 2.5f;
        case MaterialID::Wood:  return 0.8f;
        default:               return 1.0f;
    }
}

float RigidBodySystem::getMaterialFriction(MaterialID material) const {
    return 0.5f;
}

float RigidBodySystem::getMaterialRestitution(MaterialID material) const {
    return 0.2f;
}