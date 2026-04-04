#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "Constants.hpp"

class ParticleWorld;

// Spawn Modes
enum class SpawnMode { Particles, RigidBody, Entity, Weapon };
enum class EntityType { Player };

// Struct to hold loaded rigid body assets
struct RigidBodyAsset {
    std::string name;
    std::string path;
    sf::Texture texture;
    sf::Image image;
};

class UI {
public:
    UI(sf::RenderWindow& window, ParticleWorld* worldPtr);
    
    void update(sf::RenderWindow& window, sf::Time deltaTime, bool& simRunning, float frameTime);
    void render(sf::RenderWindow& window);

    MaterialID getCurrentMaterialID() const { return currentMaterial; }
    float getSelectionRadius() const { return selectionRadius; }
    bool isMouseOverUI() const; 

    SpawnMode getSpawnMode() const { return spawnMode; }
    EntityType getSelectedEntity() const { return currentEntity; }
    
    // --- NEW: Rigid Body Getters ---
    float getRigidBodyScale() const { return rigidBodyScale; }
    const sf::Image* getSelectedRigidBodyImage() const;
    const sf::Texture* getSelectedRigidBodyTexture() const;
    bool getGlueToTerrain() const { return glueToTerrain; }
    
    bool getShowChunkBounds() const { return showChunkBounds; }
    bool getShowColliders() const { return showColliders; }

private:
    bool showChunkBounds = false;
    bool showColliders = false;
    ParticleWorld* world;
    MaterialID currentMaterial = MaterialID::Sand;
    float selectionRadius = DEFAULT_SELECTION_RADIUS;
    
    SpawnMode spawnMode = SpawnMode::Particles;
    EntityType currentEntity = EntityType::Player;
    
    // Rigid Body Asset Management
    std::vector<RigidBodyAsset> rigidBodyAssets;
    int selectedRigidBodyIndex = 0;
    float rigidBodyScale = 1.0f; // 1.0 = 100%
    bool glueToTerrain = false;
    
    void loadRigidBodyAssets();
    void drawMaterialTabs();
};