#pragma once
#include <vector>
#include <cstdint>
#include <SFML/System/Vector2.hpp>

class ParticleWorld; // Forward declaration

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
    void applyDarken(int x, int y, float factor);
    void particalize(int x, int y, sf::Vector2f velocity);
};