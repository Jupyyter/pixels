#pragma once

namespace SandSim {
    // Window settings
    constexpr unsigned int WINDOW_WIDTH = 800;
    constexpr unsigned int WINDOW_HEIGHT = 600;
    constexpr unsigned int TEXTURE_WIDTH = 629;  // 1258 >> 1
    constexpr unsigned int TEXTURE_HEIGHT = 424; // 848 >> 1
    
    // Physics settings
    constexpr float GRAVITY = 10.0f;
    constexpr float DEFAULT_SELECTION_RADIUS = 10.0f;
    constexpr float MIN_SELECTION_RADIUS = 1.0f;
    constexpr float MAX_SELECTION_RADIUS = 100.0f;
    
    // Material IDs
    enum class MaterialID : uint8_t {
        Empty = 0,
        Sand = 1,
        Water = 2,
        Salt = 3,
        Wood = 4,
        Fire = 5,
        Smoke = 6,
        Ember = 7,
        Steam = 8,
        Gunpowder = 9,
        Oil = 10,
        Lava = 11,
        Stone = 12,
        Acid = 13
    };
    
    // Material colors
    constexpr sf::Color MAT_COL_EMPTY(0, 0, 0, 0);
    constexpr sf::Color MAT_COL_SAND(150, 100, 50, 255);
    constexpr sf::Color MAT_COL_WATER(20, 100, 170, 200);
    constexpr sf::Color MAT_COL_SALT(200, 180, 190, 255);
    constexpr sf::Color MAT_COL_WOOD(60, 40, 20, 255);
    constexpr sf::Color MAT_COL_FIRE(150, 20, 0, 255);
    constexpr sf::Color MAT_COL_SMOKE(50, 50, 50, 255);
    constexpr sf::Color MAT_COL_EMBER(200, 120, 20, 255);
    constexpr sf::Color MAT_COL_STEAM(220, 220, 250, 255);
    constexpr sf::Color MAT_COL_GUNPOWDER(60, 60, 60, 255);
    constexpr sf::Color MAT_COL_OIL(80, 70, 60, 255);
    constexpr sf::Color MAT_COL_LAVA(200, 50, 0, 255);
    constexpr sf::Color MAT_COL_STONE(120, 110, 120, 255);
    constexpr sf::Color MAT_COL_ACID(90, 200, 60, 255);
    
    // UI settings
    constexpr int UI_PANEL_OFFSET = 12;
    constexpr int UI_PANEL_X_OFFSET = 20;
    constexpr int UI_PANEL_BASE = 10;
    constexpr int UI_PANEL_BUTTON_SIZE = 10;
}