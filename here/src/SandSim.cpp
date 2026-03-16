#include "SandSim.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <imgui.h>
#include <imgui-SFML.h>

// Camera Constants
constexpr float CAMERA_MARGIN = 50.0f; 

SandSimApp::SandSimApp() : 
    running(true), simulationRunning(true), frameTime(0.0f), 
    hasPreviousMousePos(false), currentState(GameState::MENU),
    currentZoom(1.0f), isPanning(false)
{
    window.create(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Sand Simulation - SFML 3");
    
    // FIX 1: Never use both VSync and Framerate Limit. VSync is safer and smoother.
    window.setVerticalSyncEnabled(true); 

    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Failed to initialize ImGui-SFML" << std::endl;
    }
 
    levelMenu = std::make_unique<LevelMenu>();
    renderer = std::make_unique<Renderer>();
    
    gameView.setSize({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
    gameView.setCenter({static_cast<float>(WORLD_WIDTH) / 2.f, static_cast<float>(WORLD_HEIGHT) / 2.f});

    handleResize(window.getSize().x, window.getSize().y);

    Random::setSeed(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
}

SandSimApp::~SandSimApp() {
    ImGui::SFML::Shutdown();
}

void SandSimApp::run() {
    clock.restart();
    frameClock.restart();

    while (running && window.isOpen()) {
        handleEvents();

        if (currentState == GameState::PLAYING) {
            if (isPanning) {
                sf::Vector2i currentPos = sf::Mouse::getPosition(window);
                sf::Vector2f delta = sf::Vector2f(lastMousePos - currentPos);
                gameView.move(delta * currentZoom);
                lastMousePos = currentPos;
                constrainView();
            } else if (!isMouseOverUI()) {
                if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) || 
                    sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
                    handleMouseHeld();
                } else {
                    hasPreviousMousePos = false;
                }
            }
        }

        update();
        render();
    }
}

void SandSimApp::constrainView() {
    sf::Vector2f viewSize = gameView.getSize();
    sf::Vector2f center = gameView.getCenter();

    float maxAllowedWidth = static_cast<float>(WORLD_WIDTH) + (CAMERA_MARGIN * 2.0f);
    float maxAllowedHeight = static_cast<float>(WORLD_HEIGHT) + (CAMERA_MARGIN * 2.0f);

    if (viewSize.x > maxAllowedWidth || viewSize.y > maxAllowedHeight) {
        float ratio = viewSize.x / viewSize.y;
        if (maxAllowedWidth / ratio <= maxAllowedHeight) {
            viewSize = {maxAllowedWidth, maxAllowedWidth / ratio};
        } else {
            viewSize = {maxAllowedHeight * ratio, maxAllowedHeight};
        }
        gameView.setSize(viewSize);
        currentZoom = viewSize.x / static_cast<float>(WINDOW_WIDTH);
    }

    float limitLeft = -CAMERA_MARGIN;
    float limitRight = static_cast<float>(WORLD_WIDTH) + CAMERA_MARGIN;
    float limitTop = -CAMERA_MARGIN;
    float limitBottom = static_cast<float>(WORLD_HEIGHT) + CAMERA_MARGIN;

    float minCenterX = limitLeft + (viewSize.x / 2.f);
    float maxCenterX = limitRight - (viewSize.x / 2.f);
    float minCenterY = limitTop + (viewSize.y / 2.f);
    float maxCenterY = limitBottom - (viewSize.y / 2.f);

    center.x = std::clamp(center.x, minCenterX, maxCenterX);
    center.y = std::clamp(center.y, minCenterY, maxCenterY);

    gameView.setCenter(center);
}

void SandSimApp::handleEvents() {
    while (const std::optional event = window.pollEvent()) {
        // FIX 2: ImGui must process events regardless of GameState!
        ImGui::SFML::ProcessEvent(window, *event);

        if (event->is<sf::Event::Closed>()) {
            running = false;
        }

        if (const auto* scroll = event->getIf<sf::Event::MouseWheelScrolled>()) {
            if (currentState == GameState::PLAYING && !isMouseOverUI()) {
                handleZoom(scroll->delta, sf::Mouse::getPosition(window));
            }
        }

        if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mouseBtn->button == sf::Mouse::Button::Middle) {
                isPanning = true;
                lastMousePos = sf::Mouse::getPosition(window);
            }
        }
        if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonReleased>()) {
            if (mouseBtn->button == sf::Mouse::Button::Middle) {
                isPanning = false;
            }
        }

        if (currentState == GameState::MENU) {
            handleMenuEvents(*event);
        } else {
            handleGameEvents(*event);
        }
    }
}

void SandSimApp::handleMenuEvents(const sf::Event& event) {
    sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    if (const auto* mousePress = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mousePress->button == sf::Mouse::Button::Left) {
            if (levelMenu->handleClick(worldPos)) {
                std::string selected = levelMenu->getSelectedLevelFile();
                if (!selected.empty()) startGame(selected);
            }
        }
    }
    else if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()) levelMenu->handleMouseWheel(wheel->delta);
    else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) if (key->code == sf::Keyboard::Key::Escape) running = false;
    
    // FIX 3: Removed levelMenu->update(worldPos) from here. 
    // It should not run 100 times per frame just because the mouse moved!
}

