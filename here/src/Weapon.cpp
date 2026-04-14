#include "Weapon.hpp"
#include <cstdlib>
#include <iostream>

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
    
     hasCustomRendering = true; 
    // Force all active pixels of the actual rigid body bullet to be yellowish orange!
    for (auto& p : particles) {
        if (p.active) {
            p.base.color = sf::Color(255, 180, 0);
        }
    }
    
    b2Vec2 startPos = {startX * P2M, startY * P2M};
    b2Body_SetTransform(bodyId, startPos, b2MakeRot(angleDeg * PI / 180.0f));
    float vx = std::cos(angleDeg * PI / 180.0f) * speed;
    float vy = std::sin(angleDeg * PI / 180.0f) * speed;
    
    b2Body_SetLinearVelocity(bodyId, {vx, vy});
    b2Body_SetGravityScale(bodyId, 0.0f); // Make it fire straight like a laser
    
    lastPos = startPos;
}

void Bullet::update(float dt, ParticleWorld& world) {
    if (!b2Body_IsValid(bodyId)) return;

    bool shouldExplode = false;
    b2Vec2 vel = b2Body_GetLinearVelocity(bodyId);
    b2Vec2 pos = b2Body_GetPosition(bodyId);

    float currentAngleDeg = 0.0f;
    float speedSq = vel.x * vel.x + vel.y * vel.y;
    if (speedSq > 0.1f) {
        float newAngle = std::atan2(vel.y, vel.x);
        b2Body_SetTransform(bodyId, pos, b2MakeRot(newAngle));
        b2Body_SetAngularVelocity(bodyId, 0.0f); // Instantly kill any spinning
        currentAngleDeg = newAngle * 180.0f / PI;
    } else {
        b2Rot rot = b2Body_GetRotation(bodyId);
        currentAngleDeg = std::atan2(rot.s, rot.c) * 180.0f / PI;
    }
    
    sf::Vector2f currentPos(pos.x * M2P, pos.y * M2P);

    // --- NEW TRAIL LOGIC ---
    // Increase age of existing trail history
    for (auto& node : pathHistory) {
        node.age += dt;
    }
    // Remove nodes that are older than 0.10 seconds
    while (!pathHistory.empty() && pathHistory.back().age > BULLET_TRAIL_DURATION) {
        pathHistory.pop_back();
    }

    if (speedSq > 0.1f) {
        if (!pathHistory.empty()) {
            sf::Vector2f lastPos = pathHistory.front().pos;
            float dist = std::hypot(currentPos.x - lastPos.x, currentPos.y - lastPos.y);
            
            float step = 2.0f; // Interpolate: Spawn a copy every 2 pixels!
            if (dist > step) {
                int steps = static_cast<int>(dist / step);
                for (int i = 1; i <= steps; ++i) {
                    float t = static_cast<float>(i) / steps;
                    sf::Vector2f lerpPos = lastPos + (currentPos - lastPos) * t;
                    pathHistory.push_front({lerpPos, currentAngleDeg, 0.0f});
                }
            } else if (dist > 0.1f) {
                pathHistory.push_front({currentPos, currentAngleDeg, 0.0f});
            }
        } else {
            pathHistory.push_front({currentPos, currentAngleDeg, 0.0f});
        }
    }
    // -----------------------
    
    // Condition 1: Large velocity change (collision with physics solids)
    if (lastVel.x != 0.0f || lastVel.y != 0.0f) {
        float dvx = lastVel.x - vel.x;
        float dvy = lastVel.y - vel.y;
        float dvSq = dvx * dvx + dvy * dvy;
        
        if (dvSq > 2000.0f) {
            shouldExplode = true;
        }
    }
    lastVel = vel;

    // Condition 2: Lifetime expiry
    lifeTime -= dt;
    if (lifeTime <= 0.0f) {
        shouldExplode = true;
    }

    int hitX = static_cast<int>(pos.x * M2P);
    int hitY = static_cast<int>(pos.y * M2P);

    // Condition 3: Sweep Raycast through the destructible particle grid to prevent tunneling
    if (!shouldExplode) {
        int startX = static_cast<int>(lastPos.x * M2P);
        int startY = static_cast<int>(lastPos.y * M2P);
        
        if (lastPos.x == 0.0f && lastPos.y == 0.0f) {
            startX = hitX;
            startY = hitY;
        }

        int dx = std::abs(hitX - startX), sx = startX < hitX ? 1 : -1;
        int dy = std::abs(hitY - startY), sy = startY < hitY ? 1 : -1;
        int err = dx - dy, e2;
        
        int cx = startX, cy = startY;
        
        while (true) {
            BaseComponent* base = world.get<BaseComponent>(cx, cy);
            if (base && base->compMask != 0 && !base->flags.isRigidBodyPart) {
                Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                if (logic) {
                    MaterialGroup group = logic->getGroup();
                    if (group == MaterialGroup::MovableSolid || group == MaterialGroup::Liquid || group == MaterialGroup::ImmovableSolid) {
                        shouldExplode = true;
                        hitX = cx;
                        hitY = cy;
                        break;
                    }
                }
            }
            
            if (cx == hitX && cy == hitY) break;
            e2 = 2 * err;
            if (e2 > -dy) { err -= dy; cx += sx; }
            if (e2 < dx)  { err += dx; cy += sy; }
        }
    }

    if (shouldExplode) {
        lastPos = {hitX * P2M, hitY * P2M}; 
        
        int radius = static_cast<int>(std::max(width, height) * 1.5f);
        if (radius < 4) radius = 4;
        
        world.triggerExplosion(hitX, hitY, radius, 30);
        isDestroyed = true;
    } else {
        lastPos = pos;
    }
}

