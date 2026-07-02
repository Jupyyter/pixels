#pragma once
#include "Particles/Particle.hpp"
#include "Constants.hpp"
#include <SFML/System/Vector2.hpp>

class ParticleWorld; 

class ExplosiveContainer : public Particle {
public:
    ExplosiveContainer(MaterialID id) : Particle(id) {}

    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    
    static void spawnWithPayload(int x, int y, MaterialID payload, sf::Vector2f velocity, sf::Color color, bool ignited, ParticleWorld& world);

    MaterialGroup getGroup() const override { return MaterialGroup::Gas; } 

    void update(const ParticleContext& ctx, float dt, ParticleWorld& world) override;

private:
    void detonate(int x, int y, ParticleWorld& world);
};