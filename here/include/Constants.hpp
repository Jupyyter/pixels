#pragma once
#include <SFML/Graphics/Color.hpp>
#include <string>
#include <vector>
#include <memory>

class Particle; 

// Window settings
constexpr unsigned int WORLD_WIDTH = 2560;
constexpr unsigned int WORLD_HEIGHT = 1440;

constexpr unsigned int WINDOW_WIDTH = 1280;
constexpr unsigned int WINDOW_HEIGHT = 720;

constexpr unsigned int VIEW_WIDTH = 1280; 
constexpr unsigned int VIEW_HEIGHT = 720;

// Chunk Settings
constexpr int CHUNK_SIZE = 64;
constexpr int CHUNK_AREA = CHUNK_SIZE * CHUNK_SIZE;

// --- CHUNK COORDINATES ---
struct ChunkCoord {
    int x, y;
    bool operator==(const ChunkCoord& other) const { 
        return x == other.x && y == other.y; 
    }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const {
        return std::hash<int>()(c.x) ^ (std::hash<int>()(c.y) << 1);
    }
};

// Physics settings
constexpr float GRAVITY = 800.0f;
constexpr float DEFAULT_SELECTION_RADIUS = 10.0f;
constexpr float MIN_SELECTION_RADIUS = 1.0f;
constexpr float MAX_SELECTION_RADIUS = 100.0f;

enum class MaterialGroup { MovableSolid, ImmovableSolid, Liquid, Gas, Special };

// We keep the hardcoded base IDs explicitly so your C++ code can still reference them directly
// AI-generated IDs will start at 150 at runtime.
using MaterialID = uint8_t;
constexpr MaterialID MAT_AIR = 0;