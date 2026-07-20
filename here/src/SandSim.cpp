#include "SandSim.hpp"
#include "RigidBody.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <imgui.h>
#include <imgui-SFML.h>
#include <vector>
#include "Particles/ParticleDef.hpp"

static bool g_isDraggingLine = false;
static bool g_isDraggingEraseLine = false;
static sf::Vector2f g_lineStartPos;
static sf::Vector2f g_lineCurrentPos;

constexpr float CAMERA_MARGIN = 50.0f; 



SandSimApp::SandSimApp() : 
    running(true), simulationRunning(true), frameTime(0.0f), 
    currentState(GameState::MENU),
    currentZoom(1.0f), isPanning(false)
{

    window.create(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Sand Simulation - SFML 3");
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

sf::Image scaleImageNearestNeighbor(const sf::Image& source, float scale) {
    if (scale == 1.0f) return source; 
    
    sf::Vector2u size = source.getSize();
    unsigned int newW = std::max(1u, static_cast<unsigned int>(size.x * scale));
    unsigned int newH = std::max(1u, static_cast<unsigned int>(size.y * scale));
    
    sf::Image result;
    result.resize(sf::Vector2u(newW, newH), sf::Color::Transparent);
    
    for(unsigned int y = 0; y < newH; ++y) {
        for(unsigned int x = 0; x < newW; ++x) {
            unsigned int srcX = std::min(static_cast<unsigned int>(x / scale), size.x - 1);
            unsigned int srcY = std::min(static_cast<unsigned int>(y / scale), size.y - 1);
            result.setPixel(sf::Vector2u(x, y), source.getPixel(sf::Vector2u(srcX, srcY)));
        }
    }
    return result;
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
}

void SandSimApp::handleGameEvents(const sf::Event& event) {
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (key->code == sf::Keyboard::Key::Escape) returnToMenu();
        if (key->code == sf::Keyboard::Key::Grave) {
            isUIVisible = !isUIVisible;
        }
        if (key->code == sf::Keyboard::Key::M) {
            world->generateWorldFromImage("input_image.png", "color_map.txt");
        }
    }
    else if (const auto* resize = event.getIf<sf::Event::Resized>()) {
        handleResize(resize->size.x, resize->size.y);
    }
    else if (const auto* mouseBtn = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouseBtn->button == sf::Mouse::Button::Left) {
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            sf::Vector2f worldPos = window.mapPixelToCoords(mousePos, gameView);

            if (isUIVisible) {
                if (!isMouseOverUI() && ui && world) {
                    if (ui->getSpawnMode() == SpawnMode::Image) {
                        const sf::Image* srcImg = ui->getSelectedAssetImage();
                        if (srcImg) {
                            float scale = ui->getAssetScale();
                            sf::Image scaled = scaleImageNearestNeighbor(*srcImg, scale);
                            int startX = static_cast<int>(worldPos.x) - (scaled.getSize().x / 2);
                            int startY = static_cast<int>(worldPos.y) - (scaled.getSize().y / 2);
                            
                            if (ui->getSpawnAsRigidBody()) world->addRigidBodyFromSprite(scaled, startX, startY, ui->getCurrentMaterialID(), ui->getGlueToTerrain());
                            else world->addStructureFromSprite(scaled, startX, startY, ui->getCurrentMaterialID());
                        }
                    }
                    else if (ui->getSpawnMode() == SpawnMode::Weapon) {
                        const sf::Image* srcImg = ui->getSelectedWeaponImage();
                        std::string wName = ui->getSelectedWeaponName();
                        if (srcImg) {
                            float wScale = 1.0f;
                            if (wName.find("Revolver") != std::string::npos || wName.find("Submachine") != std::string::npos) {
                                wScale = 0.25f;
                            }
                            sf::Image scaled = scaleImageNearestNeighbor(*srcImg, wScale);
                            int startX = static_cast<int>(worldPos.x) - (scaled.getSize().x / 2);
                            int startY = static_cast<int>(worldPos.y) - (scaled.getSize().y / 2);
                            world->addWeapon(scaled, startX, startY, wName);
                        }
                    }
                    else if (ui->getSpawnMode() == SpawnMode::Entity) {
                        if (entitySystem) {
                            std::string defName = ui->getSelectedEntityName();
                            if (!defName.empty()) {
                                sf::Vector2f spawnPos = worldPos;
                                
                                // Offset the spawn position by the collider's center
                                // so it perfectly matches the ghost preview origin
                                for (const auto& def : entitySystem->getDefinitions()) {
                                    if (def.name == defName) {
                                        spawnPos.x -= (def.colliderRect.position.x + def.colliderRect.size.x / 2.0f);
                                        spawnPos.y -= (def.colliderRect.position.y + def.colliderRect.size.y / 2.0f);
                                        break;
                                    }
                                }
                                
                                entitySystem->spawnEntity(spawnPos.x, spawnPos.y, defName, ui->getSpawnAsPlayer());
                            }
                        }
                    }
                }
            } 
            else {
                if (entitySystem) {
                    entitySystem->triggerSwing(worldPos);
                }
            }
        }
    }
}

