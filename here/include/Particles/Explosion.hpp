#pragma once
#include <vector>
#include <cstdint>
#include <SFML/System/Vector2.hpp>

class ParticleWorld;
struct BaseComponent; // Forward declaration for the component pointer

class Explosion {
private:
    ParticleWorld& world;
    int centerX, centerY;
    int radius;
    int strength;

    enum CacheState : uint8_t {
        UNVISITED = 0,
        PROCESSED_UNSTOPPED = 1,
        PROCESSED_STOPPED = 2
    };

public:
    Explosion(ParticleWorld& worldRef, int x, int y, int r, int s);
    void enact();

private:
    void castRay(int destX, int destY, std::vector<uint8_t>& cache, int boxSize);
    
    // Updated to take direct BaseComponent pointer to avoid redundant lookups
    void applyDarken(int x, int y, BaseComponent* base, float factor);
    
    // Updated to take direct BaseComponent pointer to avoid redundant lookups
    void particalize(int x, int y, BaseComponent* base, sf::Vector2f velocity);
};