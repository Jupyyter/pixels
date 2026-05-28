#pragma once
#include <box2d/box2d.h>
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

class RigidBody;

struct AINode {
    sf::Vector2f pos;
    std::vector<int> neighbors;
};

struct PathNodeData {
    sf::Vector2f pos;
    bool isJump = false;
    bool isFall = false;
    bool isJumpTakeoff = false;
    float requiredVx = 0.0f;
    
};

struct PhysicsComponent {
    b2BodyId bodyId;
};

struct PlayerControllerComponent {
    bool isPlayer = true;
    
    bool hasTarget = false;
    sf::Vector2f targetPos;
    std::vector<PathNodeData> path;
    int pathIndex = 0;

    float pathRecalcTimer = 0.0f;
    float stuckTimer = 0.0f;
    sf::Vector2f lastPos = {0.0f, 0.0f};

    sf::Vector2f homePos = {0.0f, 0.0f};
    bool homePosSet = false;
    float idleWaitTimer = 0.0f;
    bool isWandering = false;
    float wanderDir = 0.0f;
    float wanderTimer = 0.0f;
    float physicsStuckTimer = 0.0f;

    float moveSpeed = 7.0f;
    float jumpForce = -38.0f;
    bool isGrounded = false;
    
    bool ePressedLastFrame = false;
    bool wPressedLastFrame = false;
    bool leftClickPressedLastFrame = false;
    
    bool fPressedLastFrame = false;
    bool isRagdoll = false;
    float uprightStunTimer = 0.0f;
    RigidBody* equippedWeapon = nullptr; 

    bool isSwinging = false;
    float swingTimer = 0.0f;
    float swingDuration = 0.25f;
    bool swingEffectApplied = false;
    sf::Vector2f swingTarget;
    float swingRandomness = 0.0f;

    bool isAiming = false;
    sf::Vector2f aimTarget;
    float fireTimer = 0.0f;

    float lastFallVelocity = 0.0f;
    float landingTimer = 0.0f;
};

struct AnimationState {
    int startFrame;
    int frameCount;
    float frameDuration;
};

struct SpriteSheetComponent {
    std::shared_ptr<sf::Texture> texture;
    std::optional<sf::Sprite> sprite;
    std::string texturePath = ""; // Essential logic anchor to restore textures accurately on loaded files dynamically globally! 

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
    b2BodyId ragdollBodyId = b2_nullBodyId;
};

struct ProceduralHand {
    sf::Vector2f offset; 
    sf::Vector2f recoilPos = {0.0f, 0.0f};
    sf::Vector2f recoilVel = {0.0f, 0.0f};
    float recoilAngle = 0.0f;
    float recoilAngularVel = 0.0f;
    b2BodyId ragdollBodyId = b2_nullBodyId;
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
    
    float recoilAngle = 0.0f;
    float recoilAngleVel = 0.0f;
    sf::Vector2f recoilOffset = {0.0f, 0.0f};
    sf::Vector2f recoilVelocity = {0.0f, 0.0f};
};