void SandSimApp::run() {
    clock.restart();
    frameClock.restart();

    while (running && window.isOpen()) {
        handleEvents();

        if (currentState == GameState::PLAYING) {
            if (!window.hasFocus()) {
                isPanning = false; 
            }

            if (isPanning) {
                sf::Vector2i currentPos = sf::Mouse::getPosition(window);
                sf::Vector2f delta = sf::Vector2f(lastMousePos - currentPos);
                gameView.move(delta * currentZoom);
                lastMousePos = currentPos;
                constrainView();
            } else if (isUIVisible && !isMouseOverUI() && window.hasFocus()) {
                bool holdingButton = (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) || sf::Mouse::isButtonPressed(sf::Mouse::Button::Right));
                if (holdingButton) {
                    handleMouseHeld();
                } else {
                    auto processFinalizationStroke = [&]() {
                        if (!currentStroke.empty() && world) {
                            float minX = currentStroke[0].x, maxX = currentStroke[0].x;
                            float minY = currentStroke[0].y, maxY = currentStroke[0].y;
                            for (size_t i = 0; i < currentStroke.size(); ++i) {
                                if (i > 0) {
                                    if (isBrushing) addParticlesLine(currentStroke[i - 1], currentStroke[i]);
                                    else if (isErasing) eraseParticlesLine(currentStroke[i - 1], currentStroke[i]);
                                } else {
                                    if (isBrushing) addParticles(currentStroke[0]);
                                    else if (isErasing) eraseParticles(currentStroke[0]);
                                }
                                minX = std::min(minX, currentStroke[i].x);
                                maxX = std::max(maxX, currentStroke[i].x);
                                minY = std::min(minY, currentStroke[i].y);
                                maxY = std::max(maxY, currentStroke[i].y);
                            }
                            float r = ui->getSelectionRadius();
                            world->notifyTerrainChanged((minX + maxX) * 0.5f, (minY + maxY) * 0.5f, std::max(maxX - minX, maxY - minY) * 0.5f + r);
                        }
                        currentStroke.clear();
                        isBrushing = false;
                        isErasing = false;
                    };

                    if (isBrushing || isErasing) processFinalizationStroke();

                    if (g_isDraggingLine) {
                        addParticlesLine(g_lineStartPos, g_lineCurrentPos);
                        float cX = (g_lineStartPos.x + g_lineCurrentPos.x) * 0.5f;
                        float cY = (g_lineStartPos.y + g_lineCurrentPos.y) * 0.5f;
                        float span = std::max(std::abs(g_lineCurrentPos.x - g_lineStartPos.x), std::abs(g_lineCurrentPos.y - g_lineStartPos.y));
                        world->notifyTerrainChanged(cX, cY, span * 0.5f + ui->getSelectionRadius());
                        g_isDraggingLine = false;
                    }

                    if (g_isDraggingEraseLine) {
                        eraseParticlesLine(g_lineStartPos, g_lineCurrentPos);
                        float cX = (g_lineStartPos.x + g_lineCurrentPos.x) * 0.5f;
                        float cY = (g_lineStartPos.y + g_lineCurrentPos.y) * 0.5f;
                        float span = std::max(std::abs(g_lineCurrentPos.x - g_lineStartPos.x), std::abs(g_lineCurrentPos.y - g_lineStartPos.y));
                        world->notifyTerrainChanged(cX, cY, span * 0.5f + ui->getSelectionRadius());
                        g_isDraggingEraseLine = false;
                    }
                }
            } 
        }

        update();
        render();
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
    // Keep track of real time passed since last loop
    sf::Time dt = clock.restart();
    frameTime = static_cast<float>(frameClock.restart().asMilliseconds());

    ImGui::SFML::Update(window, dt);

    if (currentState == GameState::PLAYING) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        sf::Vector2f worldPos = window.mapPixelToCoords(mousePos, gameView);
        
        if (ui && isUIVisible) {
            ui->update(window, dt, simulationRunning, frameTime, worldPos);
        }
        
        if (ui && renderer) {
            renderer->setShowChunkBounds(ui->getShowChunkBounds());
            renderer->setShowColliders(ui->getShowColliders());
        }
        
        if (simulationRunning && world) {
            sf::Vector2f center = gameView.getCenter();
            sf::Vector2f size = gameView.getSize();
            world->updateCameraBounds(center.x, center.y, size.x, size.y);

            // ---------------------------------------------------------
            // THE FIX: FIXED TIMESTEP ACCUMULATOR
            // ---------------------------------------------------------
            static float accumulator = 0.0f;
            const float TIME_STEP = 1.0f / 60.0f; // Exactly 60 Simulation Ticks Per Second

            // Add the time that just passed to our accumulator
            accumulator += dt.asSeconds();

            // Prevent the "Spiral of Death" if the window freezes or lags heavily
            if (accumulator > 0.25f) accumulator = 0.25f;

            // Run the simulation in fixed 1/60th of a second chunks
            while (accumulator >= TIME_STEP) {
                if (entitySystem) {
                    entitySystem->updateInput(TIME_STEP, worldPos, *world->getRigidBodySystem(), *world);
                    entitySystem->updateProceduralAnimations(TIME_STEP, *world);
                }

                world->update(TIME_STEP);
                accumulator -= TIME_STEP;
            }
        }
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

        if (world && renderer) renderer->render(window, *world);

        if (entitySystem) {
            world->getRigidBodySystem()->renderWeaponsOutline(window, entitySystem->getPlayerPos());
            if (ui && ui->getShowColliders()) world->getRigidBodySystem()->renderGluedOutlines(window, *world);
            
            entitySystem->renderEntities(window);
            
            if (ui && ui->getShowColliders()) {
                entitySystem->renderDebug(window);
            }
            
            world->getRigidBodySystem()->renderEffects(window);
        }
        
        if (isUIVisible && !isMouseOverUI() && !isPanning && window.hasFocus()) {
            sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), gameView);
            
            if (ui->getSpawnMode() == SpawnMode::Image) {
                const sf::Texture* ghostTex = ui->getSelectedAssetTexture();
                if (ghostTex) {
                    sf::Sprite ghostSprite(*ghostTex);
                    float scale = ui->getAssetScale();
                    ghostSprite.setScale({scale, scale});
                    ghostSprite.setOrigin({ghostTex->getSize().x / 2.0f, ghostTex->getSize().y / 2.0f});
                    ghostSprite.setPosition(worldPos);
                    
                    ghostSprite.setColor(sf::Color(255, 255, 255, 128)); 
                    window.draw(ghostSprite);
                }
            }
            else if (ui->getSpawnMode() == SpawnMode::Weapon) {
                const sf::Texture* ghostTex = ui->getSelectedWeaponTexture();
                std::string wName = ui->getSelectedWeaponName();
                if (ghostTex) {
                    sf::Sprite ghostSprite(*ghostTex);
                    float wScale = 1.0f;
                    if (wName.find("Revolver") != std::string::npos || wName.find("Submachine") != std::string::npos) {
                        wScale = 0.25f;
                    }
                    ghostSprite.setScale({wScale, wScale});
                    ghostSprite.setOrigin({ghostTex->getSize().x / 2.0f, ghostTex->getSize().y / 2.0f});
                    ghostSprite.setPosition(worldPos);
                    
                    ghostSprite.setColor(sf::Color(255, 255, 255, 128)); 
                    window.draw(ghostSprite);
                }
            }
            else if (ui->getSpawnMode() == SpawnMode::Entity) {
                const sf::Texture* ghostTex = ui->getSelectedEntityTexture();
                if (ghostTex) {
                    sf::Sprite ghostSprite(*ghostTex);
                    int w = std::min(32u, ghostTex->getSize().x);
                    int h = std::min(32u, ghostTex->getSize().y);
                    ghostSprite.setTextureRect(sf::IntRect({0, 0}, {w, h}));
                    
                    // For the ghost preview, we query the entity system to align perfectly to its defined origin
sf::Vector2f previewOrigin = {16.0f, 16.0f}; // Fallback
if (entitySystem) {
    for (const auto& def : entitySystem->getDefinitions()) {
        if (def.name == ui->getSelectedEntityName()) {
            // Calculate the origin from the center of the new SFML 3 colliderRect
            previewOrigin = {
                def.colliderRect.position.x + def.colliderRect.size.x / 2.0f, 
                def.colliderRect.position.y + def.colliderRect.size.y / 2.0f
            };
            break;
        }
    }
}
                    ghostSprite.setOrigin(previewOrigin); 
                    ghostSprite.setPosition(worldPos);
                    ghostSprite.setColor(sf::Color(255, 255, 255, 128)); 
                    window.draw(ghostSprite);
                } else {
                    sf::RectangleShape brush({8.0f, 16.0f});
                    brush.setOrigin({4.0f, 8.0f});
                    brush.setPosition(worldPos);
                    brush.setFillColor(sf::Color(255, 0, 0, 100));
                    window.draw(brush);
                }
            }
            else { 
                auto drawPixelatedStroke = [&](const std::vector<sf::Vector2f>& points, float radius, BrushShape shape) {
                    if (points.empty()) return;

                    float r = radius;
                    float minX = points[0].x, maxX = points[0].x;
                    float minY = points[0].y, maxY = points[0].y;
                    for (const auto& pt : points) {
                        minX = std::min(minX, pt.x);
                        maxX = std::max(maxX, pt.x);
                        minY = std::min(minY, pt.y);
                        maxY = std::max(maxY, pt.y);
                    }
                    
                    int gridMinX = static_cast<int>(std::floor(minX)) - static_cast<int>(std::ceil(radius)) - 2;
                    int gridMaxX = static_cast<int>(std::floor(maxX)) + static_cast<int>(std::ceil(radius)) + 2;
                    int gridMinY = static_cast<int>(std::floor(minY)) - static_cast<int>(std::ceil(radius)) - 2;
                    int gridMaxY = static_cast<int>(std::floor(maxY)) + static_cast<int>(std::ceil(radius)) + 2;

                    int w = gridMaxX - gridMinX;
                    int h = gridMaxY - gridMinY;

                    if (w <= 0 || h <= 0 || w > 3000 || h > 3000) return; 

                    std::vector<bool> grid(w * h, false);
                    int ir = static_cast<int>(std::ceil(radius));

                    auto applyStamp = [&](sf::Vector2f pos) {
                        int cx = static_cast<int>(std::floor(pos.x));
                        int cy = static_cast<int>(std::floor(pos.y));
                        
                        if (shape == BrushShape::Platform) {
                            int gx = cx - gridMinX;
                            int gy = cy - gridMinY;
                            if (gx >= 0 && gx < w && gy >= 0 && gy < h) {
                                grid[gy * w + gx] = true;
                            }
                        } else {
                            for (int bdy = -ir; bdy <= ir; ++bdy) {
                                for (int bdx = -ir; bdx <= ir; ++bdx) {
                                    if (shape == BrushShape::Circle && (bdx*bdx + bdy*bdy > radius*radius)) continue;
                                    int gx = cx + bdx - gridMinX;
                                    int gy = cy + bdy - gridMinY;
                                    if (gx >= 0 && gx < w && gy >= 0 && gy < h) {
                                        grid[gy * w + gx] = true;
                                    }
                                }
                            }
                        }
                    };

                    if (points.size() == 1) {
                        applyStamp(points[0]);
                    } else {
                        for (size_t p = 1; p < points.size(); ++p) {
                            sf::Vector2f startP = points[p-1];
                            sf::Vector2f endP = points[p];
                            float dx = endP.x - startP.x;
                            float dy = endP.y - startP.y;
                            float dist = std::sqrt(dx*dx + dy*dy);
                            float stepSize = (shape == BrushShape::Platform) ? 0.5f : std::max(1.0f, radius * 0.5f);
                            int steps = static_cast<int>(std::ceil(dist / stepSize));

                            for(int i = 0; i <= steps; ++i) {
                                float t = (steps > 0) ? (float)i / steps : 0.f;
                                applyStamp({startP.x + t*dx, startP.y + t*dy});
                            }
                        }
                    }
                    
                    sf::VertexArray vaFill(sf::PrimitiveType::Triangles);
                    sf::VertexArray vaOutline(sf::PrimitiveType::Triangles);
                    sf::Color fillCol(255, 255, 255, 40);
                    sf::Color outlineCol(255, 255, 255, 180);
                    
                    auto addQuad = [&](sf::VertexArray& va, int px, int py, sf::Color col) {
                        sf::Vector2f tl(px, py);
                        sf::Vector2f tr(px + 1, py);
                        sf::Vector2f br(px + 1, py + 1);
                        sf::Vector2f bl(px, py + 1);
                        va.append({tl, col}); va.append({tr, col}); va.append({br, col});
                        va.append({tl, col}); va.append({br, col}); va.append({bl, col});
                    };
                    
                    for (int y = 0; y < h; ++y) {
                        for (int x = 0; x < w; ++x) {
                            if (grid[y * w + x]) {
                                bool isOutline = false;
                                if (x == 0 || x == w - 1 || y == 0 || y == h - 1) {
                                    isOutline = true;
                                } else if (!grid[y * w + (x - 1)] || !grid[y * w + (x + 1)] || !grid[(y - 1) * w + x] || !grid[(y + 1) * w + x]) {
                                    isOutline = true;
                                }
                                
                                int worldPx = gridMinX + x;
                                int worldPy = gridMinY + y;
                                
                                if (isOutline) {
                                    addQuad(vaOutline, worldPx, worldPy, outlineCol);
                                } else {
                                    addQuad(vaFill, worldPx, worldPy, fillCol);
                                }
                            }
                        }
                    }
                    if (vaFill.getVertexCount() > 0) window.draw(vaFill);
                    if (vaOutline.getVertexCount() > 0) window.draw(vaOutline);
                };

                if (ui->getUseLineMode()) {
                    if (g_isDraggingLine || g_isDraggingEraseLine) {
                        drawPixelatedStroke({g_lineStartPos, g_lineCurrentPos}, ui->getSelectionRadius(), ui->getBrushShape());
                    } else {
                        drawPixelatedStroke({worldPos}, ui->getSelectionRadius(), ui->getBrushShape());
                    }
                } else {
                    if (isBrushing || isErasing) {
                        if (!currentStroke.empty()) drawPixelatedStroke(currentStroke, ui->getSelectionRadius(), ui->getBrushShape());
                    } else {
                        drawPixelatedStroke({worldPos}, ui->getSelectionRadius(), ui->getBrushShape());
                    }
                }
            }
        }

        window.setView(window.getDefaultView());
        if (ui && isUIVisible) ui->render(window); 
    }
    
    ImGui::SFML::Render(window);
    window.display();
}

