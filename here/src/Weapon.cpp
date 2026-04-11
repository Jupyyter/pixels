#include "Weapon.hpp"

// ==========================================
// WHEEL IMPLEMENTATION
// ==========================================
Wheel::Wheel(b2WorldId worldId, const sf::Image& img, int startX, int startY) 
    : RigidBody(worldId, img, startX, startY, MaterialID::Wood, false, false) 
{
    isIndestructible = true;
    isWeapon = false;
    
    // Convert box physics body to a proper circle for smooth rolling
    if (b2Body_IsValid(bodyId)) {
        int shapeCount = b2Body_GetShapeCount(bodyId);
        if (shapeCount > 0) {
            std::vector<b2ShapeId> shapes(shapeCount);
            b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
            for(int i = 0; i < shapeCount; ++i) {
                b2DestroyShape(shapes[i], true); 
            }
        }
        
        b2Circle circle = {};
        circle.center = {0.0f, 0.0f};
        circle.radius = std::max(width, height) / 2.0f * P2M;
        
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = 2.0f;     
        shapeDef.material.friction = 0.8f;    
        shapeDef.material.restitution = 0.3f;
        
        b2CreateCircleShape(bodyId, &shapeDef, &circle);
    }
}

void Wheel::update(float dt, ParticleWorld& world) {
    if (!b2Body_IsValid(bodyId)) return;

    b2Vec2 vel = b2Body_GetLinearVelocity(bodyId);
    
    // Ignore the very first frame to avoid self-detonation on spawn
    if (lastVel.x == 0.0f && lastVel.y == 0.0f) {
        lastVel = vel;
        return;
    }

    if (explosionTimer < 0.0f) {
        // Compute delta velocity (Impact)
        float dvx = lastVel.x - vel.x;
        float dvy = lastVel.y - vel.y;
        float dvSq = dvx * dvx + dvy * dvy;

        // Adjusted threshold: lower than 500 because the wheel is slower now.
        // 300 represents a sudden drop of ~17 m/s in velocity.
        if (dvSq > 300.0f) { 
            explosionTimer = 0.17f; // Start countdown
        }
        lastVel = vel;
    } else {
        explosionTimer -= dt;
        if (explosionTimer <= 0.0f) {
            b2Vec2 pos = b2Body_GetPosition(bodyId);
            
            // --- BIGGER EXPLOSION ---
            // Radius increased from 40 to 70, strength from 50 to 80
            world.triggerExplosion(pos.x * M2P, pos.y * M2P, 70, 80);
            
            isDestroyed = true;
        }
    }
}

// ==========================================
// WEAPON IMPLEMENTATION
// ==========================================
sf::Vector2f Weapon::getWeaponPivot(const std::string& name, int w, int h) {
    if (name == "sword") {
        return {7.0f, h / 2.0f - 0.5f}; 
    } else if (name == "spear") {
        return {15.0f, h / 2.0f - 0.5f}; 
    } else if (name.find("Revolver") != std::string::npos) {
        return {10.0f * 0.25f, 23.0f * 0.25f};
    } else if (name.find("Submachine") != std::string::npos) {
        return {22.0f * 0.25f, 17.0f * 0.25f};
    }
    return {w / 2.0f - 0.5f, h / 2.0f - 0.5f};
}
float Weapon::getWeaponVisualOffset(const std::string& name) {
    if (name == "sword" || name == "spear") {
        return -90.0f;
    }
    return 0.0f;
}

Weapon::Weapon(b2WorldId worldId, const sf::Image& img, int startX, int startY, const std::string& name)
    : RigidBody(worldId, img, startX, startY, MaterialID::Sand, true, false, getWeaponPivot(name, img.getSize().x, img.getSize().y), getWeaponVisualOffset(name)), weaponName(name)
{
    if (name.find("Revolver") != std::string::npos || name.find("Submachine") != std::string::npos) {
        isGun = true;
    }
}

Weapon::Weapon(b2WorldId worldId, int w, int h, const std::vector<LocalParticle>& parts, b2Vec2 pos, float angle, b2Vec2 linVel, float angVel, const std::string& name, bool glued, int sX, int sY)
    : RigidBody(worldId, w, h, parts, pos, angle, linVel, angVel, true, glued, sX, sY, getWeaponPivot(name, w, h), getWeaponVisualOffset(name)), weaponName(name)
{
    if (name.find("Revolver") != std::string::npos || name.find("Submachine") != std::string::npos) {
        isGun = true;
    }
}

void Weapon::performSwingEffect(sf::Vector2f playerPos, sf::Vector2f targetPos, ParticleWorld& world, RigidBodySystem& rbs) {
    sf::Vector2f dir = targetPos - playerPos;
    float len = std::hypot(dir.x, dir.y);
    if (len > 0.0f) { dir.x /= len; dir.y /= len; }
    
    sf::Vector2f spawnPos = {playerPos.x + dir.x * 20.0f, playerPos.y + dir.y * 20.0f};

    if (weaponName == "stick") {
        rbs.applyMeleeHit(playerPos, dir, 80.0f, 6000.0f, false, world);
    } 
    else if (weaponName == "spear") {
        rbs.applyMeleeHit(playerPos, dir, 120.0f, 2000.0f, true, world);
    } 
    else if (weaponName == "sword") {
        sf::Image wheelImg;
        if (wheelImg.loadFromFile("assets/images/rigidBodies/wheel.png")) {
            int wx = static_cast<int>(spawnPos.x) - wheelImg.getSize().x / 2;
            int wy = static_cast<int>(spawnPos.y) - wheelImg.getSize().y / 2;
            
            // --- SLOWER WHEEL ---
            // Reduced from 120.0f to 65.0f for better control/visibility
            b2Vec2 initVel = {dir.x * 65.0f, dir.y * 65.0f}; 
            
            auto wheel = std::make_unique<Wheel>(worldId, wheelImg, wx, wy);
            b2Body_SetLinearVelocity(wheel->bodyId, initVel);
            b2Body_SetAngularVelocity(wheel->bodyId, dir.x > 0 ? 30.0f : -30.0f);
            
            rbs.addBody(std::move(wheel));
        }
    }
}