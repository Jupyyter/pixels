#include "SandSim.hpp"
#include <cmath>
#include <algorithm>

namespace SandSim {

SandSimApp::SandSimApp() : running(true), simulationRunning(true), frameTime(0.0f), 
                          hasPreviousMousePos(false), currentState(GameState::MENU) {
    // Initialize window
    window.create(sf::VideoMode({static_cast<unsigned int>(WINDOW_WIDTH), static_cast<unsigned int>(WINDOW_HEIGHT)}), "Sand Simulation - SFML 3");
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(false);
    
    // Initialize level menu first
    levelMenu = std::make_unique<LevelMenu>();
    
    // Don't initialize world and UI yet - wait for level selection
    renderer = std::make_unique<Renderer>();
    
    // Initialize random seed
    Random::setSeed(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
}

void SandSimApp::run() {
    while (running && window.isOpen()) {
        handleEvents();
        update();
        render();
    }
}

void SandSimApp::handleEvents() {
    while (auto event = window.pollEvent()) {
        // SFML 3 uses direct access to event members
        if (event->is<sf::Event::Closed>()) {
            running = false;
        }
        
        if (currentState == GameState::MENU) {
            handleMenuEvents(*event);
        } else if (currentState == GameState::PLAYING) {
            handleGameEvents(*event);
        }
    }
    
    // Handle continuous input based on current state
    if (currentState == GameState::PLAYING) {
        // Handle continuous mouse input for game
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) || sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
            handleMouseHeld();
        }
    }
}

void SandSimApp::handleMenuEvents(const sf::Event& event) {
    // FIXED: Use level menu coordinate conversion instead of game world coordinates
    sf::Vector2f windowMousePos = sf::Vector2f(
        static_cast<float>(sf::Mouse::getPosition(window).x), 
        static_cast<float>(sf::Mouse::getPosition(window).y)
    );
    sf::Vector2f menuMousePos = levelMenu->windowToMenuCoords(windowMousePos, window);
    
    if (auto mouseEvent = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouseEvent->button == sf::Mouse::Button::Left) {
            if (levelMenu->handleClick(menuMousePos)) {
                // Level selected, switch to game
                std::string selectedFile = levelMenu->getSelectedLevelFile();
                if (!selectedFile.empty()) {
                    startGame(selectedFile);
                }
            }
        }
    }
    else if (event.is<sf::Event::MouseMoved>()) {
        bool pressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        levelMenu->handleMouseDrag(menuMousePos, pressed);
        levelMenu->update(menuMousePos);
    }
    else if (auto wheelEvent = event.getIf<sf::Event::MouseWheelScrolled>()) {
        levelMenu->handleMouseWheel(wheelEvent->delta);
    }
    else if (auto keyEvent = event.getIf<sf::Event::KeyPressed>()) {
        if (keyEvent->code == sf::Keyboard::Key::Escape) {
            running = false;
        }
    }
    // ADDED: Handle resize events in menu state too
    else if (auto resizeEvent = event.getIf<sf::Event::Resized>()) {
        handleResize(resizeEvent->size.x, resizeEvent->size.y);
    }
}

void SandSimApp::handleGameEvents(const sf::Event& event) {
    if (auto keyEvent = event.getIf<sf::Event::KeyPressed>()) {
        if (keyEvent->code == sf::Keyboard::Key::Escape) {
            // Return to menu
            returnToMenu();
            return;
        }
        handleKeyPress(keyEvent->code);
        if (ui) {
            ui->handleKeyPress(keyEvent->code);
        }
    }
    else if (auto wheelEvent = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (ui) {
            ui->handleMouseWheel(wheelEvent->delta);
        }
    }
    else if (auto mouseEvent = event.getIf<sf::Event::MouseButtonPressed>()) {
        handleMousePress(*mouseEvent);
    }
    else if (auto mouseEvent = event.getIf<sf::Event::MouseButtonReleased>()) {
        handleMouseRelease(*mouseEvent);
    }
    else if (auto resizeEvent = event.getIf<sf::Event::Resized>()) {
        handleResize(resizeEvent->size.x, resizeEvent->size.y);
    }
}

void SandSimApp::startGame(const std::string& worldFile) {
    // Initialize world with selected level
    world = std::make_unique<ParticleWorld>(TEXTURE_WIDTH, TEXTURE_HEIGHT, worldFile);
    ui = std::make_unique<UI>(world.get());
    currentState = GameState::PLAYING;
    
    std::cout << "Started game with level: " << worldFile << std::endl;
}

void SandSimApp::returnToMenu() {
    // Clean up game objects
    world.reset();
    ui.reset();
    
    // Reset level menu selection and refresh levels to show any newly saved worlds
    levelMenu->resetSelection();
    levelMenu->refreshLevels();  // This will reload the levels from the worlds directory
    currentState = GameState::MENU;
}