void Bullet::renderEffects(sf::RenderTarget& target) {
    if (!b2Body_IsValid(bodyId)) return;

    // NEW: Calculate the bullet's age to control the initial tail length.
    // lifeTime starts at 2.0 and counts down.
    float bulletAge = 2.0f - lifeTime;

    // Draw the trail iterating backwards so the smaller/older copies are drawn beneath newer ones!
    for (auto it = pathHistory.rbegin(); it != pathHistory.rend(); ++it) {
        // NEW: Don't draw parts of the trail that are "older" than the bullet itself.
        // This makes the tail appear to grow from nothing, preventing the initial overlap.
        if (it->age > bulletAge) {
            continue;
        }

        // Linearly drops from 1.0 down to 0.0 over the trail's duration
        float scale = 1.0f - (it->age / BULLET_TRAIL_DURATION);
        if (scale < 0.0f) scale = 0.0f;
        if (scale > 1.0f) scale = 1.0f;

        // Skip drawing if it has essentially shrunk into nothingness
        if (scale < 0.05f) continue;

        // Using your newly augmented exact pixel rendering to draw the scaled-down bullet copies!
        renderPixelated(target, it->pos, it->angleDeg, false, sf::Color::Transparent, false, scale);
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
        
        bulletOffset = {12, -2.25f};
        flashOffset = {9.0f, -2.25f};
        
        recoilForce = 4.0f;
        visualRecoilAngle = 40.0f;
        
    } else if (weaponName.find("Submachine") != std::string::npos) {
        isGun = true;
        semiAuto = false;
        fireRate = 0.1f; 
        
        bulletOffset = {12, -2.25f};
        flashOffset = {9.25f, -2.25f};
        
        recoilForce = 1.0f;
        visualRecoilAngle = 15.0f;
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

void Weapon::update(float dt, ParticleWorld& world) {
    if (flashTimer > 0.0f) {
        flashTimer -= dt;
    }
}

void Weapon::fire(sf::Vector2f handPos, sf::Vector2f targetPos, float weaponAngle, bool flipX, RigidBodySystem& rbs, ParticleWorld& world) {
    static sf::Image bulletImg;
    static bool bulletLoaded = false;
    
    if (!bulletLoaded) {
        if (!bulletImg.loadFromFile("assets/images/weapons/Bullet.png")) {
            bulletImg.resize({8, 2}, sf::Color(255, 180, 0)); // Yellowish orange fallback
        }
        bulletLoaded = true;
    }

    // 1. Calculate precise bullet start position
    float finalAngle = weaponAngle + (flipX ? -visualAngleOffset : visualAngleOffset);
    float rad = finalAngle * PI / 180.0f;
    
    float ox = bulletOffset.x;
    float oy = bulletOffset.y;
    if (flipX) ox = -ox;
    
    float cs = std::cos(rad);
    float sn = std::sin(rad);
    
    float world_dx = ox * cs - oy * sn;
    float world_dy = ox * sn + oy * cs;
    
    sf::Vector2f idealBarrelPos = {handPos.x + world_dx, handPos.y + world_dy};

    // 2. Raycast from hand to barrel to prevent bullet spawning stuck inside a wall
    int hX = static_cast<int>(handPos.x);
    int hY = static_cast<int>(handPos.y);
    int bX = static_cast<int>(idealBarrelPos.x);
    int bY = static_cast<int>(idealBarrelPos.y);
    
    int dx = std::abs(bX - hX), sx = hX < bX ? 1 : -1;
    int dy = std::abs(bY - hY), sy = hY < bY ? 1 : -1;
    int err = dx - dy, e2;
    
    int cx = hX, cy = hY;
    int lastValidX = hX, lastValidY = hY;
    
    while(true) {
        BaseComponent* base = world.get<BaseComponent>(cx, cy);
        if (base && base->compMask != 0 && !base->flags.isRigidBodyPart) {
            Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
            if (logic && logic->getGroup() != MaterialGroup::Gas) {
                break; // Hit terrain, clamp the barrel here
            }
        }
        
        lastValidX = cx;
        lastValidY = cy;
        
        if (cx == bX && cy == bY) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 < dx)  { err += dx; cy += sy; }
    }
    
    sf::Vector2f barrelPos = {static_cast<float>(lastValidX), static_cast<float>(lastValidY)};

    // 3. Set physical trajectory (shootAngle)
    float dir_ox = flipX ? -1.0f : 1.0f;
    float dir_dx = dir_ox * cs;
    float dir_dy = dir_ox * sn;
    
    float shootAngle = std::atan2(dir_dy, dir_dx) * 180.0f / PI;

    // 4. Spawn the Bullet
    auto bullet = std::make_unique<Bullet>(worldId, bulletImg, static_cast<int>(barrelPos.x), static_cast<int>(barrelPos.y), shootAngle, 100.0f);
    rbs.addBody(std::move(bullet));

    // 5. Trigger the visual muzzle flash directly on the weapon
    flashTimer = 0.05f; // lasts for 50 milliseconds
    flashFrame = std::rand() % 10;
}

void Weapon::renderPixelated(sf::RenderTarget& target, sf::Vector2f pos, float angleDeg, bool flipX, sf::Color overrideColor, bool applyVisualOffset, float scale) {
    // Render the base physical gun pixels first passing along the scaling parameter
    RigidBody::renderPixelated(target, pos, angleDeg, flipX, overrideColor, applyVisualOffset, scale);

    // If the gun just fired, safely overlay the flash sprite on top!
    if (flashTimer > 0.0f && isEquipped && overrideColor == sf::Color::Transparent) {
        static sf::Texture flashTex;
        static bool flashLoaded = false;
        
        if (!flashLoaded) {
            // (void) suppresses the SFML 3 [[nodiscard]] warning
            (void)flashTex.loadFromFile("assets/images/weapons/gunFireSheet.png");
            flashLoaded = true;
        }

        // Apply visual angle logic 
        float finalAngle = angleDeg;
        if (applyVisualOffset) {
            finalAngle += (flipX ? -visualAngleOffset : visualAngleOffset);
        }

        float rad = finalAngle * PI / 180.0f;
        float cs = std::cos(rad);
        float sn = std::sin(rad);

        float ox = flashOffset.x;
        float oy = flashOffset.y;
        if (flipX) ox = -ox;

        // Calculate translation perfectly synced to where the gun is on the screen right now
        float world_dx = ox * cs - oy * sn;
        float world_dy = ox * sn + oy * cs;

        sf::Sprite flashSprite(flashTex);
        
        // SFML 3: Extract the selected random frame using Vector pairs ({x,y}, {w,h})
        int frameWidth = flashTex.getSize().x / 10; 
        int frameHeight = flashTex.getSize().y;
        flashSprite.setTextureRect(sf::IntRect({flashFrame * frameWidth, 0}, {frameWidth, frameHeight}));
        
        // Center rotation directly on the tip
        flashSprite.setOrigin({0.0f, 16.0f}); // SFML 3 requires vector initialization here too
        
        // Setup final transform overrides
        flashSprite.setPosition({pos.x + world_dx, pos.y + world_dy});
        
        // SFML 3 requires strict sf::Angle wrapping instead of raw floats
        flashSprite.setRotation(sf::degrees(finalAngle));

        if (flipX) {
            flashSprite.setScale({-1.0f, 1.0f});
        }

        target.draw(flashSprite);
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
            
            b2Vec2 initVel = {dir.x * 65.0f, dir.y * 65.0f}; 
            
            auto wheel = std::make_unique<Wheel>(worldId, wheelImg, wx, wy);
            b2Body_SetLinearVelocity(wheel->bodyId, initVel);
            b2Body_SetAngularVelocity(wheel->bodyId, dir.x > 0 ? 30.0f : -30.0f);
            
            rbs.addBody(std::move(wheel));
        }
    }
}