#include "Weapon.hpp"
#ifndef PI
constexpr float PI = 3.14159265358979323846f;
#endif
// ==========================================
// WHEEL IMPLEMENTATION
// ==========================================
Wheel::Wheel(b2WorldId worldId, const sf::Image& img, int startX, int startY) 
    : RigidBody(worldId, img, startX, startY, MaterialID::Wood, false, false) 
{
    isIndestructible = true;
    isWeapon = false;
    
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
    
    if (lastVel.x == 0.0f && lastVel.y == 0.0f) {
        lastVel = vel;
        return;
    }

    if (explosionTimer < 0.0f) {
        float dvx = lastVel.x - vel.x;
        float dvy = lastVel.y - vel.y;
        float dvSq = dvx * dvx + dvy * dvy;

        if (dvSq > 300.0f) { 
            explosionTimer = 0.17f; 
        }
        lastVel = vel;
    } else {
        explosionTimer -= dt;
        if (explosionTimer <= 0.0f) {
            b2Vec2 pos = b2Body_GetPosition(bodyId);
            world.triggerExplosion(pos.x * M2P, pos.y * M2P, 70, 80);
            isDestroyed = true;
        }
    }
}

// ==========================================
// BULLET IMPLEMENTATION
// ==========================================
Bullet::Bullet(b2WorldId worldId, const sf::Image& img, int startX, int startY, float angleDeg, float speed)
    : RigidBody(worldId, img, startX, startY, MaterialID::Stone, false, false) 
{
    isWeapon = false;
    isGun = false;
    isIndestructible = true; // Prevent crumbling on impact
    
    b2Body_SetTransform(bodyId, {startX * P2M, startY * P2M}, b2MakeRot(angleDeg * PI / 180.0f));
    float vx = std::cos(angleDeg * PI / 180.0f) * speed;
    float vy = std::sin(angleDeg * PI / 180.0f) * speed;
    
    b2Body_SetLinearVelocity(bodyId, {vx, vy});
    b2Body_SetGravityScale(bodyId, 0.0f); // Make it fire straight like a laser
}

