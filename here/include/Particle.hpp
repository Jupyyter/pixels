#pragma once
#include <SFML/Graphics.hpp>
#include "Constants.hpp"
#include "Random.hpp"

namespace SandSim {
    struct Particle {
        MaterialID id = MaterialID::Empty;
        float lifeTime = 0.0f;
        sf::Vector2f velocity{0.0f, 0.0f};
        sf::Color color = MAT_COL_EMPTY;
        bool hasBeenUpdatedThisFrame = false;
        
        // Factory methods for different particle types
        static Particle createEmpty() {
            return Particle{MaterialID::Empty, 0.0f, {0.0f, 0.0f}, MAT_COL_EMPTY, false};
        }
        
        static Particle createSand() {
            auto p = Particle{MaterialID::Sand, 0.0f, {0.0f, 0.0f}, MAT_COL_SAND, false};
            // Add slight color variation
            p.color.r += Random::randInt(-20, 20);
            p.color.g += Random::randInt(-20, 20);
            p.color.b += Random::randInt(-20, 20);
            return p;
        }
        
        static Particle createWater() {
            auto p = Particle{MaterialID::Water, 0.0f, {0.0f, 0.0f}, MAT_COL_WATER, false};
            p.color.b += Random::randInt(-30, 30);
            return p;
        }
        
        static Particle createSalt() {
            return Particle{MaterialID::Salt, 0.0f, {0.0f, 0.0f}, MAT_COL_SALT, false};
        }
        
        static Particle createWood() {
            auto p = Particle{MaterialID::Wood, 0.0f, {0.0f, 0.0f}, MAT_COL_WOOD, false};
            p.color.r += Random::randInt(-10, 10);
            p.color.g += Random::randInt(-10, 10);
            return p;
        }
        
        static Particle createFire() {
            auto p = Particle{MaterialID::Fire, 0.0f, {0.0f, 0.0f}, MAT_COL_FIRE, false};
            int colorVariant = Random::randInt(0, 3);
            switch (colorVariant) {
                case 0: p.color = sf::Color(255, 80, 20, 255); break;
                case 1: p.color = sf::Color(250, 150, 10, 255); break;
                case 2: p.color = sf::Color(200, 150, 0, 255); break;
                case 3: p.color = sf::Color(100, 50, 2, 255); break;
            }
            return p;
        }
        
        static Particle createSmoke() {
            return Particle{MaterialID::Smoke, 0.0f, {0.0f, 0.0f}, MAT_COL_SMOKE, false};
        }
        
        static Particle createEmber() {
            return Particle{MaterialID::Ember, 0.0f, {0.0f, 0.0f}, MAT_COL_EMBER, false};
        }
        
        static Particle createSteam() {
            return Particle{MaterialID::Steam, 0.0f, {0.0f, 0.0f}, MAT_COL_STEAM, false};
        }
        
        static Particle createGunpowder() {
            return Particle{MaterialID::Gunpowder, 0.0f, {0.0f, 0.0f}, MAT_COL_GUNPOWDER, false};
        }
        
        static Particle createOil() {
            return Particle{MaterialID::Oil, 0.0f, {0.0f, 0.0f}, MAT_COL_OIL, false};
        }
        
        static Particle createLava() {
            auto p = Particle{MaterialID::Lava, 0.0f, {0.0f, 0.0f}, MAT_COL_LAVA, false};
            p.color.r += Random::randInt(-30, 30);
            p.color.g += Random::randInt(-10, 10);
            return p;
        }
        
        static Particle createStone() {
            auto p = Particle{MaterialID::Stone, 0.0f, {0.0f, 0.0f}, MAT_COL_STONE, false};
            p.color.r += Random::randInt(-20, 20);
            p.color.g += Random::randInt(-20, 20);
            p.color.b += Random::randInt(-20, 20);
            return p;
        }
        
        static Particle createAcid() {
            auto p = Particle{MaterialID::Acid, 0.0f, {0.0f, 0.0f}, MAT_COL_ACID, false};
            p.color.g += Random::randInt(-30, 30);
            return p;
        }
    };
}