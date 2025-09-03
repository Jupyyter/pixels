#include "ParticleWorld.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <fstream>
#include <filesystem>

namespace SandSim 
{

ParticleWorld::ParticleWorld(unsigned int w, unsigned int h, const std::string &worldFile)
    : width(w), height(h), frameCounter(0)
{
    particles.resize(width * height);
    pixelBuffer.resize(width * height * 4); // RGBA

    if (!worldFile.empty() && std::filesystem::exists(worldFile))
    {
        if (!loadWorld(worldFile))
        {
            std::cerr << "Failed to load world file, starting with empty world" << std::endl;
            clear();
        }
    }
    else
    {
        clear();
    }
}

void ParticleWorld::clear()
{
    for (auto &p : particles)
    {
        p = Particle::createEmpty();
    }
    std::fill(pixelBuffer.begin(), pixelBuffer.end(), 0);
}

void ParticleWorld::setParticleAt(int x, int y, const Particle &particle)
{
    if (!inBounds(x, y))
        return;

    int idx = computeIndex(x, y);
    particles[idx] = particle;

    // Update pixel buffer for rendering
    int pixelIdx = idx * 4;
    pixelBuffer[pixelIdx] = particle.color.r;
    pixelBuffer[pixelIdx + 1] = particle.color.g;
    pixelBuffer[pixelIdx + 2] = particle.color.b;
    pixelBuffer[pixelIdx + 3] = particle.color.a;
}

void ParticleWorld::swapParticles(int x1, int y1, int x2, int y2)
{
    if (!inBounds(x1, y1) || !inBounds(x2, y2))
        return;

    Particle temp = getParticleAt(x1, y1);
    setParticleAt(x1, y1, getParticleAt(x2, y2));
    setParticleAt(x2, y2, temp);
}

bool ParticleWorld::isInLiquid(int x, int y, int *lx, int *ly) const
{
    // Check 8-directional neighbors for liquid particles
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny))
            {
                const auto &p = getParticleAt(nx, ny);
                if (p.id == MaterialID::Water || p.id == MaterialID::Oil)
                {
                    *lx = nx;
                    *ly = ny;
                    return true;
                }
            }
        }
    }
    return false;
}

bool ParticleWorld::isInWater(int x, int y, int *lx, int *ly) const
{
    // Check 8-directional neighbors for water specifically
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny))
            {
                const auto &p = getParticleAt(nx, ny);
                if (p.id == MaterialID::Water)
                {
                    *lx = nx;
                    *ly = ny;
                    return true;
                }
            }
        }
    }
    return false;
}

void ParticleWorld::update(float deltaTime)
{
    frameCounter++;
    bool frameCounterEven = (frameCounter % 2) == 0;
    int ran = frameCounterEven ? 0 : 1;

    // Process from bottom to top to prevent double updates
    for (int y = height - 1; y >= 0; --y)
    {
        for (int x = ran ? 0 : width - 1; ran ? x < width : x > 0; ran ? ++x : --x)
        {
            auto &particle = getParticleAt(x, y);

            if (particle.id == MaterialID::Empty)
                continue;

            // Update particle lifetime
            particle.lifeTime += deltaTime;

            // Dispatch to material-specific update function
            switch (particle.id)
            {
            case MaterialID::Sand:      updateSand(x, y, deltaTime); break;
            case MaterialID::Water:     updateWater(x, y, deltaTime); break;
            case MaterialID::Salt:      updateSalt(x, y, deltaTime); break;
            case MaterialID::Fire:      updateFire(x, y, deltaTime); break;
            case MaterialID::Smoke:     updateSmoke(x, y, deltaTime); break;
            case MaterialID::Ember:     updateEmber(x, y, deltaTime); break;
            case MaterialID::Steam:     updateSteam(x, y, deltaTime); break;
            case MaterialID::Gunpowder: updateGunpowder(x, y, deltaTime); break;
            case MaterialID::Oil:       updateOil(x, y, deltaTime); break;
            case MaterialID::Lava:      updateLava(x, y, deltaTime); break;
            case MaterialID::Acid:      updateAcid(x, y, deltaTime); break;
            default: break;
            }
        }
    }

    // Reset frame update flags
    for (auto &p : particles)
    {
        p.hasBeenUpdatedThisFrame = false;
    }
}

