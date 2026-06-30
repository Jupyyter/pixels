// text/plain
// simpleentity.hpp
#pragma once
#include "Entity.hpp"
#include <vector>
#include <SFML/System/Vector2.hpp>

class SimpleEntity : public Entity {
public:
    // Store actual collider dimensions for easy access
    float colWidth;
    float colHeight;

    float maxStepUp;
    std::vector<sf::Vector2f> boundarySamples;

    SimpleEntity(b2WorldId physWorld, EntitySystem* sys, float x, float y, const EntityDefinition& def, bool isPlayer);
    ~SimpleEntity() override;

    void updateAnimations(float dt, ParticleWorld& pw) override;
    void render(sf::RenderTarget& target) override;
    
    void toggleRagdoll(bool enable) override;

    void save(std::ostream& out) const override;
    void load(std::istream& in) override;
    
    int getType() const override { return 1; }
};