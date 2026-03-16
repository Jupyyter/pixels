#pragma once
#include "Particles/Particle.hpp"
#include "Constants.hpp"
#include <SFML/System/Vector2.hpp>

class ParticleWorld; 

class ExplosiveContainer : public Particle {
public:
    ExplosiveContainer() : Particle(MaterialID::ExplosiveContainer) {}

    // Setup basic projectile components
    void onSpawn(uint32_t index, int x, int y, ParticleWorld& world) override;
    
    // Custom Spawn Helper to configure the projectile payload
    static void spawnWithPayload(int x, int y, MaterialID payload, sf::Vector2f velocity, sf::Color color, bool ignited, ParticleWorld& world);

    MaterialGroup getGroup() const override { return MaterialGroup::Gas; } // Gas group allows it to pass through other fluids easily

    void update(const ParticleContext& ctx, float dt, ParticleWorld& world);

private:
    void detonate(uint32_t index, int x, int y, ParticleWorld& world);
};