void ParticleWorld::addParticleCircle(int centerX, int centerY, float radius, MaterialID materialType)
{
    // Place particles in circular area around center point
    for (int dy = -static_cast<int>(radius); dy <= static_cast<int>(radius); ++dy)
    {
        for (int dx = -static_cast<int>(radius); dx <= static_cast<int>(radius); ++dx)
        {
            int x = centerX + dx;
            int y = centerY + dy;

            // Check distance from center
            float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (distance <= radius)
            {
                // Only place particle if cell is empty
                if (isEmpty(x, y))
                {
                    Particle p = createParticleByType(materialType);
                    // Add random initial velocity
                    p.velocity = {Random::randFloat(-0.5f, 0.5f), Random::randFloat(-0.5f, 0.5f)};
                    setParticleAt(x, y, p);
                }
            }
        }
    }
}

void ParticleWorld::eraseCircle(int centerX, int centerY, float radius)
{
    // Remove particles in circular area
    for (int i = -radius; i < radius; ++i)
    {
        for (int j = -radius; j < radius; ++j)
        {
            int x = centerX + j;
            int y = centerY + i;

            if (inBounds(x, y))
            {
                float distance = std::sqrt(i * i + j * j);
                if (distance <= radius)
                {
                    setParticleAt(x, y, Particle::createEmpty());
                }
            }
        }
    }
}

std::string ParticleWorld::getNextAvailableFilename(const std::string& baseName) 
{
    std::string filename;
    int counter = 0;
    
    do {
        filename = baseName + std::to_string(counter) + ".rrr";
        counter++;
    } while (std::filesystem::exists(filename));
    
    return filename;
}

bool ParticleWorld::saveWorld(const std::string& baseFilename) 
{
    std::string filename = getNextAvailableFilename("worlds/" + baseFilename);
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }
    
    try {
        // Write header: dimensions and frame counter
        file.write(reinterpret_cast<const char*>(&width), sizeof(width));
        file.write(reinterpret_cast<const char*>(&height), sizeof(height));
        file.write(reinterpret_cast<const char*>(&frameCounter), sizeof(frameCounter));
        
        // Write all particle data sequentially
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const auto& particle = getParticleAt(x, y);
                
                file.write(reinterpret_cast<const char*>(&particle.id), sizeof(particle.id));
                file.write(reinterpret_cast<const char*>(&particle.velocity.x), sizeof(particle.velocity.x));
                file.write(reinterpret_cast<const char*>(&particle.velocity.y), sizeof(particle.velocity.y));
                file.write(reinterpret_cast<const char*>(&particle.lifeTime), sizeof(particle.lifeTime));
                file.write(reinterpret_cast<const char*>(&particle.color.r), sizeof(particle.color.r));
                file.write(reinterpret_cast<const char*>(&particle.color.g), sizeof(particle.color.g));
                file.write(reinterpret_cast<const char*>(&particle.color.b), sizeof(particle.color.b));
                file.write(reinterpret_cast<const char*>(&particle.color.a), sizeof(particle.color.a));
            }
        }
        
        file.close();
        std::cout << "World saved successfully as: " << filename << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving world: " << e.what() << std::endl;
        file.close();
        return false;
    }
}

