#pragma once
#include "particles/Particle.hpp"
#include "Constants.hpp"
#include <SFML/System/Vector2.hpp>

// Forward declaration is enough here
class ParticleWorld; 

class ExplosiveContainer : public Particle {
public:
    MaterialID containedElementType;

    ExplosiveContainer(MaterialID contained, sf::Vector2f initialVelocity);

    MaterialGroup getGroup() const override;
    std::unique_ptr<Particle> clone() const override;
    void update(int x, int y, float dt, ParticleWorld& world) override;

private:
    void detonate(int x, int y, ParticleWorld& world);
    void spawnPayload(int x, int y, ParticleWorld& world);
};