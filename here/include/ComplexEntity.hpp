#pragma once
#include "Entity.hpp"

class ComplexEntity : public Entity {
public:
    ProceduralLeg legA;
    ProceduralLeg legB;
    
    float downhillOffset = 0.0f;
    int   steppingLeg  = -1;   
    float stepProgress = 0.0f; 
    bool  isStopping   = false;

    float stepLookahead  = 8.0f;  
    float stepArcHeight  = 5.0f;  
    float minStepRate    = 0.8f;  

    ComplexEntity(b2WorldId physWorld, EntitySystem* sys, float x, float y, const EntityDefinition& def, bool isPlayer);
    ~ComplexEntity() override;

    void updateAnimations(float dt, ParticleWorld& pw) override;
    void render(sf::RenderTarget& target) override;

    void toggleRagdoll(bool enable) override;
    bool checkSideSnag(float dir, b2BodyId sideBody) override;

    void save(std::ostream& out) const override;
    void load(std::istream& in) override;
    
    int getType() const override { return 0; }
};