bool ParticleWorld::loadWorld(const std::string& filename) 
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for reading: " << filename << std::endl;
        return false;
    }
    
    try {
        // Read and verify header dimensions
        int fileWidth, fileHeight;
        file.read(reinterpret_cast<char*>(&fileWidth), sizeof(fileWidth));
        file.read(reinterpret_cast<char*>(&fileHeight), sizeof(fileHeight));
        
        if (fileWidth != width || fileHeight != height) {
            std::cerr << "World dimensions mismatch! File: " << fileWidth << "x" << fileHeight 
                      << ", Current: " << width << "x" << height << std::endl;
            file.close();
            return false;
        }
        
        file.read(reinterpret_cast<char*>(&frameCounter), sizeof(frameCounter));
        
        // Read particle data sequentially
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Particle particle;
                
                file.read(reinterpret_cast<char*>(&particle.id), sizeof(particle.id));
                file.read(reinterpret_cast<char*>(&particle.velocity.x), sizeof(particle.velocity.x));
                file.read(reinterpret_cast<char*>(&particle.velocity.y), sizeof(particle.velocity.y));
                file.read(reinterpret_cast<char*>(&particle.lifeTime), sizeof(particle.lifeTime));
                file.read(reinterpret_cast<char*>(&particle.color.r), sizeof(particle.color.r));
                file.read(reinterpret_cast<char*>(&particle.color.g), sizeof(particle.color.g));
                file.read(reinterpret_cast<char*>(&particle.color.b), sizeof(particle.color.b));
                file.read(reinterpret_cast<char*>(&particle.color.a), sizeof(particle.color.a));
                
                particle.hasBeenUpdatedThisFrame = false;
                setParticleAt(x, y, particle);
            }
        }
        
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading world: " << e.what() << std::endl;
        file.close();
        return false;
    }
}

Particle ParticleWorld::createParticleByType(MaterialID type)
{
    switch (type)
    {
    case MaterialID::Sand:      return Particle::createSand();
    case MaterialID::Water:     return Particle::createWater();
    case MaterialID::Salt:      return Particle::createSalt();
    case MaterialID::Wood:      return Particle::createWood();
    case MaterialID::Fire:      return Particle::createFire();
    case MaterialID::Smoke:     return Particle::createSmoke();
    case MaterialID::Ember:     return Particle::createEmber();
    case MaterialID::Steam:     return Particle::createSteam();
    case MaterialID::Gunpowder: return Particle::createGunpowder();
    case MaterialID::Oil:       return Particle::createOil();
    case MaterialID::Lava:      return Particle::createLava();
    case MaterialID::Stone:     return Particle::createStone();
    case MaterialID::Acid:      return Particle::createAcid();
    default:                    return Particle::createEmpty();
    }
}

void ParticleWorld::updateLiquidMovement(int x, int y, float dt, float horizontalChance, float velocityMultiplier) 
{
    auto& p = getParticleAt(x, y);
    
    // Apply gravity with velocity clamping
    p.velocity.y = std::clamp(p.velocity.y + (GRAVITY * dt), -10.0f, 10.0f);
    p.velocity.x *= 0.98f; // Apply friction
    
    // Try movement with accumulated velocity
    int targetX = x + static_cast<int>(std::round(p.velocity.x));
    int targetY = y + static_cast<int>(std::round(p.velocity.y));
    
    if (inBounds(targetX, targetY) && isEmpty(targetX, targetY)) {
        swapParticles(x, y, targetX, targetY);
        return;
    }
    
    // Direct downward fall
    if (inBounds(x, y + 1) && isEmpty(x, y + 1)) {
        swapParticles(x, y, x, y + 1);
        return;
    }
    
    // Diagonal fall with velocity influence
    bool canFallLeft = inBounds(x - 1, y + 1) && isEmpty(x - 1, y + 1);
    bool canFallRight = inBounds(x + 1, y + 1) && isEmpty(x + 1, y + 1);
    
    if (canFallLeft && canFallRight) {
        // Choose direction based on horizontal velocity or randomly
        if (std::abs(p.velocity.x) > 0.1f) {
            if (p.velocity.x < 0) {
                swapParticles(x, y, x - 1, y + 1);
            } else {
                swapParticles(x, y, x + 1, y + 1);
            }
        } else {
            swapParticles(x, y, Random::randBool() ? x - 1 : x + 1, y + 1);
        }
        return;
    }
    
    if (canFallLeft) {
        p.velocity.x = std::clamp(p.velocity.x - velocityMultiplier, -5.0f, 5.0f);
        swapParticles(x, y, x - 1, y + 1);
        return;
    }
    
    if (canFallRight) {
        p.velocity.x = std::clamp(p.velocity.x + velocityMultiplier, -5.0f, 5.0f);
        swapParticles(x, y, x + 1, y + 1);
        return;
    }
    
    // Horizontal flow with pressure simulation
    bool canFlowLeft = inBounds(x - 1, y) && isEmpty(x - 1, y);
    bool canFlowRight = inBounds(x + 1, y) && isEmpty(x + 1, y);
    
    if (canFlowLeft || canFlowRight) {
        p.velocity.x += Random::randFloat(-0.8f, 0.8f);
        
        if (canFlowLeft && (p.velocity.x < 0 || !canFlowRight)) {
            if (Random::chance(2)) {
                swapParticles(x, y, x - 1, y);
                return;
            }
        }
        
        if (canFlowRight && (p.velocity.x > 0 || !canFlowLeft)) {
            if (Random::chance(2)) {
                swapParticles(x, y, x + 1, y);
                return;
            }
        }
    }
    
    // Apply damping when blocked
    p.velocity.y *= 0.7f;
    p.velocity.x *= 0.8f;
}

