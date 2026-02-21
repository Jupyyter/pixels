#include "RigidBody.hpp"
#include "ParticleWorld.hpp"
#include "Random.hpp"
#include <cmath>
RigidBodySystem::RigidBodySystem(int width, int height)
    : worldWidth(width), worldHeight(height)
{
    // Create Box2D world with gravity
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, 9.8f}; // Gravity pointing down
    worldId = b2CreateWorld(&worldDef);

    // Create world boundaries
    createWorldBoundaries();
}

RigidBodySystem::~RigidBodySystem()
{
    clear();
    // Destroy the world
    if (B2_IS_NON_NULL(worldId))
    {
        b2DestroyWorld(worldId);
    }
}

void RigidBodySystem::createWorldBoundaries()
{
    // Create static body definition for boundaries
    b2BodyDef groundBodyDef = b2DefaultBodyDef();
    groundBodyDef.position = {0.0f, 0.0f};
    groundBodyDef.type = b2_staticBody;

    // Create bottom boundary
    {
        b2BodyId groundBodyId = b2CreateBody(worldId, &groundBodyDef);
        boundaryBodies.push_back(groundBodyId);

        b2Segment segment = {{0.0f, worldHeight * PHYSICS_SCALE},
                             {worldWidth * PHYSICS_SCALE, worldHeight * PHYSICS_SCALE}};

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        b2CreateSegmentShape(groundBodyId, &shapeDef, &segment);
    }

    // Create left boundary
    {
        b2BodyId leftBodyId = b2CreateBody(worldId, &groundBodyDef);
        boundaryBodies.push_back(leftBodyId);

        b2Segment segment = {{0.0f, 0.0f}, {0.0f, worldHeight * PHYSICS_SCALE}};

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        b2CreateSegmentShape(leftBodyId, &shapeDef, &segment);
    }

    // Create right boundary
    {
        b2BodyId rightBodyId = b2CreateBody(worldId, &groundBodyDef);
        boundaryBodies.push_back(rightBodyId);

        b2Segment segment = {{worldWidth * PHYSICS_SCALE, 0.0f},
                             {worldWidth * PHYSICS_SCALE, worldHeight * PHYSICS_SCALE}};

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        b2CreateSegmentShape(rightBodyId, &shapeDef, &segment);
    }

    // Create top boundary (optional, for containment)
    {
        b2BodyId topBodyId = b2CreateBody(worldId, &groundBodyDef);
        boundaryBodies.push_back(topBodyId);

        b2Segment segment = {{0.0f, 0.0f}, {worldWidth * PHYSICS_SCALE, 0.0f}};

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        b2CreateSegmentShape(topBodyId, &shapeDef, &segment);
    }
}

RigidBodyData *RigidBodySystem::createCircle(float x, float y, float radius, MaterialID material)
{
    auto rbData = std::make_unique<RigidBodyData>();
    rbData->shape = RigidBodyShape::Circle;
    rbData->radius = radius;
    rbData->materialType = material;
    rbData->color = getMaterialColor(material);

    // Create Box2D body
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {x * PHYSICS_SCALE, y * PHYSICS_SCALE};
    rbData->bodyId = b2CreateBody(worldId, &bodyDef);

    // Create circle shape
    b2Circle circle = {{0.0f, 0.0f}, radius * PHYSICS_SCALE};

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = getMaterialDensity(material);
    shapeDef.material.friction = getMaterialFriction(material);
    shapeDef.material.restitution = getMaterialRestitution(material);

    b2CreateCircleShape(rbData->bodyId, &shapeDef, &circle);

    RigidBodyData *result = rbData.get();
    rigidBodies.push_back(std::move(rbData));
    return result;
}

RigidBodyData *RigidBodySystem::createSquare(float x, float y, float size, MaterialID material)
{
    auto rbData = std::make_unique<RigidBodyData>();
    rbData->shape = RigidBodyShape::Square;
    rbData->size = size;
    rbData->materialType = material;
    rbData->color = getMaterialColor(material);

    // Create Box2D body
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {x * PHYSICS_SCALE, y * PHYSICS_SCALE};
    rbData->bodyId = b2CreateBody(worldId, &bodyDef);

    // Create box shape
    float halfSize = size * 0.5f * PHYSICS_SCALE;
    b2Polygon box = b2MakeBox(halfSize, halfSize);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = getMaterialDensity(material);
    shapeDef.material.friction = getMaterialFriction(material);
    shapeDef.material.restitution = getMaterialRestitution(material);

    b2CreatePolygonShape(rbData->bodyId, &shapeDef, &box);

    // Setup vertices for rendering
    setupRigidBodyVertices(rbData.get());

    RigidBodyData *result = rbData.get();
    rigidBodies.push_back(std::move(rbData));
    return result;
}

