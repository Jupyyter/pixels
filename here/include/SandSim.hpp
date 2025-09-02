#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <chrono>
#include "ParticleWorld.hpp"
#include "Renderer.hpp"
#include "UI.hpp"
#include "Constants.hpp"
#include "Random.hpp"
#include "GameState.hpp"
#include "LevelMenu.hpp"
namespace SandSim {
    class SandSimApp {
    private:
    GameState currentState;
    std::unique_ptr<LevelMenu> levelMenu;
        sf::RenderWindow window;
        std::unique_ptr<ParticleWorld> world;
        std::unique_ptr<Renderer> renderer;
        std::unique_ptr<UI> ui;
        
        sf::Clock clock;
        sf::Clock frameClock;
        
        bool running;
        bool simulationRunning;
        float frameTime;
        
        // Mouse tracking for continuous drawing
        sf::Vector2f previousMouseWorldPos;
        bool hasPreviousMousePos;
        
    public:
        SandSimApp();
        void run();
        
    private:
        // Event handling
        void handleEvents();
        void handleKeyPress(sf::Keyboard::Key key);
        void handleMousePress(const sf::Event::MouseButtonPressed& mouseButton);
        void handleMouseRelease(const sf::Event::MouseButtonReleased& mouseButton);
        void handleMouseHeld();
        void handleResize(unsigned int width, unsigned int height);
        void handleMenuEvents(const sf::Event& event);
        void handleGameEvents(const sf::Event& event);
        void returnToMenu();
        void startGame(const std::string& worldFile);
        
        // Coordinate conversion
        sf::Vector2f screenToWorldCoordinates(const sf::Vector2f& screenPos);
        
        // UI interaction
        bool isMouseOverUI(const sf::Vector2f& worldPos);
        
        // Particle manipulation
        void addParticles(const sf::Vector2f& worldPos);
        void eraseParticles(const sf::Vector2f& worldPos);
        void addParticlesLine(const sf::Vector2f& startPos, const sf::Vector2f& endPos);
        void eraseParticlesLine(const sf::Vector2f& startPos, const sf::Vector2f& endPos);
        
        // Game loop
        void update();
        void render();
    };
}