void SandSimApp::startGame(const std::string& worldFile) {
    // 1. Create the base world FIRST so it exists in memory
    world = std::make_unique<ParticleWorld>(VIEW_WIDTH, VIEW_HEIGHT);

    // 2. Load all materials BEFORE the entities or save files try to use them!
    world->loadAllMaterials({
        "src/Particles/data/Special.json",
        "src/Particles/data/ImmovableSolid.json",
        "src/Particles/data/Liquid.json",
        "src/Particles/data/Gas.json",
        "src/Particles/data/MovableSolid.json"
    });

    // 3. NOW it is safe to set up the Entity System using the World
    entitySystem = std::make_unique<EntitySystem>(world->getRigidBodySystem()->getWorldId());
    world->setEntitySystem(entitySystem.get());
    
    // 4. Load the save file (if one exists)
    if (!worldFile.empty()) {
        world->loadWorld(worldFile);
    }
    
    // 5. Setup the UI
    ui = std::make_unique<UI>(window, world.get());
    
    // 6. Reset camera
    currentZoom = 1.0f;
    gameView.setSize({static_cast<float>(WINDOW_WIDTH), static_cast<float>(WINDOW_HEIGHT)});
    gameView.setCenter({static_cast<float>(WORLD_WIDTH) / 2.f, static_cast<float>(WORLD_HEIGHT) / 2.f});
    
    currentState = GameState::PLAYING;
    handleResize(window.getSize().x, window.getSize().y);
}
void SandSimApp::returnToMenu() {
    entitySystem.reset(); 
    world.reset();
    ui.reset();
    g_isDraggingLine = false;
    g_isDraggingEraseLine = false;
    isBrushing = false;
    isErasing = false;
    currentStroke.clear();
    currentState = GameState::MENU;
}

