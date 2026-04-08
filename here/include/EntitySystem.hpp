#pragma once
#include <entt/entt.hpp>
#include <box2d/box2d.h>
#include <SFML/Graphics.hpp>
#include "EntityComponents.hpp"
#include "ParticleWorld.hpp"
#include "RigidBody.hpp"

class EntitySystem {
private:
    entt::registry registry;
    b2WorldId physicsWorldId;
    std::shared_ptr<sf::Texture> defaultPlayerTexture;

    float groundCastY(float worldX, float castFromY, float maxDown, ParticleWorld& pw);

    void drawPixelatedLeg(sf::RenderTarget& target, const sf::Vector2f& hipWorld, const sf::Vector2f& footWorld, sf::Color color);
    void drawPixelatedHand(sf::RenderTarget& target, const sf::Vector2f& center, sf::Color color);

public:
    EntitySystem(b2WorldId physWorld);
    ~EntitySystem();

    entt::entity spawnPlayer(float x, float y, const std::string& texturePath = "");

    void triggerSwing(sf::Vector2f targetWorldPos);
    void updateInput(float dt, RigidBodySystem& rbs, ParticleWorld& pw);
    void updateProceduralAnimations(float dt, ParticleWorld& particleWorld);
    void renderEntities(sf::RenderTarget& target);

    void killAndRagdollEntity(entt::entity e, ParticleWorld& particleWorld, MaterialID meatMaterial);
    
    sf::Vector2f getPlayerPos() const;
};