// text/plain
// entity.hpp
#pragma once
#include <box2d/box2d.h>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include "EntityComponents.hpp"

class EntitySystem;
class ParticleWorld;
class RigidBodySystem;

class Entity {
protected:
    b2WorldId physicsWorldId;
    EntitySystem* system;

    b2BodyId createRagdollPart(float w, float h, sf::Vector2f worldPosPx, float angle, float density, bool isCircle = false);
    void createRevoluteJoint(b2BodyId bA, b2BodyId bB, b2Vec2 anchorWorldPx);
    void createDistanceJoint(b2BodyId bA, b2BodyId bB, b2Vec2 anchorWorldPx);
    void createMainCollider(float density, float friction);

public:
    b2BodyId bodyId = b2_nullBodyId;
    
    std::string defName;
    // Store the calculated half-dimensions for easy access in physics calculations.
    float colHalfW;
    float colHalfH;
    float colliderRadius;
    float hipBaseY;
    float armBaseY;

    float uprightMult = 1.0f;
    PlayerController pCtrl;
    SpriteSheet sprite;
    ProceduralHand handA;
    ProceduralHand handB;
    float weaponAngle = 90.0f;
    BodyBobState bob;
    
    float armSwing = 0.0f;
    float strideDistance = 12.0f; 

    Entity(b2WorldId physWorld, EntitySystem* sys, float x, float y, const EntityDefinition& def, bool isPlayer);
    virtual ~Entity();

    virtual void updateInput(float dt, sf::Vector2f mouseWorldPos, RigidBodySystem& rbs, ParticleWorld& pw, bool orderGiven);
    virtual void updateAnimations(float dt, ParticleWorld& pw);
    virtual void render(sf::RenderTarget& target) = 0;
    
    void updateArms(float dt, sf::Vector2f bodyPos, float bodyAng, bool isAirborne, b2Vec2 b2Vel, ParticleWorld& pw);
    void renderSprite(sf::RenderTarget& target, sf::Vector2f bodyPos, float bodyAng);
    void renderArmsAndWeapon(sf::RenderTarget& target, sf::Vector2f bodyPos, float bodyAng);

    void triggerSwing(sf::Vector2f targetWorldPos);

    virtual void toggleRagdoll(bool enable);
    virtual bool checkSideSnag(float dir, b2BodyId sideBody) { return false; }

    virtual void save(std::ostream& out) const;
    virtual void load(std::istream& in);
    
    virtual int getType() const = 0; 
};