void ParticleWorld::updateSolidMovement(int x, int y, float dt, bool canDisplaceLiquids) 
{
    auto& p = getParticleAt(x, y);
    
    // Apply gravity acceleration
    p.velocity.y = std::clamp(p.velocity.y + (GRAVITY * dt), -15.0f, 15.0f);
    
    // Add horizontal randomness when falling fast
    if (p.velocity.y > 2.0f) {
        p.velocity.x += Random::randFloat(-0.1f, 0.1f);
        p.velocity.x = std::clamp(p.velocity.x, -2.0f, 2.0f);
    }
    
    // Calculate target movement based on velocity
    int moveY = static_cast<int>(std::round(p.velocity.y));
    int moveX = static_cast<int>(std::round(p.velocity.x));
    int targetY = y + moveY;
    int targetX = x + moveX;
    
    // Try direct movement to target position
    if (inBounds(targetX, targetY)) {
        auto& target = getParticleAt(targetX, targetY);
        if (target.id == MaterialID::Empty || 
            (canDisplaceLiquids && (target.id == MaterialID::Water || target.id == MaterialID::Oil) && !target.hasBeenUpdatedThisFrame)) {
            
            if (target.id == MaterialID::Water || target.id == MaterialID::Oil) {
                target.hasBeenUpdatedThisFrame = true;
                
                // Find nearby empty spot for displaced liquid
                bool placed = false;
                for (int searchRadius = 1; searchRadius <= 3 && !placed; searchRadius++) {
                    for (int dy = -searchRadius; dy <= 0 && !placed; dy++) {
                        for (int dx = -searchRadius; dx <= searchRadius && !placed; dx++) {
                            if (isEmpty(x + dx, y + dy)) {
                                setParticleAt(x + dx, y + dy, target);
                                placed = true;
                            }
                        }
                    }
                }
            }
            
            swapParticles(x, y, targetX, targetY);
            return;
        }
    }
    
    // Step-by-step downward movement if direct movement failed
    for (int step = 1; step <= std::abs(moveY); step++) {
        int testY = y + step;
        if (inBounds(x, testY)) {
            auto& below = getParticleAt(x, testY);
            if (below.id == MaterialID::Empty || 
                (canDisplaceLiquids && (below.id == MaterialID::Water || below.id == MaterialID::Oil) && !below.hasBeenUpdatedThisFrame)) {
                
                if (below.id == MaterialID::Water || below.id == MaterialID::Oil) {
                    // Reduced splash displacement
                    below.velocity = {Random::randFloat(-1.5f, 1.5f), Random::randFloat(-0.5f, -1.5f)};
                    below.hasBeenUpdatedThisFrame = true;
                    
                    // Find spot for displaced liquid
                    bool placed = false;
                    for (int searchRadius = 1; searchRadius <= 3 && !placed; searchRadius++) {
                        for (int dy = -searchRadius; dy <= 0 && !placed; dy++) {
                            for (int dx = -searchRadius; dx <= searchRadius && !placed; dx++) {
                                if (isEmpty(x + dx, y + dy)) {
                                    setParticleAt(x + dx, y + dy, below);
                                    placed = true;
                                }
                            }
                        }
                    }
                }
                
                swapParticles(x, y, x, testY);
                return;
            } else {
                // Hit obstacle - reduce velocity and try diagonal
                p.velocity.y *= 0.3f;
                break;
            }
        }
    }
    
    // Try diagonal movement if vertical blocked
    if (moveY > 0) {
        int diagX = x + (moveX > 0 ? 1 : (moveX < 0 ? -1 : (Random::randBool() ? 1 : -1)));
        
        if (inBounds(diagX, y + 1)) {
            auto& diag = getParticleAt(diagX, y + 1);
            if (diag.id == MaterialID::Empty || 
                (canDisplaceLiquids && (diag.id == MaterialID::Water || diag.id == MaterialID::Oil))) {
                
                p.velocity.x = (diagX > x) ? std::abs(p.velocity.x) : -std::abs(p.velocity.x);
                swapParticles(x, y, diagX, y + 1);
                return;
            }
        }
    }
    
    // Settle in liquid if surrounded
    int lx, ly;
    if (isInLiquid(x, y, &lx, &ly) && Random::chance(15)) {
        swapParticles(x, y, lx, ly);
        p.velocity.y *= 0.5f;
    }
    
    // Apply friction when not moving
    p.velocity.x *= 0.9f;
    if (std::abs(p.velocity.x) < 0.1f) p.velocity.x = 0.0f;
}