void SandSimApp::handleMouseHeld() {
    if (ui && ui->getSpawnMode() != SpawnMode::Particles) return;

    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    sf::Vector2f worldPos = window.mapPixelToCoords(mousePos, gameView);
    
    if (ui->getUseLineMode()) {
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            if (!g_isDraggingLine) {
                g_isDraggingLine = true;
                g_lineStartPos = worldPos;
            }
            g_lineCurrentPos = worldPos;
        }
        else if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
            if (!g_isDraggingEraseLine) {
                g_isDraggingEraseLine = true;
                g_lineStartPos = worldPos;
            }
            g_lineCurrentPos = worldPos;
        }
    } else {
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            if (!isBrushing) {
                isBrushing = true;
                currentStroke.clear();
            }
            if (currentStroke.empty() || std::hypot(currentStroke.back().x - worldPos.x, currentStroke.back().y - worldPos.y) >= 1.0f) {
                currentStroke.push_back(worldPos);
            }
        } 
        else if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
            if (!isErasing) {
                isErasing = true;
                currentStroke.clear();
            }
            if (currentStroke.empty() || std::hypot(currentStroke.back().x - worldPos.x, currentStroke.back().y - worldPos.y) >= 1.0f) {
                currentStroke.push_back(worldPos);
            }
        }
    }
}

