#pragma once
#include <box2d/box2d.h>
#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include "Constants.hpp"
#include <algorithm>

class ParticleWorld;

enum class RigidBodyShape {
    Circle,
    Square,
    Triangle
};

struct RigidBodyData {
    b2BodyId bodyId;
    RigidBodyShape shape;
    MaterialID materialType;
    float radius; 
    float size;   
    sf::Color color;
    std::vector<sf::Vector2f> vertices; 
    std::vector<sf::Vector2i> previousPixels; 
    bool isActive;
    
    RigidBodyData() : bodyId(b2_nullBodyId), shape(RigidBodyShape::Circle), 
                     materialType(MaterialID::Stone), radius(10.0f), 
                     size(20.0f), color(sf::Color::White), isActive(true) {}
};

class RigidBodySystem {
private:
    b2WorldId worldId;
    std::vector<std::unique_ptr<RigidBodyData>> rigidBodies;
    std::vector<b2BodyId> boundaryBodies; 
    
    static constexpr float PHYSICS_SCALE = 0.01f; 
    static constexpr float INV_PHYSICS_SCALE = 100.0f; 
    
    int worldWidth, worldHeight;
    
public:
    RigidBodySystem(int width, int height);
    ~RigidBodySystem();
    
    RigidBodyData* createCircle(float x, float y, float radius, MaterialID material);
    RigidBodyData* createSquare(float x, float y, float size, MaterialID material);
    RigidBodyData* createTriangle(float x, float y, float size, MaterialID material);
    
    void update(float deltaTime);
    void renderToParticleWorld(ParticleWorld* particleWorld);
    
    sf::Vector2f box2DToSFML(const b2Vec2& vec) const;
    b2Vec2 sfmlToBox2D(const sf::Vector2f& vec) const;
    
    void removeInactiveBodies();
    void clear();
    
private:
    void createWorldBoundaries();
    void setupRigidBodyVertices(RigidBodyData* rbData);
    float getMaterialDensity(MaterialID material) const;
    float getMaterialFriction(MaterialID material) const;
    float getMaterialRestitution(MaterialID material) const;
};