void ParticleWorld::updateGasMovement(int x, int y, float dt, float buoyancy, float chaosLevel) 
{
    auto& p = getParticleAt(x, y);
    
    // Apply buoyancy (negative gravity)
    p.velocity.y = std::clamp(p.velocity.y - (GRAVITY * dt * buoyancy), -5.0f, 2.0f);
    
    // Add chaotic horizontal movement
    p.velocity.x += Random::randFloat(-chaosLevel, chaosLevel);
    p.velocity.x = std::clamp(p.velocity.x, -3.0f, 3.0f);
    
    // Random turbulence
    if (Random::chance(5)) {
        p.velocity.x += Random::randFloat(-1.0f, 1.0f);
        p.velocity.y += Random::randFloat(-0.5f, 0.5f);
    }
    
    // Calculate target position
    int targetX = x + static_cast<int>(std::round(p.velocity.x));
    int targetY = y + static_cast<int>(std::round(p.velocity.y));
    
    // Try direct movement (gases can pass through liquids)
    if (inBounds(targetX, targetY) && 
        (isEmpty(targetX, targetY) || 
         getParticleAt(targetX, targetY).id == MaterialID::Water ||
         getParticleAt(targetX, targetY).id == MaterialID::Oil)) {
        swapParticles(x, y, targetX, targetY);
        return;
    }
    
    // Try upward movement
    if (inBounds(x, y - 1) && 
        (isEmpty(x, y - 1) || 
         getParticleAt(x, y - 1).id == MaterialID::Water ||
         getParticleAt(x, y - 1).id == MaterialID::Oil)) {
        swapParticles(x, y, x, y - 1);
        return;
    }
    
    // Try horizontal movement
    int direction = (Random::randFloat(-1.0f, 1.0f) > 0) ? 1 : -1;
    if (inBounds(x + direction, y) && 
        (isEmpty(x + direction, y) || 
         getParticleAt(x + direction, y).id == MaterialID::Water ||
         getParticleAt(x + direction, y).id == MaterialID::Oil)) {
        p.velocity.x += direction * 0.5f;
        swapParticles(x, y, x + direction, y);
        return;
    }
    
    // Try opposite direction
    if (inBounds(x - direction, y) && 
        (isEmpty(x - direction, y) || 
         getParticleAt(x - direction, y).id == MaterialID::Water ||
         getParticleAt(x - direction, y).id == MaterialID::Oil)) {
        p.velocity.x -= direction * 0.5f;
        swapParticles(x, y, x - direction, y);
        return;
    }
    
    // Try diagonal upward movement
    for (int dx = -1; dx <= 1; dx += 2) {
        if (inBounds(x + dx, y - 1) && 
            (isEmpty(x + dx, y - 1) || 
             getParticleAt(x + dx, y - 1).id == MaterialID::Water ||
             getParticleAt(x + dx, y - 1).id == MaterialID::Oil)) {
            swapParticles(x, y, x + dx, y - 1);
            return;
        }
    }
    
    // Apply friction when stuck
    p.velocity.x *= 0.8f;
    p.velocity.y *= 0.9f;
}

