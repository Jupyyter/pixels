#pragma once
#include "Particles/Particle.hpp"
#include "Constants.hpp"
#include <SFML/System/Vector2.hpp>

class ParticleWorld; 

class ExplosiveContainer : public Particle {
public:
    ExplosiveContainer() : Particle(MaterialID::ExplosiveContainer) {}

    // 1. Setup Data
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    
    // 2. Custom Spawn Helper (To set the payload)
    // We can't change the signature of onSpawn, so we need a helper or use the world map directly.
    static void spawnWithPayload(int x, int y, MaterialID payload, sf::Vector2f velocity, sf::Color color, bool ignited, ParticleWorld& world);

    MaterialGroup getGroup() const override { return MaterialGroup::Gas; }

    void update(int x, int y, uint32_t index, float dt, ParticleWorld& world) override;

private:
    void detonate(uint32_t index, int x, int y, ParticleWorld& world);
    void spawnPayload(uint32_t index, int x, int y, ParticleWorld& world);
};