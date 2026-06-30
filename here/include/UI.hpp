#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "Constants.hpp"

class ParticleWorld;

enum class SpawnMode { Particles, Image, Entity, Weapon };
enum class BrushShape { Circle, Square, Platform };

struct ImageAsset {
    std::string name;
    std::string path;
    sf::Image image;
    sf::Texture texture;
};

class UI {
private:
    ParticleWorld* world;
    std::vector<ImageAsset> imageAssets;
    std::vector<ImageAsset> weaponAssets;
    std::vector<ImageAsset> entityAssets;
    
    int selectedAssetIndex = 0;
    int selectedWeaponIndex = 0;
    int selectedEntityIndex = 0;
    
    SpawnMode spawnMode = SpawnMode::Particles;
    BrushShape brushShape = BrushShape::Circle;
    
    float selectionRadius = 10.0f;
    float assetScale = 1.0f;
    bool spawnAsRigidBody = true;
    bool glueToTerrain = false;
    bool spawnAsPlayer = false;
    bool useLineMode = false;
    
    bool showChunkBounds = false;
    bool showColliders = false;
    
    MaterialID currentMaterial = MaterialID::Sand; 
    
    void drawMaterialTabs();
    void loadImageAssets();
    void loadWeaponAssets();
    void loadEntityAssets();
    
public:
    UI(sf::RenderWindow& window, ParticleWorld* worldPtr);
    
    void update(sf::RenderWindow& window, sf::Time deltaTime, bool& simRunning, float frameTime, sf::Vector2f mouseWorldPos);
    void render(sf::RenderWindow& window);
    
    bool isMouseOverUI() const;
    
    SpawnMode getSpawnMode() const { return spawnMode; }
    BrushShape getBrushShape() const { return brushShape; }
    float getSelectionRadius() const { return selectionRadius; }
    float getAssetScale() const { return assetScale; }
    bool getSpawnAsRigidBody() const { return spawnAsRigidBody; }
    bool getGlueToTerrain() const { return glueToTerrain; }
    bool getSpawnAsPlayer() const { return spawnAsPlayer; }
    bool getUseLineMode() const { return useLineMode; }
    bool getShowChunkBounds() const { return showChunkBounds; }
    bool getShowColliders() const { return showColliders; }
    MaterialID getCurrentMaterialID() const { return currentMaterial; }
    
    const sf::Image* getSelectedAssetImage() const;
    const sf::Texture* getSelectedAssetTexture() const;
    
    const sf::Image* getSelectedWeaponImage() const;
    const sf::Texture* getSelectedWeaponTexture() const;
    std::string getSelectedWeaponName() const;
    
    const sf::Image* getSelectedEntityImage() const;
    const sf::Texture* getSelectedEntityTexture() const;
    std::string getSelectedEntityName() const;
    std::string getSelectedEntityPath() const;
};