void ParticleWorld::updateSand(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Apply gravity acceleration for realistic falling
    p.velocity.y = std::clamp(p.velocity.y + (GRAVITY * dt), -10.0f, 10.0f);
    
    updateSolidMovement(x, y, dt, true);
}

void ParticleWorld::updateWater(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    updateLiquidMovement(x, y, dt, 2.0f, 2.0f);
}

void ParticleWorld::updateSalt(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Dissolve in nearby liquid
    int lx, ly;
    if (isInLiquid(x, y, &lx, &ly) && Random::chance(800)) {
        setParticleAt(x, y, Particle::createEmpty());
        return;
    }
    
    updateSolidMovement(x, y, dt, false);
}

void ParticleWorld::updateFire(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Fire dies after lifetime or randomly
    if (p.lifeTime > 1.5f || (p.lifeTime > 0.3f && Random::chance(150))) {
        // Create byproducts when dying
        if (Random::chance(5)) {
            setParticleAt(x, y, Particle::createEmber());
        } else if (Random::chance(3)) {
            setParticleAt(x, y, Particle::createSmoke());
        } else {
            setParticleAt(x, y, Particle::createEmpty());
        }
        return;
    }
    
    // Update fire color randomly
    if (Random::chance(20)) {
        int colorVariant = Random::randInt(0, 3);
        switch (colorVariant) {
            case 0: p.color = sf::Color(255, 80, 20, 255); break;
            case 1: p.color = sf::Color(250, 150, 10, 255); break;
            case 2: p.color = sf::Color(200, 150, 0, 255); break;
            case 3: p.color = sf::Color(255, 200, 50, 255); break;
        }
    }
    
    // Ignite nearby flammable materials
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny)) {
                auto& neighbor = getParticleAt(nx, ny);
                if ((neighbor.id == MaterialID::Wood || neighbor.id == MaterialID::Oil || 
                     neighbor.id == MaterialID::Gunpowder) && Random::chance(100)) {
                    setParticleAt(nx, ny, Particle::createFire());
                }
            }
        }
    }
    
    // Create steam when touching water
    int lx, ly;
    if (isInWater(x, y, &lx, &ly) && Random::chance(5)) {
        setParticleAt(x, y, Particle::createSteam());
        setParticleAt(lx, ly, Particle::createSteam());
        return;
    }
}

void ParticleWorld::updateSmoke(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Smoke dissipates over time
    if (p.lifeTime > 15.0f) {
        setParticleAt(x, y, Particle::createEmpty());
        return;
    }
    
    // Fade color based on lifetime
    float lifeFactor = std::clamp((15.0f - p.lifeTime) / 15.0f, 0.1f, 1.0f);
    p.color.r = static_cast<uint8_t>(lifeFactor * 80);
    p.color.g = static_cast<uint8_t>(lifeFactor * 70);
    p.color.b = static_cast<uint8_t>(lifeFactor * 60);
    p.color.a = static_cast<uint8_t>(lifeFactor * 255);
    
    updateGasMovement(x, y, dt, 0.8f, 1.2f);
}

