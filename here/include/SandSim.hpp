#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <string>
#include "Constants.hpp"
#include "ParticleWorld.hpp"
#include "Renderer.hpp"
#include "UI.hpp"
#include "LevelMenu.hpp"
#include "Random.hpp"
#include "EntitySystem.hpp" // NEW

enum class GameState { MENU, PLAYING };

class SandSimApp {
public:
    SandSimApp();
    ~SandSimApp();
    void run();

private:
    void handleEvents();
    void handleMenuEvents(const sf::Event& event);
    void handleGameEvents(const sf::Event& event);
    void handleMouseHeld();
    void update();
    void render();

    // Camera/View logic
    void handleZoom(float delta, const sf::Vector2i& mousePos);
    void handleResize(unsigned int width, unsigned int height);
    void constrainView();

    // Simulation logic
    void startGame(const std::string& worldFile);
    void returnToMenu();
    void addParticles(const sf::Vector2f& worldPos);
    void addParticlesLine(const sf::Vector2f& start, const sf::Vector2f& end);
    void eraseParticles(const sf::Vector2f& worldPos);
    void eraseParticlesLine(const sf::Vector2f& start, const sf::Vector2f& end);
    
    sf::Vector2f screenToWorldCoordinates(const sf::Vector2f& screenPos);
    bool isMouseOverUI();

    sf::RenderWindow window;
    bool running;
    bool simulationRunning;
    GameState currentState;

    sf::Clock clock;
    sf::Clock frameClock;
    float frameTime;

    // Camera state
    sf::View gameView;
    float currentZoom;
    bool isPanning;
    sf::Vector2i lastMousePos;

    // Spawning state
    sf::Vector2f previousMouseWorldPos;
    bool hasPreviousMousePos;

    std::unique_ptr<ParticleWorld> world;
    std::unique_ptr<EntitySystem> entitySystem; // NEW
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<UI> ui;
    std::unique_ptr<LevelMenu> levelMenu;
};