void Bullet::update(float dt, ParticleWorld& world) {
    if (!b2Body_IsValid(bodyId)) return;

    bool shouldExplode = false;
    
    // Condition 1: Large velocity change (collision with physics solids)
    b2Vec2 vel = b2Body_GetLinearVelocity(bodyId);
    if (lastVel.x != 0.0f || lastVel.y != 0.0f) {
        float dvx = lastVel.x - vel.x;
        float dvy = lastVel.y - vel.y;
        float dvSq = dvx * dvx + dvy * dvy;
        if (dvSq > 1000.0f) {
            shouldExplode = true;
        }
    }
    lastVel = vel;

    // Condition 2: Lifetime expiry
    lifeTime -= dt;
    if (lifeTime <= 0.0f) {
        shouldExplode = true;
    }

    // Condition 3: Overlap with destructible particles (if not already exploding)
    if (!shouldExplode) {
        b2Transform transform = b2Body_GetTransform(bodyId);
        float cs = transform.q.c;
        float sn = transform.q.s;

        // Iterate through the bullet's own local particles to find their world positions
        for (const auto& p : particles) {
            if (p.active) {
                // Transform local particle coords to world space
                // local coords are relative to top-left, so we must offset by pivot for rotation
                float rx = (p.localX - pivot.x) * P2M;
                float ry = (p.localY - pivot.y) * P2M;
                
                float world_x_m = transform.p.x + (rx * cs - ry * sn);
                float world_y_m = transform.p.y + (rx * sn + ry * cs);

                int world_x_p = static_cast<int>(world_x_m * M2P);
                int world_y_p = static_cast<int>(world_y_m * M2P);

                BaseComponent* base = world.get<BaseComponent>(world_x_p, world_y_p);
                // Check if the cell contains a particle that is NOT part of another rigid body
                if (base && base->compMask != 0 && !base->flags.isRigidBodyPart) {
                    Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                    if (logic) {
                        MaterialGroup group = logic->getGroup();
                        if (group == MaterialGroup::MovableSolid || group == MaterialGroup::Liquid) {
                            shouldExplode = true;
                            break; // Found a particle, no need to check others
                        }
                    }
                }
            }
        }
    }

    if (shouldExplode) {
        b2Vec2 pos = b2Body_GetPosition(bodyId);
        int radius = static_cast<int>(std::max(width, height) * 1.5f);
        if (radius < 4) radius = 4;
        
        world.triggerExplosion(pos.x * M2P, pos.y * M2P, radius, 30);
        isDestroyed = true;
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

void Weapon::setupGunProperties() {
    if (weaponName.find("Revolver") != std::string::npos) {
        isGun = true;
        semiAuto = true;
        fireRate = 0.0f; 
        barrelOffset = {12.5f, -3.75f};
    } else if (weaponName.find("Submachine") != std::string::npos) {
        isGun = true;
        semiAuto = false;
        fireRate = 0.1f; // Fast automatic rate
        barrelOffset = {13.5f, -1.25f};
    }
}

Weapon::Weapon(b2WorldId worldId, const sf::Image& img, int startX, int startY, const std::string& name)
    : RigidBody(worldId, img, startX, startY, MaterialID::Sand, true, false, getWeaponPivot(name, img.getSize().x, img.getSize().y), getWeaponVisualOffset(name)), weaponName(name)
{
    setupGunProperties();
}

Weapon::Weapon(b2WorldId worldId, int w, int h, const std::vector<LocalParticle>& parts, b2Vec2 pos, float angle, b2Vec2 linVel, float angVel, const std::string& name, bool glued, int sX, int sY)
    : RigidBody(worldId, w, h, parts, pos, angle, linVel, angVel, true, glued, sX, sY, getWeaponPivot(name, w, h), getWeaponVisualOffset(name)), weaponName(name)
{
    setupGunProperties();
}

void Weapon::fire(sf::Vector2f handPos, sf::Vector2f targetPos, float weaponAngle, bool flipX, RigidBodySystem& rbs, ParticleWorld& world) {
    static sf::Image bulletImg;
    static bool bulletLoaded = false;
    
    // Load the bullet image only once
    if (!bulletLoaded) {
        if (!bulletImg.loadFromFile("assets/images/weapons/Bullet.png")) {
            bulletImg.resize({4, 2}, sf::Color::Yellow);
        }
        bulletLoaded = true;
    }

    // Convert local barrel position to mapped world rotation coordinates
    float finalAngle = weaponAngle + (flipX ? -visualAngleOffset : visualAngleOffset);
    float rad = finalAngle * PI / 180.0f;
    
    float ox = barrelOffset.x;
    float oy = barrelOffset.y;
    if (flipX) ox = -ox;
    
    float cs = std::cos(rad);
    float sn = std::sin(rad);
    
    float world_dx = ox * cs - oy * sn;
    float world_dy = ox * sn + oy * cs;
    
    sf::Vector2f barrelPos = {handPos.x + world_dx, handPos.y + world_dy};

    // Calculate final aim trajectory
    sf::Vector2f dir = targetPos - barrelPos;
    float dist = std::hypot(dir.x, dir.y);
    if (dist > 0.001f) {
        dir.x /= dist;
        dir.y /= dist;
    } else {
        dir = {flipX ? -1.0f : 1.0f, 0.0f};
    }
    
    float shootAngle = std::atan2(dir.y, dir.x) * 180.0f / PI;

    // Spawn the bullet, moving at a smooth, visible speed of 150 m/s
    auto bullet = std::make_unique<Bullet>(worldId, bulletImg, static_cast<int>(barrelPos.x), static_cast<int>(barrelPos.y), shootAngle, 150.0f);
    rbs.addBody(std::move(bullet));
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
            
            b2Vec2 initVel = {dir.x * 65.0f, dir.y * 65.0f}; 
            
            auto wheel = std::make_unique<Wheel>(worldId, wheelImg, wx, wy);
            b2Body_SetLinearVelocity(wheel->bodyId, initVel);
            b2Body_SetAngularVelocity(wheel->bodyId, dir.x > 0 ? 30.0f : -30.0f);
            
            rbs.addBody(std::move(wheel));
        }
    }
}