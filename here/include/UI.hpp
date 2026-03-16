#pragma once
#include <SFML/Graphics.hpp>
#include "Constants.hpp"
#include "RigidBody.hpp" // <-- Add this to access RigidBodyShape

class ParticleWorld;

class UI {
public:
    UI(sf::RenderWindow& window, ParticleWorld* worldPtr);
    
    void update(sf::RenderWindow& window, sf::Time deltaTime, bool& simRunning, float frameTime);
    void render(sf::RenderWindow& window);

    // Getters for SandSimApp
    MaterialID getCurrentMaterialID() const { return currentMaterial; }
    float getSelectionRadius() const { return selectionRadius; }
    bool isMouseOverUI() const; 

    // --- NEW RIGID BODY GETTERS ---
    bool isCurrentSelectionRigidBody() const { return spawnAsRigidBody; } 
    RigidBodyShape getRigidBodyShape() const { return currentShape; }
bool getShowChunkBounds() const { return showChunkBounds; }
    bool getShowColliders() const { return showColliders; }
private:
bool showChunkBounds = false;
    bool showColliders = false;
    ParticleWorld* world;
    MaterialID currentMaterial = MaterialID::Sand;
    float selectionRadius = DEFAULT_SELECTION_RADIUS;
    
    // --- NEW RIGID BODY STATE ---
    bool spawnAsRigidBody = false;
    RigidBodyShape currentShape = RigidBodyShape::Box;
    
    // Internal helper to draw material tabs
    void drawMaterialTabs();
};