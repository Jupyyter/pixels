#pragma once
#include <box2d/box2d.h>
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <optional>

class RigidBody; // Forward declare for weapon attachment

// Core Physics wrapper
struct PhysicsComponent {
    b2BodyId bodyId;
};

// Player Input & Controller State
struct PlayerControllerComponent {
    float moveSpeed = 5.0f;
    float jumpForce = -38.0f;
    bool isGrounded = false;
    
    bool ePressedLastFrame = false;
    RigidBody* equippedWeapon = nullptr; // Pointer managed safely by RigidBodySystem
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

// ---------------------------------------------------------------------------
// PROCEDURAL LEG & HAND
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Body-bob state
// ---------------------------------------------------------------------------
struct BodyBobState {
    float offsetY   = 0.0f;
    float velocity  = 0.0f;
    float stiffness = 60.0f;
    float damping   = 12.0f;
};

// ---------------------------------------------------------------------------
// Full procedural animation component
// ---------------------------------------------------------------------------
struct ProceduralAnimationComponent {
    ProceduralLeg  legA;
    ProceduralLeg  legB;
    ProceduralHand handA; // Front Hand (Direction of gaze)
    ProceduralHand handB; // Back Hand  (Trailing)
    BodyBobState   bob;

    int   steppingLeg  = -1;   
    float stepProgress = 0.0f; 
    bool  isStopping   = false;

    float armSwing    = 0.0f;
    float weaponAngle = 90.0f; // Calculated dynamically via parabola derivative

    float strideDistance = 12.0f; 
    float stepLookahead  = 8.0f;  
    float stepArcHeight  = 5.0f;  
    float minStepRate    = 0.8f;  
};