void SandSimApp::handleKeyPress(sf::Keyboard::Key key) {
    switch (key) {            
        case sf::Keyboard::Key::Space:
            simulationRunning = !simulationRunning;
            break;
            
        case sf::Keyboard::Key::R:
            if (world) {
                world->clear();
            }
            break;
            
        case sf::Keyboard::Key::B:
            renderer->setUsePostProcessing(!renderer->getUsePostProcessing());
            break;
            
        default:
            break;
    }
}

void SandSimApp::handleMousePress(const sf::Event::MouseButtonPressed& mouseButton) {
    sf::Vector2f worldPos = screenToWorldCoordinates(sf::Vector2f(static_cast<float>(mouseButton.position.x), static_cast<float>(mouseButton.position.y)));
    
    // Check if UI consumed the click first
    if (ui && ui->handleClick(worldPos)) {
        return; // UI handled it, don't spawn particles
    }
    
    // Check if mouse is over UI area (prevent spawning when over UI)
    if (isMouseOverUI(worldPos)) {
        return; // Don't spawn particles when over UI
    }
    
    // Handle world interaction only if not over UI
    if (mouseButton.button == sf::Mouse::Button::Left) {
        addParticles(worldPos);
        previousMouseWorldPos = worldPos;
        hasPreviousMousePos = true;
    } else if (mouseButton.button == sf::Mouse::Button::Right) {
        eraseParticles(worldPos);
        previousMouseWorldPos = worldPos;
        hasPreviousMousePos = true;
    }
}

void SandSimApp::handleMouseRelease(const sf::Event::MouseButtonReleased& mouseButton) {
    // Reset mouse tracking when button is released
    hasPreviousMousePos = false;
}

void SandSimApp::handleMouseHeld() {
    sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
    sf::Vector2f worldPos = screenToWorldCoordinates(sf::Vector2f(static_cast<float>(mousePixelPos.x), static_cast<float>(mousePixelPos.y)));
    
    // Check if mouse is over UI - if so, don't spawn/erase particles
    if (isMouseOverUI(worldPos)) {
        return;
    }
    
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        if (hasPreviousMousePos) {
            // Draw a line from previous position to current position
            addParticlesLine(previousMouseWorldPos, worldPos);
        } else {
            addParticles(worldPos);
        }
        previousMouseWorldPos = worldPos;
        hasPreviousMousePos = true;
    } else if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
        if (hasPreviousMousePos) {
            // Draw a line from previous position to current position
            eraseParticlesLine(previousMouseWorldPos, worldPos);
        } else {
            eraseParticles(worldPos);
        }
        previousMouseWorldPos = worldPos;
        hasPreviousMousePos = true;
    }
}

void SandSimApp::handleResize(unsigned int width, unsigned int height) {
    // Update view to maintain aspect ratio - SFML 3 constructor signature
    sf::FloatRect visibleArea({0.0f, 0.0f}, {static_cast<float>(width), static_cast<float>(height)});
    window.setView(sf::View(visibleArea));
}

sf::Vector2f SandSimApp::screenToWorldCoordinates(const sf::Vector2f& screenPos) {
    // Convert screen coordinates to world coordinates
    sf::Vector2u windowSize = window.getSize();
    
    // Calculate scale used by renderer
    float scaleX = static_cast<float>(windowSize.x) / TEXTURE_WIDTH;
    float scaleY = static_cast<float>(windowSize.y) / TEXTURE_HEIGHT;
    float scale = std::min(scaleX, scaleY);
    
    // Calculate offset used by renderer
    float offsetX = (windowSize.x - TEXTURE_WIDTH * scale) / 2.0f;
    float offsetY = (windowSize.y - TEXTURE_HEIGHT * scale) / 2.0f;
    
    // Convert to world coordinates
    float worldX = (screenPos.x - offsetX) / scale;
    float worldY = (screenPos.y - offsetY) / scale;
    
    return sf::Vector2f(worldX, worldY);
}

bool SandSimApp::isMouseOverUI(const sf::Vector2f& worldPos) {
    // Only check UI collision if we're in game and UI exists
    if (currentState != GameState::PLAYING || !ui) {
        return false;
    }
    
    // Only consider mouse over UI if the material panel is actually shown
    if (!ui->getShowMaterialPanel()) {
        return false;
    }

    // Calculate the correct bounding box of the material selection panel
    float panelLeft = static_cast<float>(TEXTURE_WIDTH - UI_PANEL_X_OFFSET);
    float panelRight = static_cast<float>(TEXTURE_WIDTH - UI_PANEL_X_OFFSET + UI_PANEL_BUTTON_SIZE);

    // The buttons start at UI_PANEL_BASE Y and each button is UI_PANEL_BUTTON_SIZE tall
    float panelTop = static_cast<float>(UI_PANEL_BASE);
    size_t numButtons = ui->getMaterialButtons().size();
    
    // Account for save button (it's wider and positioned below material buttons)
    float saveButtonBottom = static_cast<float>(UI_PANEL_BASE + numButtons * UI_PANEL_OFFSET + 20 + UI_PANEL_BUTTON_SIZE);
    float panelBottom = saveButtonBottom;

    // Add some padding to make it easier to interact with UI
    const float UI_PADDING = 5.0f;
    panelLeft -= UI_PADDING;
    panelRight += UI_PADDING;
    panelTop -= UI_PADDING;
    panelBottom += UI_PADDING;

    // Check if the world mouse position is within these calculated bounds
    bool overUI = worldPos.x >= panelLeft && worldPos.x <= panelRight &&
                 worldPos.y >= panelTop && worldPos.y <= panelBottom;

    return overUI;
}

