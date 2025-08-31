#include "SandSim.hpp"
#include <cmath>
#include <algorithm>

namespace SandSim {

SandSimApp::SandSimApp() : running(true), simulationRunning(true), frameTime(0.0f), hasPreviousMousePos(false) {
    // Initialize window
    window.create(sf::VideoMode({static_cast<unsigned int>(WINDOW_WIDTH), static_cast<unsigned int>(WINDOW_HEIGHT)}), "Sand Simulation - SFML 3");
    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(false);
    
    // Initialize components
    world = std::make_unique<ParticleWorld>(TEXTURE_WIDTH, TEXTURE_HEIGHT);
    renderer = std::make_unique<Renderer>();
    ui = std::make_unique<UI>();
    
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
        else if (auto keyEvent = event->getIf<sf::Event::KeyPressed>()) {
            handleKeyPress(keyEvent->code);
            ui->handleKeyPress(keyEvent->code);
        }
        else if (auto wheelEvent = event->getIf<sf::Event::MouseWheelScrolled>()) {
            ui->handleMouseWheel(wheelEvent->delta);
        }
        else if (auto mouseEvent = event->getIf<sf::Event::MouseButtonPressed>()) {
            handleMousePress(*mouseEvent);
        }
        else if (auto mouseEvent = event->getIf<sf::Event::MouseButtonReleased>()) {
            handleMouseRelease(*mouseEvent);
        }
        else if (auto resizeEvent = event->getIf<sf::Event::Resized>()) {
            handleResize(resizeEvent->size.x, resizeEvent->size.y);
        }
    }
    
    // Handle continuous mouse input
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) || sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
        handleMouseHeld();
    }
}

void SandSimApp::handleKeyPress(sf::Keyboard::Key key) {
    switch (key) {
        case sf::Keyboard::Key::Escape:
            running = false;
            break;
            
        case sf::Keyboard::Key::Space:
            simulationRunning = !simulationRunning;
            break;
            
        case sf::Keyboard::Key::R:
            world->clear();
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
    if (ui->handleClick(worldPos)) {
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
    // Only consider mouse over UI if the material panel is actually shown
    if (!ui->getShowMaterialPanel()) {
        return false;
    }

    // FIXED: Calculate the correct bounding box of the material selection panel
    // The buttons are positioned at (TEXTURE_WIDTH - UI_PANEL_X_OFFSET, UI_PANEL_BASE + i * UI_PANEL_OFFSET)
    // So the left edge is at (TEXTURE_WIDTH - UI_PANEL_X_OFFSET) and extends UI_PANEL_BUTTON_SIZE to the right
    
    float panelLeft = static_cast<float>(TEXTURE_WIDTH - UI_PANEL_X_OFFSET);
    float panelRight = static_cast<float>(TEXTURE_WIDTH - UI_PANEL_X_OFFSET + UI_PANEL_BUTTON_SIZE);

    // The buttons start at UI_PANEL_BASE Y and each button is UI_PANEL_BUTTON_SIZE tall
    // with UI_PANEL_OFFSET spacing between them
    float panelTop = static_cast<float>(UI_PANEL_BASE);
    size_t numButtons = ui->getMaterialButtons().size();
    float panelBottom = static_cast<float>(UI_PANEL_BASE + (numButtons - 1) * UI_PANEL_OFFSET + UI_PANEL_BUTTON_SIZE);

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
    int x = static_cast<int>(worldPos.x);
    int y = static_cast<int>(worldPos.y);
    
    if (x >= 0 && x < TEXTURE_WIDTH && y >= 0 && y < TEXTURE_HEIGHT) {
        world->addParticleCircle(x, y, ui->getSelectionRadius(), ui->getCurrentMaterialID());
    }
}

void SandSimApp::eraseParticles(const sf::Vector2f& worldPos) {
    int x = static_cast<int>(worldPos.x);
    int y = static_cast<int>(worldPos.y);
    
    if (x >= 0 && x < TEXTURE_WIDTH && y >= 0 && y < TEXTURE_HEIGHT) {
        world->eraseCircle(x, y, ui->getSelectionRadius());
    }
}

void SandSimApp::addParticlesLine(const sf::Vector2f& startPos, const sf::Vector2f& endPos) {
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
    // Use smaller steps for smoother lines, but ensure we have at least a few steps
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
    // Use smaller steps for smoother lines, but ensure we have at least a few steps
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
    sf::Time deltaTime = clock.restart();
    
    // Measure frame time
    frameTime = static_cast<float>(frameClock.restart().asMilliseconds());
    
    // Update simulation
    if (simulationRunning) {
        world->update(deltaTime.asSeconds());
    }
    
    // Update UI
    sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
    sf::Vector2f worldMousePos = screenToWorldCoordinates(sf::Vector2f(static_cast<float>(mousePixelPos.x), static_cast<float>(mousePixelPos.y)));
    ui->update(worldMousePos, frameTime, simulationRunning);
}

void SandSimApp::render() {
    // Clear window
    window.clear(sf::Color(20, 20, 20));
    
    // Render particle world
    renderer->render(window, *world);
    
    // Render UI
    ui->render(window);
    
    // Display
    window.display();
}

} // namespace SandSim