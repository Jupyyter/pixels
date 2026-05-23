#pragma once
#include <entt/entt.hpp>
#include <box2d/box2d.h>
#include <SFML/Graphics.hpp>
#include "EntityComponents.hpp"
#include "ParticleWorld.hpp"
#include "RigidBody.hpp"

struct DebugLine {
    sf::Vector2f p1;
    sf::Vector2f p2;
    sf::Color color;
};

class EntitySystem {
private:
    entt::registry registry;
    b2WorldId physicsWorldId;
    std::shared_ptr<sf::Texture> defaultPlayerTexture;
    
    std::vector<DebugLine> debugLines;

    float groundCastY(float worldX, float castFromY, float maxDown, ParticleWorld& pw);

    void drawPixelatedLeg(sf::RenderTarget& target, const sf::Vector2f& hipWorld, const sf::Vector2f& footWorld, sf::Color color);
    void drawPixelatedHand(sf::RenderTarget& target, const sf::Vector2f& center, sf::Color color);

    // AI Global Navigation
    std::vector<AINode> globalNavGraph;
    bool globalGraphBuilt = false;
    
    bool isSolid(int cx, int cy, ParticleWorld& pw);
    int getClosestNode(sf::Vector2f pos);
    std::vector<PathNodeData> findPath(sf::Vector2f start, sf::Vector2f target);
sf::Vector2f resolveTargetPos(sf::Vector2f clickPos, ParticleWorld& pw);
public:
    EntitySystem(b2WorldId physWorld);
    ~EntitySystem();

    entt::entity spawnEntity(float x, float y, const std::string& texturePath = "", bool isPlayer = true);

    void buildGlobalNavGraph(ParticleWorld& pw); // Exposed in case you want to trigger it from UI
    void triggerSwing(sf::Vector2f targetWorldPos);
    void updateInput(float dt, sf::Vector2f mouseWorldPos, RigidBodySystem& rbs, ParticleWorld& pw);
    void updateProceduralAnimations(float dt, ParticleWorld& particleWorld);
    void renderEntities(sf::RenderTarget& target);
    void renderDebug(sf::RenderTarget& target);

    void killAndRagdollEntity(entt::entity e, ParticleWorld& particleWorld, MaterialID meatMaterial);
    
    sf::Vector2f getPlayerPos() const;
};