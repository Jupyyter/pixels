#pragma once
#include <SFML/Graphics.hpp>
#include "Constants.hpp"
#include "RigidBody.hpp" 

class ParticleWorld;

// Added 'Weapon' to Spawn Modes
enum class SpawnMode { Particles, RigidBody, Entity, Weapon };
enum class EntityType { Player };

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
    RigidBodyShape getRigidBodyShape() const { return currentShape; }
    
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
    RigidBodyShape currentShape = RigidBodyShape::Box;
    
    void drawMaterialTabs();
};