RigidBodyData *RigidBodySystem::createTriangle(float x, float y, float size, MaterialID material)
{
    auto rbData = std::make_unique<RigidBodyData>();
    rbData->shape = RigidBodyShape::Triangle;
    rbData->size = size;
    rbData->materialType = material;
    rbData->color = getMaterialColor(material);

    // Create Box2D body
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {x * PHYSICS_SCALE, y * PHYSICS_SCALE};
    rbData->bodyId = b2CreateBody(worldId, &bodyDef);

    // Create triangle shape
    b2Vec2 vertices[3];
    float halfSize = size * 0.5f * PHYSICS_SCALE;

    // Equilateral triangle vertices
    vertices[0] = {0.0f, -halfSize};     // Top vertex
    vertices[1] = {-halfSize, halfSize}; // Bottom left
    vertices[2] = {halfSize, halfSize};  // Bottom right

    b2Hull hull = b2ComputeHull(vertices, 3);
    b2Polygon triangle = b2MakePolygon(&hull, 0.0f);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = getMaterialDensity(material);
    shapeDef.material.friction = getMaterialFriction(material);
    shapeDef.material.restitution = getMaterialRestitution(material);

    b2CreatePolygonShape(rbData->bodyId, &shapeDef, &triangle);

    // Setup vertices for rendering
    setupRigidBodyVertices(rbData.get());

    RigidBodyData *result = rbData.get();
    rigidBodies.push_back(std::move(rbData));
    return result;
}

void RigidBodySystem::setupRigidBodyVertices(RigidBodyData *rbData)
{
    rbData->vertices.clear();

    float halfSize = rbData->size * 0.5f;

    switch (rbData->shape)
    {
    case RigidBodyShape::Square:
        rbData->vertices.push_back(sf::Vector2f(-halfSize, -halfSize));
        rbData->vertices.push_back(sf::Vector2f(halfSize, -halfSize));
        rbData->vertices.push_back(sf::Vector2f(halfSize, halfSize));
        rbData->vertices.push_back(sf::Vector2f(-halfSize, halfSize));
        break;

    case RigidBodyShape::Triangle:
        rbData->vertices.push_back(sf::Vector2f(0.0f, -halfSize));     // Top
        rbData->vertices.push_back(sf::Vector2f(-halfSize, halfSize)); // Bottom left
        rbData->vertices.push_back(sf::Vector2f(halfSize, halfSize));  // Bottom right
        break;

    default:
        break; // Circle doesn't need vertices
    }
}

void RigidBodySystem::update(float deltaTime)
{
    // Step the physics simulation
    b2World_Step(worldId, deltaTime, 4); // Using 4 sub-steps as default

    removeInactiveBodies();
}

void RigidBodySystem::renderToParticleWorld(ParticleWorld *particleWorld)
{
    if (!particleWorld)
        return;

    for (const auto &rbData : rigidBodies)
    {
        if (!rbData->isActive || B2_IS_NULL(rbData->bodyId))
            continue;

        // Clear previous pixels first
        for (const auto &pixel : rbData->previousPixels)
        {
            if (particleWorld->inBounds(pixel.x, pixel.y))
            {
                const Particle*currentParticle = particleWorld->getParticleAt(pixel.x, pixel.y);
                // Only clear if it's still a rigid body particle (lifetime == -1.0f)
                if (currentParticle->id == rbData->materialType && currentParticle->lifeSpan == -1.0f)
                {
                    particleWorld->setParticleAt(pixel.x, pixel.y, nullptr);
                }
            }
        }
        rbData->previousPixels.clear();

        b2Vec2 position = b2Body_GetPosition(rbData->bodyId);
        b2Rot rotation = b2Body_GetRotation(rbData->bodyId);
        float angle = b2Rot_GetAngle(rotation);
        sf::Vector2f sfmlPosition = box2DToSFML(position);

        switch (rbData->shape)
        {
        case RigidBodyShape::Circle:
        {
            // Render circle using particle circle method
            int centerX = static_cast<int>(sfmlPosition.x);
            int centerY = static_cast<int>(sfmlPosition.y);

            // Draw filled circle and track pixels
            for (int dy = -static_cast<int>(rbData->radius); dy <= static_cast<int>(rbData->radius); ++dy)
            {
                for (int dx = -static_cast<int>(rbData->radius); dx <= static_cast<int>(rbData->radius); ++dx)
                {
                    int x = centerX + dx;
                    int y = centerY + dy;

                    float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    if (distance <= rbData->radius && particleWorld->inBounds(x, y))
                    {
                        // Only draw on empty spaces
                        if (particleWorld->isEmpty(x, y))
                        {
                            auto p = particleWorld->createParticleByType(rbData->materialType);
                            p->color = rbData->color;
                            p->lifeSpan = -1.0f; // Mark as rigid body particle
                            particleWorld->setParticleAt(x, y, std::move(p));
                            rbData->previousPixels.push_back(sf::Vector2i(x, y));
                        }
                    }
                }
            }
            break;
        }

        case RigidBodyShape::Square:
        case RigidBodyShape::Triangle:
        {
            // Transform vertices to world coordinates
            std::vector<sf::Vector2f> worldVertices;
            for (const auto &vertex : rbData->vertices)
            {
                // Rotate and translate vertex
                float cosA = std::cos(angle);
                float sinA = std::sin(angle);

                float rotatedX = vertex.x * cosA - vertex.y * sinA;
                float rotatedY = vertex.x * sinA + vertex.y * cosA;

                worldVertices.push_back(sf::Vector2f(
                    sfmlPosition.x + rotatedX,
                    sfmlPosition.y + rotatedY));
            }

            // Simple rasterization using bounding box and point-in-polygon test
            // Find bounding box
            float minX = worldVertices[0].x, maxX = worldVertices[0].x;
            float minY = worldVertices[0].y, maxY = worldVertices[0].y;

            for (const auto &v : worldVertices)
            {
                minX = std::min(minX, v.x);
                maxX = std::max(maxX, v.x);
                minY = std::min(minY, v.y);
                maxY = std::max(maxY, v.y);
            }

            // Rasterize within bounding box and track pixels
            for (int y = static_cast<int>(minY); y <= static_cast<int>(maxY); ++y)
            {
                for (int x = static_cast<int>(minX); x <= static_cast<int>(maxX); ++x)
                {
                    if (!particleWorld->inBounds(x, y))
                        continue;

                    // Point-in-polygon test using ray casting
                    bool inside = false;
                    for (size_t i = 0, j = worldVertices.size() - 1; i < worldVertices.size(); j = i++)
                    {
                        if (((worldVertices[i].y > y) != (worldVertices[j].y > y)) &&
                            (x < (worldVertices[j].x - worldVertices[i].x) * (y - worldVertices[i].y) /
                                         (worldVertices[j].y - worldVertices[i].y) +
                                     worldVertices[i].x))
                        {
                            inside = !inside;
                        }
                    }

                    if (inside && particleWorld->isEmpty(x, y))
                    {
                        auto p = particleWorld->createParticleByType(rbData->materialType);

                        if (p)
                        {
                            p->color = rbData->color;
                            p->lifeSpan = -1.0f;

                            particleWorld->setParticleAt(x, y, std::move(p));

                            rbData->previousPixels.push_back(sf::Vector2i(x, y));
                        }
                    }
                }
            }
            break;
        }
        }
    }
}

