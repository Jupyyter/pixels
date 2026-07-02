#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include <chrono>
#include <vector>

#include "Constants.hpp"
#include "ParticleWorld.hpp"
#include "Renderer.hpp"
#include "UI.hpp"
#include "LevelMenu.hpp"
#include "Entities/EntitySystem.hpp"

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
    void handleZoom(float delta, const sf::Vector2i& mousePos);
    void handleResize(unsigned int width, unsigned int height);
    void update();
    void render();
    void constrainView();
    void startGame(const std::string& worldFile);
    void returnToMenu();

    void handleMouseHeld();
    void addParticles(const sf::Vector2f& worldPos);
    void eraseParticles(const sf::Vector2f& worldPos);
    void addParticlesLine(const sf::Vector2f& start, const sf::Vector2f& end);
    void eraseParticlesLine(const sf::Vector2f& start, const sf::Vector2f& end);
    bool isMouseOverUI();

    sf::RenderWindow window;
    sf::View gameView;
    sf::Clock clock;
    sf::Clock frameClock;
    
    std::unique_ptr<ParticleWorld> world;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<UI> ui;
    std::unique_ptr<LevelMenu> levelMenu;
    std::unique_ptr<EntitySystem> entitySystem;
    
    GameState currentState;
    bool running;
    bool simulationRunning;
    bool isUIVisible = true; 
    float frameTime;

    sf::Vector2i lastMousePos;
    bool isPanning;
    float currentZoom;
    
    // Properties for continuous "freehand" brushing
    std::vector<sf::Vector2f> currentStroke;
    bool isBrushing = false;
    bool isErasing = false;

    // Properties for click-and-drag "line mode" drawing
    bool isDraggingLine = false;
    bool isDraggingEraseLine = false;
    sf::Vector2f lineStartPos;
    sf::Vector2f lineCurrentPos;
};