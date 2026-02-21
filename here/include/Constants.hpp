#pragma once
#include <SFML/Graphics/Color.hpp>
#include <string>
#include <vector>
#include <functional>
#include <memory>

class Particle; 

// Window settings
constexpr unsigned int TEXTURE_WIDTH = 1280;  
constexpr unsigned int TEXTURE_HEIGHT = 720; 

// Keep the window size standard
constexpr unsigned int WINDOW_WIDTH = 1280;
constexpr unsigned int WINDOW_HEIGHT = 720;

// Physics settings
constexpr float GRAVITY = 800.0f;
constexpr float DEFAULT_SELECTION_RADIUS = 10.0f;
constexpr float MIN_SELECTION_RADIUS = 1.0f;
constexpr float MAX_SELECTION_RADIUS = 100.0f;


// --- THE MASTER LIST ---
// Define everything here. 1st param: Class Name, 2nd param: Color
#define PARTICLE_DATA(X) \
    X(Sand,          { sf::Color(150, 100, 50), sf::Color(170, 120, 60) }) \
    X(Water,         { sf::Color(20, 100, 170), sf::Color(30, 110, 180) }) \
    X(Stone,         { sf::Color(120, 110, 120), sf::Color(100, 95, 100) }) \
    X(Wood,          { sf::Color(60, 40, 20), sf::Color(70, 50, 30), sf::Color(55, 35, 15) }) \
    X(Salt,          { sf::Color(200, 180, 190), sf::Color(220, 210, 215) }) \
    X(Smoke,         { sf::Color(50, 50, 50), sf::Color(60, 60, 60), sf::Color(40, 40, 40) }) \
    X(Steam,         { sf::Color(220, 220, 250), sf::Color(230, 230, 255) }) \
    X(Gunpowder,     { sf::Color(60, 60, 60), sf::Color(45, 45, 45) }) \
    X(Oil,           { sf::Color(80, 70, 60), sf::Color(65, 55, 45) }) \
    X(Lava,          { sf::Color(200, 50, 0), sf::Color(220, 60, 10), sf::Color(180, 40, 0) }) \
    X(Acid,          { sf::Color(90, 200, 60), sf::Color(100, 215, 70) }) \
    X(Snow,          { sf::Color(255, 250, 250), sf::Color(235, 240, 255) }) \
    X(Dirt,          { sf::Color(182, 159, 102), sf::Color(160, 140, 90) }) \
    X(Coal,          { sf::Color(115, 116, 115), sf::Color(90, 90, 90) }) \
    X(Ember,         { sf::Color(200, 120, 20), sf::Color(220, 140, 40) }) \
    X(Cement,        { sf::Color(165, 163, 145), sf::Color(150, 148, 130) }) \
    X(Blood,         { sf::Color(136, 8, 8), sf::Color(110, 0, 0) }) \
    X(FlammableGas,  { sf::Color(0, 255, 0), sf::Color(20, 230, 20) }) \
    X(Spark,         { sf::Color(89, 35, 14), sf::Color(120, 50, 20) }) \
    X(ExplosionSpark,{ sf::Color(255, 165, 0), sf::Color(255, 140, 0) }) \
    X(SlimeMold,     { sf::Color(201, 58, 107), sf::Color(180, 50, 90) }) \
    X(Brick,         { sf::Color(188, 3, 0), sf::Color(160, 10, 10) }) \
    X(ExplosiveContainer,         { sf::Color(0, 0, 0), sf::Color(160, 10, 10) }) \
    X(Explosion,     { sf::Color(255, 69, 0), sf::Color(255, 140, 0) }) \

enum class MaterialGroup { MovableSolid, ImmovableSolid, Liquid, Gas, Special };

// Enum Generation
// We use ... (variadic args) to ignore the color list when generating IDs
enum class MaterialID : uint8_t {
    #define X(name, ...) name,
    PARTICLE_DATA(X)
    #undef X
};

struct MaterialProps {
    MaterialID id;
    std::string name;
    std::vector<sf::Color> palette; // CHANGED: Now a vector
    MaterialGroup group;
    std::function<std::unique_ptr<Particle>()> create;
};

extern const std::vector<MaterialProps> ALL_MATERIALS;

inline const MaterialProps& GetProps(MaterialID id) {
    for (const auto& p : ALL_MATERIALS) if (p.id == id) return p;
    return ALL_MATERIALS.back(); 
}