sf::Vector2f RigidBodySystem::box2DToSFML(const b2Vec2 &vec) const
{
    return sf::Vector2f(vec.x * INV_PHYSICS_SCALE, vec.y * INV_PHYSICS_SCALE);
}

b2Vec2 RigidBodySystem::sfmlToBox2D(const sf::Vector2f &vec) const
{
    return b2Vec2{vec.x * PHYSICS_SCALE, vec.y * PHYSICS_SCALE};
}

void RigidBodySystem::removeInactiveBodies()
{
    rigidBodies.erase(
        std::remove_if(rigidBodies.begin(), rigidBodies.end(),
                       [this](const std::unique_ptr<RigidBodyData> &rb)
                       {
                           if (!rb->isActive)
                           {
                               if (B2_IS_NON_NULL(rb->bodyId))
                               {
                                   b2DestroyBody(rb->bodyId);
                               }
                               return true;
                           }
                           return false;
                       }),
        rigidBodies.end());
}

void RigidBodySystem::clear()
{
    for (auto &rb : rigidBodies)
    {
        if (B2_IS_NON_NULL(rb->bodyId))
        {
            b2DestroyBody(rb->bodyId);
        }
    }
    rigidBodies.clear();

    // Clear boundary bodies
    for (auto &bodyId : boundaryBodies)
    {
        if (B2_IS_NON_NULL(bodyId))
        {
            b2DestroyBody(bodyId);
        }
    }
    boundaryBodies.clear();
    createWorldBoundaries();
}

sf::Color RigidBodySystem::getMaterialColor(MaterialID material) const
{
    switch (material)
    {
    case MaterialID::Stone:
        return getMaterialColor(MaterialID::Stone);
    case MaterialID::Wood:
        return getMaterialColor(MaterialID::Wood);
    case MaterialID::Sand:
        return getMaterialColor(MaterialID::Sand);
    default:
        return sf::Color::White;
    }
}

float RigidBodySystem::getMaterialDensity(MaterialID material) const
{
    switch (material)
    {
    case MaterialID::Stone:
        return 2.5f;
    case MaterialID::Wood:
        return 0.8f;
    case MaterialID::Sand:
        return 1.5f;
    default:
        return 1.0f;
    }
}

float RigidBodySystem::getMaterialFriction(MaterialID material) const
{
    switch (material)
    {
    case MaterialID::Stone:
        return 0.7f;
    case MaterialID::Wood:
        return 0.5f;
    case MaterialID::Sand:
        return 0.6f;
    default:
        return 0.3f;
    }
}

float RigidBodySystem::getMaterialRestitution(MaterialID material) const
{
    switch (material)
    {
    case MaterialID::Stone:
        return 0.3f;
    case MaterialID::Wood:
        return 0.1f;
    case MaterialID::Sand:
        return 0.05f;
    default:
        return 0.2f;
    }
}

void RigidBodySystem::applyParticleForces(ParticleWorld *particleWorld)
{

    if (!particleWorld)
        return;
}