void SandSimApp::handleGameEvents(const sf::Event& event) {
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::Escape) returnToMenu();
    }
    else if (const auto* resize = event.getIf<sf::Event::Resized>()) {
        handleResize(resize->size.x, resize->size.y);
    }
    // --- NEW: SINGLE CLICK RIGID BODY SPAWNING ---
    else if (const auto* mouseBtn = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouseBtn->button == sf::Mouse::Button::Left && !isMouseOverUI()) {
            if (ui && ui->isCurrentSelectionRigidBody() && world) {
                sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                sf::Vector2f worldPos = window.mapPixelToCoords(mousePos, gameView);
                
                // Spawn it! The diameter is Radius * 2
                world->addRigidBody(
                    static_cast<int>(worldPos.x), 
                    static_cast<int>(worldPos.y), 
                    ui->getSelectionRadius() * 2.0f, 
                    ui->getRigidBodyShape(), 
                    ui->getCurrentMaterialID()
                );
            }
        }
    }
}

void SandSimApp::handleZoom(float delta, const sf::Vector2i& mousePos) {
    sf::Vector2f worldBefore = window.mapPixelToCoords(mousePos, gameView);
    float factor = (delta > 0) ? 0.9f : 1.1f;
    
    gameView.zoom(factor);
    currentZoom *= factor;

    sf::Vector2f worldAfter = window.mapPixelToCoords(mousePos, gameView);
    gameView.move(worldBefore - worldAfter);
    
    constrainView();
}

void SandSimApp::handleResize(unsigned int width, unsigned int height) {
    float windowRatio = static_cast<float>(width) / height;
    float viewRatio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT; 
    sf::FloatRect viewport({0.f, 0.f}, {1.f, 1.f});
    
    if (windowRatio > viewRatio) {
        float p = viewRatio / windowRatio;
        viewport.position.x = (1.0f - p) / 2.0f;
        viewport.size.x = p;
    } else {
        float p = windowRatio / viewRatio;
        viewport.position.y = (1.0f - p) / 2.0f;
        viewport.size.y = p;
    }
    gameView.setViewport(viewport);
    constrainView();
}

void SandSimApp::update() {
    sf::Time dt = clock.restart();
    frameTime = static_cast<float>(frameClock.restart().asMilliseconds());

    // FIX 4: ImGui MUST update every frame, regardless of game state!
    ImGui::SFML::Update(window, dt);

    if (currentState == GameState::PLAYING) {
    if (ui) {
            ui->update(window, dt, simulationRunning, frameTime);
            
            // --- NEW: SYNC UI DEBUG STATE TO RENDERER ---
            if (renderer) {
                renderer->setShowChunkBounds(ui->getShowChunkBounds());
                renderer->setShowColliders(ui->getShowColliders());
            }
        }
    
    if (simulationRunning && world) {
        // --- NEW: Tell the world where the camera is! ---
        sf::Vector2f center = gameView.getCenter();
        sf::Vector2f size = gameView.getSize();
        world->updateCameraBounds(center.x, center.y, size.x, size.y);
        // ------------------------------------------------
        
        world->update(dt.asSeconds());
    }
}
    else if (currentState == GameState::MENU) {
        // FIX 5: Level Menu updates safely ONCE per frame
        sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        levelMenu->update(worldPos);
    }
}

void SandSimApp::render() {
    window.clear(sf::Color(0, 0, 0));
    
    if (currentState == GameState::MENU) {
        window.setView(window.getDefaultView());
        levelMenu->render(window);
    } 
    else if (currentState == GameState::PLAYING) {
        window.setView(gameView);

        sf::RectangleShape border;
        border.setSize({static_cast<float>(WORLD_WIDTH), static_cast<float>(WORLD_HEIGHT)});
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(sf::Color(160, 32, 240)); 
        border.setOutlineThickness(2.0f / currentZoom);
        window.draw(border);

        if (world && renderer) {
            // Because we changed how ParticleWorld handles the camera, 
            // we must sync the world's internal buffer to our camera position!

            
            renderer->render(window, *world);
        }
        
        if (!isMouseOverUI() && !isPanning) {
            sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), gameView);
            
            // Draw a Box brush if Box mode is active
            if (ui->isCurrentSelectionRigidBody() && ui->getRigidBodyShape() == RigidBodyShape::Box) {
                float diameter = ui->getSelectionRadius() * 2.0f;
                sf::RectangleShape brush({diameter, diameter});
                brush.setOrigin({ui->getSelectionRadius(), ui->getSelectionRadius()});
                brush.setPosition(worldPos);
                brush.setFillColor(sf::Color(255, 255, 255, 40));
                brush.setOutlineColor(sf::Color(255, 255, 255, 180));
                brush.setOutlineThickness(1.0f / currentZoom);
                window.draw(brush);
            } else {
                // Default Circle brush
                sf::CircleShape brush(ui->getSelectionRadius());
                brush.setOrigin({ui->getSelectionRadius(), ui->getSelectionRadius()});
                brush.setPosition(worldPos);
                brush.setFillColor(sf::Color(255, 255, 255, 40));
                brush.setOutlineColor(sf::Color(255, 255, 255, 180));
                brush.setOutlineThickness(1.0f / currentZoom);
                window.draw(brush);
            }
        }

        window.setView(window.getDefaultView());
        if (ui) ui->render(window); // Render any UI shapes built inside UI.cpp
    }
    
    // FIX 6: ImGui MUST render every frame!
    ImGui::SFML::Render(window);
    
    window.display();
}