void SandSimApp::addParticles(const sf::Vector2f& worldPos) {
    if (world && ui) {
        MaterialID currentMat = ui->getCurrentMaterialID();
        if (currentMat == GetMatID("Explosion")) {
            int radius = static_cast<int>(ui->getSelectionRadius());
            world->triggerExplosion(static_cast<int>(worldPos.x), static_cast<int>(worldPos.y), radius, 20);
        } else {
            if (ui->getBrushShape() == BrushShape::Platform) {
                int px = static_cast<int>(std::round(worldPos.x));
                int py = static_cast<int>(std::round(worldPos.y));
                world->spawnParticle(currentMat, px, py);
                BaseComponent* base = world->get<BaseComponent>(px, py);
                if (base) base->compMask |= COMP_PLATFORM;
            } else if (ui->getBrushShape() == BrushShape::Square) {
                world->addParticleSquare(worldPos.x, worldPos.y, ui->getSelectionRadius(), currentMat);
            } else {
                world->addParticleCircle(worldPos.x, worldPos.y, ui->getSelectionRadius(), currentMat);
            }
        }
    }
}

void SandSimApp::eraseParticles(const sf::Vector2f& worldPos) {
    if (world && ui) {
        float r = ui->getSelectionRadius();
        if (ui->getBrushShape() == BrushShape::Square) {
            world->eraseSquare(worldPos.x, worldPos.y, r);
            if (entitySystem) entitySystem->eraseEntitiesInSquare(worldPos, r);
            if (world->getRigidBodySystem()) world->getRigidBodySystem()->eraseInSquare(worldPos, r);
        } else {
            world->eraseCircle(worldPos.x, worldPos.y, r);
            if (entitySystem) entitySystem->eraseEntitiesInRadius(worldPos, r);
            if (world->getRigidBodySystem()) world->getRigidBodySystem()->eraseInRadius(worldPos, r);
        }
    }
}

void SandSimApp::addParticlesLine(const sf::Vector2f& start, const sf::Vector2f& end) {
    MaterialID currentMat = ui->getCurrentMaterialID();
    if (currentMat == GetMatID("Explosion")) {
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
    
    if (ui->getBrushShape() == BrushShape::Platform) {
        float stepSize = 0.5f;
        int steps = static_cast<int>(std::ceil(dist / stepSize));
        for(int i=0; i<=steps; ++i) {
            float t = (steps > 0) ? (float)i/steps : 0.f;
            int px = static_cast<int>(std::round(start.x + t*dx));
            int py = static_cast<int>(std::round(start.y + t*dy));
            world->spawnParticle(currentMat, px, py);
            BaseComponent* base = world->get<BaseComponent>(px, py);
            if (base) base->compMask |= COMP_PLATFORM;
        }
        return;
    }

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