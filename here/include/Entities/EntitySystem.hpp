#pragma once
#include <box2d/box2d.h>
#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include "EntityComponents.hpp"

class ParticleWorld;
class RigidBodySystem;
class Entity;

struct DebugLine {
    sf::Vector2f p1;
    sf::Vector2f p2;
    sf::Color color;
};

class EntitySystem {
private:
    b2WorldId physicsWorldId;
    std::shared_ptr<sf::Texture> defaultPlayerTexture;
    
    std::vector<DebugLine> debugLines;
    
    sf::FloatRect dirtyNavRect;
    bool hasDirtyNavRegion = false;

    std::vector<AINode> globalNavGraph;
    bool globalGraphBuilt = false;
    
    std::vector<EntityDefinition> entityDefs;
    
    void registerEntities();
    
public:
    std::vector<std::unique_ptr<Entity>> entities;

    EntitySystem(b2WorldId physWorld);
    ~EntitySystem();

    float groundCastY(float worldX, float castFromY, float maxDown, ParticleWorld& pw, bool ignorePlatforms = false, float bodyPosY = -1000.0f);
    bool isSolid(int cx, int cy, ParticleWorld& pw, bool ignorePlatforms = false);
    int getClosestNode(sf::Vector2f pos);
    std::vector<PathNodeData> findPath(sf::Vector2f start, sf::Vector2f target);
    sf::Vector2f resolveTargetPos(sf::Vector2f clickPos, ParticleWorld& pw);

    void addDebugLine(const sf::Vector2f& p1, const sf::Vector2f& p2, sf::Color color) {
        debugLines.push_back({p1, p2, color});
    }

    std::shared_ptr<sf::Texture> getDefaultTexture() { return defaultPlayerTexture; }

    void drawPixelatedLeg(sf::RenderTarget& target, const sf::Vector2f& hipWorld, const sf::Vector2f& footWorld, sf::Color color);
    void drawPixelatedHand(sf::RenderTarget& target, const sf::Vector2f& center, sf::Color color);

    void notifyTerrainChanged(sf::Vector2f center, float radius);

    void save(std::ostream& out) const;
    void load(std::istream& in);
    void clearAll();
    void eraseEntitiesInRadius(sf::Vector2f center, float radius);
    void eraseEntitiesInSquare(sf::Vector2f center, float radius);

    const std::vector<EntityDefinition>& getDefinitions() const { return entityDefs; }
    Entity* spawnEntity(float x, float y, const std::string& defName, bool isPlayer = true);

    void buildGlobalNavGraph(ParticleWorld& pw); 
    void triggerSwing(sf::Vector2f targetWorldPos);
    void issueMovementOrder(sf::Vector2f targetPos, ParticleWorld& pw);
    void updateInput(float dt, sf::Vector2f mouseWorldPos, RigidBodySystem& rbs, ParticleWorld& pw);
    void updateProceduralAnimations(float dt, ParticleWorld& particleWorld);
    void renderEntities(sf::RenderTarget& target);
    void renderDebug(sf::RenderTarget& target);

    void killAndRagdollEntity(Entity* e, ParticleWorld& particleWorld, uint8_t meatMaterial);
    
    sf::Vector2f getPlayerPos() const;
    
    const std::vector<AINode>& getGlobalNavGraph() const { return globalNavGraph; }
    bool isGlobalGraphBuilt() const { return globalGraphBuilt; }
};