void SandSimApp::startGame(const std::string& worldFile) {
    world = std::make_unique<ParticleWorld>(VIEW_WIDTH, VIEW_HEIGHT, worldFile);
    ui = std::make_unique<UI>(window, world.get());
    
    currentZoom = 1.0f;
    gameView.setSize({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
    gameView.setCenter({static_cast<float>(WORLD_WIDTH) / 2.f, static_cast<float>(WORLD_HEIGHT) / 2.f});
    
    currentState = GameState::PLAYING;
    handleResize(window.getSize().x, window.getSize().y);
}

void SandSimApp::returnToMenu() {
    world.reset();
    ui.reset();
    currentState = GameState::MENU;
}

void SandSimApp::handleMouseHeld() {
    
if (ui && ui->isCurrentSelectionRigidBody()) return;

    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    sf::Vector2f worldPos = window.mapPixelToCoords(mousePos, gameView);
    
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        if (hasPreviousMousePos) addParticlesLine(previousMouseWorldPos, worldPos);
        else addParticles(worldPos);
        previousMouseWorldPos = worldPos;
        hasPreviousMousePos = true;
    } 
    else if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
        if (hasPreviousMousePos) eraseParticlesLine(previousMouseWorldPos, worldPos);
        else eraseParticles(worldPos);
        previousMouseWorldPos = worldPos;
        hasPreviousMousePos = true;
    }
}

void SandSimApp::addParticles(const sf::Vector2f& worldPos) {
    if (world && ui) {
        MaterialID currentMat = ui->getCurrentMaterialID();
        if (currentMat == MaterialID::Explosion) {
            int radius = static_cast<int>(ui->getSelectionRadius());
            world->triggerExplosion(static_cast<int>(worldPos.x), static_cast<int>(worldPos.y), radius, 20);
        } else {
            world->addParticleCircle(worldPos.x, worldPos.y, ui->getSelectionRadius(), currentMat);
        }
    }
}

void SandSimApp::eraseParticles(const sf::Vector2f& worldPos) {
    if (world && ui) world->eraseCircle(worldPos.x, worldPos.y, ui->getSelectionRadius());
}

void SandSimApp::addParticlesLine(const sf::Vector2f& start, const sf::Vector2f& end) {
    MaterialID currentMat = ui->getCurrentMaterialID();
    if (currentMat == MaterialID::Explosion) {
         float dx = end.x - start.x, dy = end.y - start.y;
         float dist = std::sqrt(dx*dx + dy*dy);
         float stepSize = std::max(1.0f, ui->getSelectionRadius()); 
         int steps = static_cast<int>(std::ceil(dist / stepSize));
         for(int i=0; i<=steps; ++i) {
             float t = (steps > 0) ? (float)i/steps : 0.f;
             sf::Vector2f pos = {start.x + t*dx, start.y + t*dy};
             world->triggerExplosion((int)pos.x, (int)pos.y, (int)ui->getSelectionRadius(), 20);
         }
         return;
    }
    float dx = end.x - start.x, dy = end.y - start.y;
    float dist = std::sqrt(dx*dx + dy*dy);
    float stepSize = std::max(1.0f, ui->getSelectionRadius() * 0.5f);
    int steps = static_cast<int>(std::ceil(dist / stepSize));
    for(int i=0; i<=steps; ++i) {
        float t = (steps > 0) ? (float)i/steps : 0.f;
        addParticles({start.x + t*dx, start.y + t*dy});
    }
}

void SandSimApp::eraseParticlesLine(const sf::Vector2f& start, const sf::Vector2f& end) {
    float dx = end.x - start.x, dy = end.y - start.y;
    float dist = std::sqrt(dx*dx + dy*dy);
    float stepSize = std::max(1.0f, ui->getSelectionRadius() * 0.5f);
    int steps = static_cast<int>(std::ceil(dist / stepSize));
    for(int i=0; i<=steps; ++i) {
        float t = (steps > 0) ? (float)i/steps : 0.f;
        eraseParticles({start.x + t*dx, start.y + t*dy});
    }
}

bool SandSimApp::isMouseOverUI() {
    return (currentState == GameState::PLAYING) && ImGui::GetIO().WantCaptureMouse;
}