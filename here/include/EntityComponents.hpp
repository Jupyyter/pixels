#pragma once
#include <box2d/box2d.h>
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <optional>

class RigidBody;

// Core Physics wrapper
struct PhysicsComponent {
    b2BodyId bodyId;
};

// Player Input & Controller State
struct PlayerControllerComponent {
    float moveSpeed = 7.0f;
    float jumpForce = -38.0f;
    bool isGrounded = false;
    
    bool ePressedLastFrame = false;
    bool wPressedLastFrame = false;
    
    RigidBody* equippedWeapon = nullptr; 

    // Swing mechanics
    bool isSwinging = false;
    float swingTimer = 0.0f;
    float swingDuration = 0.25f;
    bool swingEffectApplied = false;
    sf::Vector2f swingTarget;
    float swingRandomness = 0.0f;

    // Landing recovery
    float lastFallVelocity = 0.0f;
    float landingTimer = 0.0f;
};

// --- SPRITE SHEET ANIMATION ---
struct AnimationState {
    int startFrame;
    int frameCount;
    float frameDuration;
};

struct SpriteSheetComponent {
    std::shared_ptr<sf::Texture> texture;
    std::optional<sf::Sprite> sprite;

    int frameWidth  = 32;
    int frameHeight = 32;

    std::string currentState = "Idle";
    std::unordered_map<std::string, AnimationState> animations;

    int   currentFrameIndex = 0;
    float frameTimer        = 0.0f;
    bool  flipX             = false;
};

struct ProceduralLeg {
    sf::Vector2f hipOffset;
    sf::Vector2f footWorld;
    sf::Vector2f footStart;
    sf::Vector2f footTarget;
    float plantedX = 0.0f;
    float plantedY = 0.0f;
    bool  isPlanted = false;
};

struct ProceduralHand {
    sf::Vector2f offset; 
};

struct BodyBobState {
    float offsetY   = 0.0f;
    float velocity  = 0.0f;
    float stiffness = 60.0f;
    float damping   = 12.0f;
};

struct ProceduralAnimationComponent {
    ProceduralLeg  legA;
    ProceduralLeg  legB;
    ProceduralHand handA; 
    ProceduralHand handB; 
    BodyBobState   bob;

    int   steppingLeg  = -1;   
    float stepProgress = 0.0f; 
    bool  isStopping   = false;

    float armSwing    = 0.0f;
    float weaponAngle = 90.0f; 

    float strideDistance = 12.0f; 
    float stepLookahead  = 8.0f;  
    float stepArcHeight  = 5.0f;  
    float minStepRate    = 0.8f;  
    
    float downhillOffset = 0.0f;
    
};