void ParticleWorld::updateEmber(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Embers die quickly
    if (p.lifeTime > 0.2f && Random::chance(100)) {
        setParticleAt(x, y, Particle::createEmpty());
        return;
    }
    
    // Embers rise with slight buoyancy
    p.velocity.y = std::clamp(p.velocity.y - (GRAVITY * dt) * 0.2f, -5.0f, 0.0f);
    p.velocity.x = std::clamp(p.velocity.x + Random::randFloat(-0.01f, 0.01f), -0.5f, 0.5f);
    
    // Random color variation
    if (Random::chance(static_cast<int>(p.lifeTime * 100.0f + 1)) && Random::chance(200)) {
        int colorVariant = Random::randInt(0, 3);
        switch (colorVariant) {
            case 0: p.color = sf::Color(255, 80, 20, 255); break;
            case 1: p.color = sf::Color(250, 150, 10, 255); break;
            case 2: p.color = sf::Color(200, 150, 0, 255); break;
            case 3: p.color = sf::Color(100, 50, 2, 255); break;
        }
        // Update pixel buffer directly for color changes
        int idx = computeIndex(x, y);
        int pixelIdx = idx * 4;
        pixelBuffer[pixelIdx] = p.color.r;
        pixelBuffer[pixelIdx + 1] = p.color.g;
        pixelBuffer[pixelIdx + 2] = p.color.b;
        pixelBuffer[pixelIdx + 3] = p.color.a;
    }
    
    // Embers can still ignite wood
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny) && getParticleAt(nx, ny).id == MaterialID::Wood && Random::chance(150)) {
                setParticleAt(nx, ny, Particle::createFire());
            }
        }
    }
    
    // Create steam when touching water
    int lx, ly;
    if (isInWater(x, y, &lx, &ly)) {
        if (Random::randBool()) {
            // Generate steam in area above
            for (int i = -5; i > -5; --i) {
                for (int j = -5; j < 5; ++j) {
                    if (inBounds(x + j, y + i) && isEmpty(x + j, y + i)) {
                        setParticleAt(x + j, y + i, Particle::createSteam());
                        break;
                    }
                }
            }
            setParticleAt(lx, ly, Particle::createEmpty());
            setParticleAt(x, y, Particle::createEmpty());
            return;
        }
    }
    
    // Simple upward movement
    int vx = static_cast<int>(p.velocity.x);
    int vy = static_cast<int>(p.velocity.y);
    
    if (inBounds(x + vx, y + vy) && (isEmpty(x + vx, y + vy) || 
        getParticleAt(x + vx, y + vy).id == MaterialID::Water ||
        getParticleAt(x + vx, y + vy).id == MaterialID::Smoke)) {
        swapParticles(x, y, x + vx, y + vy);
    }
    else if (inBounds(x, y - 1) && (isEmpty(x, y - 1) || 
             getParticleAt(x, y - 1).id == MaterialID::Water ||
             getParticleAt(x, y - 1).id == MaterialID::Smoke)) {
        swapParticles(x, y, x, y - 1);
    }
}

void ParticleWorld::updateSteam(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Steam dissipates over time
    if (p.lifeTime > 12.0f) {
        setParticleAt(x, y, Particle::createEmpty());
        return;
    }
    
    // Fade transparency based on lifetime
    float lifeFactor = std::clamp((12.0f - p.lifeTime) / 12.0f, 0.1f, 1.0f);
    p.color.a = static_cast<uint8_t>(lifeFactor * 255 * 0.8f);
    
    updateGasMovement(x, y, dt, 1.0f, 1.8f);
    
    // Condense back to water when cooled
    if (p.lifeTime > 8.0f && Random::chance(200)) {
        setParticleAt(x, y, Particle::createWater());
        return;
    }
}

void ParticleWorld::updateGunpowder(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Check for nearby fire to trigger explosion
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny) && getParticleAt(nx, ny).id == MaterialID::Fire) {
                // Create explosion in radius
                for (int ey = -4; ey <= 4; ey++) {
                    for (int ex = -4; ex <= 4; ex++) {
                        int explosionX = x + ex, explosionY = y + ey;
                        if (inBounds(explosionX, explosionY)) {
                            float distance = std::sqrt(ex * ex + ey * ey);
                            if (distance <= 4.0f && Random::chance(3)) {
                                setParticleAt(explosionX, explosionY, Particle::createFire());
                            }
                        }
                    }
                }
                return;
            }
        }
    }
    
    updateSolidMovement(x, y, dt, true);
}

void ParticleWorld::updateOil(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Check for fire ignition
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny) && getParticleAt(nx, ny).id == MaterialID::Fire && Random::chance(30)) {
                setParticleAt(x, y, Particle::createFire());
                return;
            }
        }
    }
    
    // Oil flows like thick liquid
    updateLiquidMovement(x, y, dt, 4.0f, 1.5f);
}