void SandSimApp::addParticles(const sf::Vector2f& worldPos) {
    if (!world || !ui) return;
    
    int x = static_cast<int>(worldPos.x);
    int y = static_cast<int>(worldPos.y);
    
    if (x >= 0 && x < TEXTURE_WIDTH && y >= 0 && y < TEXTURE_HEIGHT) {
        world->addParticleCircle(x, y, ui->getSelectionRadius(), ui->getCurrentMaterialID());
    }
}

void SandSimApp::eraseParticles(const sf::Vector2f& worldPos) {
    if (!world || !ui) return;
    
    int x = static_cast<int>(worldPos.x);
    int y = static_cast<int>(worldPos.y);
    
    if (x >= 0 && x < TEXTURE_WIDTH && y >= 0 && y < TEXTURE_HEIGHT) {
        world->eraseCircle(x, y, ui->getSelectionRadius());
    }
}

void SandSimApp::addParticlesLine(const sf::Vector2f& startPos, const sf::Vector2f& endPos) {
    if (!ui) return;
    
    // Calculate the distance between start and end positions
    float dx = endPos.x - startPos.x;
    float dy = endPos.y - startPos.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // If the distance is very small, just add particles at the current position
    if (distance < 1.0f) {
        addParticles(endPos);
        return;
    }
    
    // Calculate the number of steps based on the distance and brush radius
    float stepSize = std::max(1.0f, ui->getSelectionRadius() * 0.5f);
    int steps = static_cast<int>(std::ceil(distance / stepSize));
    
    // Draw particles along the line
    for (int i = 0; i <= steps; ++i) {
        float t = (steps > 0) ? static_cast<float>(i) / static_cast<float>(steps) : 0.0f;
        sf::Vector2f currentPos(
            startPos.x + t * dx,
            startPos.y + t * dy
        );
        addParticles(currentPos);
    }
}

void SandSimApp::eraseParticlesLine(const sf::Vector2f& startPos, const sf::Vector2f& endPos) {
    if (!ui) return;
    
    // Calculate the distance between start and end positions
    float dx = endPos.x - startPos.x;
    float dy = endPos.y - startPos.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    // If the distance is very small, just erase particles at the current position
    if (distance < 1.0f) {
        eraseParticles(endPos);
        return;
    }
    
    // Calculate the number of steps based on the distance and brush radius
    float stepSize = std::max(1.0f, ui->getSelectionRadius() * 0.5f);
    int steps = static_cast<int>(std::ceil(distance / stepSize));
    
    // Erase particles along the line
    for (int i = 0; i <= steps; ++i) {
        float t = (steps > 0) ? static_cast<float>(i) / static_cast<float>(steps) : 0.0f;
        sf::Vector2f currentPos(
            startPos.x + t * dx,
            startPos.y + t * dy
        );
        eraseParticles(currentPos);
    }
}

void SandSimApp::update() {
    if (currentState == GameState::PLAYING) {
        sf::Time deltaTime = clock.restart();
        
        // Measure frame time
        frameTime = static_cast<float>(frameClock.restart().asMilliseconds());
        
        // Update simulation
        if (simulationRunning && world) {
            world->update(deltaTime.asSeconds());
        }
        
        // Update UI
        if (ui) {
            sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
            sf::Vector2f worldMousePos = screenToWorldCoordinates(sf::Vector2f(static_cast<float>(mousePixelPos.x), static_cast<float>(mousePixelPos.y)));
            ui->update(worldMousePos, frameTime, simulationRunning);
        }
    }
    // Menu doesn't need update in the main loop - it's handled in events
}

void SandSimApp::render() {
    // Clear window
    window.clear(sf::Color(20, 20, 20));
    
    if (currentState == GameState::MENU) {
        // Render menu
        levelMenu->render(window);
    } else if (currentState == GameState::PLAYING) {
        // Render game
        if (world && renderer) {
            renderer->render(window, *world);
        }
        
        if (ui) {
            ui->render(window);
        }
    }
    
    // Display
    window.display();
}

} // namespace SandSim