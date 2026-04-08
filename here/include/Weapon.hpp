#pragma once
#include "RigidBody.hpp"
#include <string>

// The Explosive Wheel spawned by the sword
class Wheel : public RigidBody {
public:
    float explosionTimer = -1.0f;
    b2Vec2 lastVel = {0.0f, 0.0f};

    // FIX: Removed the unused initialVelocity parameter to match the .cpp file
    Wheel(b2WorldId worldId, const sf::Image& img, int startX, int startY);
    void update(float dt, ParticleWorld& world) override;
};

// Base class for Equippable Weapons
class Weapon : public RigidBody {
public:
    std::string weaponName;

    Weapon(b2WorldId worldId, const sf::Image& img, int startX, int startY, const std::string& name);
    Weapon(b2WorldId worldId, int w, int h, const std::vector<LocalParticle>& parts, b2Vec2 pos, float angle, b2Vec2 linVel, float angVel, const std::string& name, bool glued, int sX, int sY);

    static sf::Vector2f getWeaponPivot(const std::string& name, int width, int height);
    static float getWeaponVisualOffset(const std::string& name);

    void performSwingEffect(sf::Vector2f playerPos, sf::Vector2f targetPos, ParticleWorld& world, RigidBodySystem& rbs);
};