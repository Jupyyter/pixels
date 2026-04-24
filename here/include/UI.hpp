#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "Constants.hpp"

class ParticleWorld;

// Spawn Modes
enum class SpawnMode { Particles, Image, Entity, Weapon };
enum class EntityType { Player };

// Brush Shapes
enum class BrushShape { Circle, Square };

// Struct to hold loaded image assets (Used for Rigid Bodies, Structures, and Weapons)
struct ImageAsset {
    std::string name;
    std::string path;
    sf::Texture texture;
    sf::Image image;
};

class UI {
public:
    UI(sf::RenderWindow& window, ParticleWorld* worldPtr);
    
    void update(sf::RenderWindow& window, sf::Time deltaTime, bool& simRunning, float frameTime, sf::Vector2f mouseWorldPos);
    void render(sf::RenderWindow& window);

    MaterialID getCurrentMaterialID() const { return currentMaterial; }
    float getSelectionRadius() const { return selectionRadius; }
    bool isMouseOverUI() const; 

    SpawnMode getSpawnMode() const { return spawnMode; }
    EntityType getSelectedEntity() const { return currentEntity; }
    BrushShape getBrushShape() const { return brushShape; }
    bool getUseLineMode() const { return useLineMode; }
    
    // Unified Asset Getters
    float getAssetScale() const { return assetScale; }
    const sf::Image* getSelectedAssetImage() const;
    const sf::Texture* getSelectedAssetTexture() const;
    
    // Weapons Getters
    const sf::Image* getSelectedWeaponImage() const;
    const sf::Texture* getSelectedWeaponTexture() const;
    std::string getSelectedWeaponName() const;

    // Spawning properties
    bool getSpawnAsRigidBody() const { return spawnAsRigidBody; }
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
    BrushShape brushShape = BrushShape::Circle;
    bool useLineMode = false;
    
    // Unified Assets list (Structures + Rigid Bodies)
    std::vector<ImageAsset> imageAssets;
    int selectedAssetIndex = 0;
    float assetScale = 1.0f;
    bool spawnAsRigidBody = false;
    bool glueToTerrain = false;

    // Weapons Assets
    std::vector<ImageAsset> weaponAssets;
    int selectedWeaponIndex = 0;

    void loadImageAssets();
    void loadWeaponAssets();
    void drawMaterialTabs();
};