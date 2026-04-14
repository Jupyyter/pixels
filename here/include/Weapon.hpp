#pragma once
#include "RigidBody.hpp"
#include <string>
#include <deque>

// NEW: Stores the state of the bullet at a specific point in time for the trail
struct TrailNode {
    sf::Vector2f pos;
    float angleDeg;
    float age;
};

// The Explosive Wheel spawned by the sword
class Wheel : public RigidBody {
public:
    float explosionTimer = -1.0f;
    b2Vec2 lastVel = {0.0f, 0.0f};

    Wheel(b2WorldId worldId, const sf::Image& img, int startX, int startY);
    void update(float dt, ParticleWorld& world) override;
};
constexpr float BULLET_TRAIL_DURATION = 0.05f;
// The Projectile spawned by guns
class Bullet : public RigidBody {
public:
    b2Vec2 lastVel = {0.0f, 0.0f};
    b2Vec2 lastPos = {0.0f, 0.0f};
    float lifeTime = 2.0f; 
    
    // UPDATED: Now uses TrailNode to track age and angle alongside position
    std::deque<TrailNode> pathHistory;

    Bullet(b2WorldId worldId, const sf::Image& img, int startX, int startY, float angleDeg, float speed);
    void update(float dt, ParticleWorld& world) override;
    void renderEffects(sf::RenderTarget& target) override;
};

// Base class for Equippable Weapons
class Weapon : public RigidBody {
public:
    std::string weaponName;
    
    // Gun Specific Properties
    bool semiAuto = false;
    float fireRate = 0.1f;
    sf::Vector2f bulletOffset = {0.0f, 0.0f};
    sf::Vector2f flashOffset = {0.0f, 0.0f};
    
    float recoilForce = 2.0f;
    float visualRecoilAngle = 30.0f;
    float flashTimer = 0.0f;
    int flashFrame = 0;

    Weapon(b2WorldId worldId, const sf::Image& img, int startX, int startY, const std::string& name);
    Weapon(b2WorldId worldId, int w, int h, const std::vector<LocalParticle>& parts, b2Vec2 pos, float angle, b2Vec2 linVel, float angVel, const std::string& name, bool glued, int sX, int sY);

    static sf::Vector2f getWeaponPivot(const std::string& name, int width, int height);
    static float getWeaponVisualOffset(const std::string& name);

    void setupGunProperties();
    void fire(sf::Vector2f handPos, sf::Vector2f targetPos, float weaponAngle, bool flipX, RigidBodySystem& rbs, ParticleWorld& world);
    void performSwingEffect(sf::Vector2f playerPos, sf::Vector2f targetPos, ParticleWorld& world, RigidBodySystem& rbs);
    
    void update(float dt, ParticleWorld& world) override;
    
    // UPDATED: Added scale parameter
    void renderPixelated(sf::RenderTarget& target, sf::Vector2f pos, float angleDeg, bool flipX, sf::Color overrideColor = sf::Color::Transparent, bool applyVisualOffset = true, float scale = 1.0f) override;
};