void ParticleWorld::updateLava(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Apply gravity with slight reduction for viscosity
    p.velocity.y = std::clamp(p.velocity.y + (GRAVITY * dt * 0.85f), -8.0f, 8.0f);
    p.velocity.x *= 0.95f;
    
    // Ignite nearby flammable materials
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny)) {
                auto& neighbor = getParticleAt(nx, ny);
                if ((neighbor.id == MaterialID::Wood || neighbor.id == MaterialID::Oil || neighbor.id == MaterialID::Gunpowder) && Random::chance(80)) {
                    setParticleAt(nx, ny, Particle::createFire());
                }
                else if (neighbor.id == MaterialID::Water && Random::chance(15)) {
                    setParticleAt(nx, ny, Particle::createSteam());
                }
            }
        }
    }
    
    // Movement with accumulated velocity
    int targetX = x + static_cast<int>(std::round(p.velocity.x));
    int targetY = y + static_cast<int>(std::round(p.velocity.y));
    
    if (inBounds(targetX, targetY) && isEmpty(targetX, targetY)) {
        swapParticles(x, y, targetX, targetY);
        return;
    }
    
    // Direct downward fall
    if (inBounds(x, y + 1) && isEmpty(x, y + 1)) {
        swapParticles(x, y, x, y + 1);
        return;
    }
    
    // Diagonal fall
    bool canFallLeft = inBounds(x - 1, y + 1) && isEmpty(x - 1, y + 1);
    bool canFallRight = inBounds(x + 1, y + 1) && isEmpty(x + 1, y + 1);
    
    if (canFallLeft && canFallRight) {
        if (std::abs(p.velocity.x) > 0.1f) {
            if (p.velocity.x < 0) {
                swapParticles(x, y, x - 1, y + 1);
            } else {
                swapParticles(x, y, x + 1, y + 1);
            }
        } else {
            swapParticles(x, y, Random::randBool() ? x - 1 : x + 1, y + 1);
        }
        return;
    }
    
    if (canFallLeft) {
        p.velocity.x = std::clamp(p.velocity.x - 0.8f, -4.0f, 4.0f);
        swapParticles(x, y, x - 1, y + 1);
        return;
    }
    
    if (canFallRight) {
        p.velocity.x = std::clamp(p.velocity.x + 0.8f, -4.0f, 4.0f);
        swapParticles(x, y, x + 1, y + 1);
        return;
    }
    
    // Horizontal flow (slower than water)
    bool canFlowLeft = inBounds(x - 1, y) && isEmpty(x - 1, y);
    bool canFlowRight = inBounds(x + 1, y) && isEmpty(x + 1, y);
    
    if (canFlowLeft || canFlowRight) {
        p.velocity.x += Random::randFloat(-0.3f, 0.3f);
        
        if (canFlowLeft && (p.velocity.x < 0 || !canFlowRight)) {
            if (Random::chance(5)) {
                swapParticles(x, y, x - 1, y);
                return;
            }
        }
        
        if (canFlowRight && (p.velocity.x > 0 || !canFlowLeft)) {
            if (Random::chance(5)) {
                swapParticles(x, y, x + 1, y);
                return;
            }
        }
    }
    
    // Apply damping when blocked
    p.velocity.y *= 0.8f;
    p.velocity.x *= 0.85f;
}

void ParticleWorld::updateAcid(int x, int y, float dt) 
{
    auto& p = getParticleAt(x, y);
    if (p.hasBeenUpdatedThisFrame) return;
    p.hasBeenUpdatedThisFrame = true;
    
    // Dissolve nearby materials (except stone and acid)
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (inBounds(nx, ny)) {
                auto& neighbor = getParticleAt(nx, ny);
                if (neighbor.id != MaterialID::Empty && neighbor.id != MaterialID::Acid && 
                    neighbor.id != MaterialID::Stone && Random::chance(300)) {
                    setParticleAt(nx, ny, Particle::createEmpty());
                }
            }
        }
    }
    
    updateLiquidMovement(x, y, dt, 3.0f, 1.3f);
}

} // namespace SandSim