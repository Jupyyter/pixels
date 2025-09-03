#include "UI.hpp"
#include <iostream>
#include <algorithm>
#include "ParticleWorld.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
namespace SandSim {

UI::UI(ParticleWorld* worldPtr) : currentSelection(MaterialSelection::Sand),
           showMaterialPanel(true),
           showFrameCount(true),
           showSimulationState(true),
           showControls(true),
           selectionRadius(DEFAULT_SELECTION_RADIUS),
           uiTexture(sf::Vector2u(TEXTURE_WIDTH, TEXTURE_HEIGHT)),
           uiSprite(uiTexture.getTexture()),
           fontLoaded(false),
           world(worldPtr){  // Initialize world pointer

    // Apply no-repeat settings to UI texture to prevent edge bleeding
    const_cast<sf::Texture&>(uiTexture.getTexture()).setRepeated(false);
    const_cast<sf::Texture&>(uiTexture.getTexture()).setSmooth(false);

    // Load font
    fontLoaded = loadFont();

    // Initialize text objects if font loaded successfully
    if (fontLoaded) {
        frameInfoText.setFont(font);
        frameInfoText.setCharacterSize(16);
        frameInfoText.setFillColor(sf::Color::White);
        frameInfoText.setPosition({10, 10});

        materialHoverText.setFont(font);
        materialHoverText.setCharacterSize(14);
        materialHoverText.setFillColor(sf::Color::White);

        simulationStateText.setFont(font);
        simulationStateText.setCharacterSize(14);
        simulationStateText.setFillColor(sf::Color::White);
        simulationStateText.setPosition({10, 0});

        controlsText.setFont(font);
        controlsText.setCharacterSize(12);
        controlsText.setFillColor(sf::Color(200, 200, 200));
        controlsText.setPosition({10, 70});
        
        std::string controls = 
            "Controls:\n"
            "B - Bloom\n"
            "I - Toggle UI | F - Toggle FPS\n";
        controlsText.setString(controls);

        // Initialize save button text
        saveButtonText.setFont(font);
        saveButtonText.setCharacterSize(14);
        saveButtonText.setFillColor(sf::Color::White);
    }

    setupMaterialButtons();
    setupSaveButton();  // Initialize save button
}
// Fixed setupSaveButton method - place this in your UI.cpp file

void UI::setupSaveButton() {
    // Position button right under the controls text
    // Controls text is at {10, 70} and has 3 lines with font size 12
    // Each line is approximately 14-16 pixels tall (font size + line spacing)
    int controlsTextHeight = 3 * 16; // 3 lines * ~16px per line
    int buttonY = 70 + controlsTextHeight + 10; // controls Y + text height + 10px gap
    
    // Calculate button size based on text dimensions if font is loaded
    if (fontLoaded) {
        saveButtonText.setString(saveButton.text);
        sf::FloatRect textBounds = saveButtonText.getLocalBounds();
        
        // Add padding around the text (20px horizontal, 10px vertical)
        saveButton.size = {
            static_cast<int>(textBounds.size.x) + 20,
            static_cast<int>(textBounds.size.y) + 10
        };
    } else {
        // Fallback size if font isn't loaded
        saveButton.size = {100, 30}; // Reasonable default size
    }
    
    // Position button on the LEFT side, aligned with controls text
    saveButton.position = {10, buttonY}; // Same X as controls text (10px from left)
    
    if (fontLoaded) {
        saveButtonText.setString(saveButton.text);
        
        // Recalculate text position after button size is determined
        sf::FloatRect textBounds = saveButtonText.getLocalBounds();

        float textX = saveButton.position.x + 
                      (saveButton.size.x - textBounds.size.x) / 2.f;

        float textY = saveButton.position.y + 
                      (saveButton.size.y - textBounds.size.y) / 2.f - textBounds.position.y;

        saveButtonText.setPosition({textX, textY});  // SFML 3 syntax
    }
}
bool UI::handleClick(const sf::Vector2f& worldMousePos) {
    if (!showMaterialPanel) return false;
    
    // Check save button click first
    if (isPointInRect(worldMousePos, saveButton.position, saveButton.size)) {
        if (world != nullptr) {
            saveButton.isPressed = true;
            world->saveWorld("world");  // Call save method
            std::cout << "Save button clicked!" << std::endl;
        } else {
            std::cerr << "Warning: World reference not set in UI!" << std::endl;
        }
        return true; // UI consumed the click
    }
    
    // Check material buttons
    for (const auto& button : materialButtons) {
        if (isPointInRect(worldMousePos, button.position, button.size)) {
            currentSelection = button.selection;
            return true; // UI consumed the click
        }
    }
    return false; // Click not consumed by UI
}
bool UI::loadFont()
{
    // Try multiple font paths as fallbacks
    std::vector<std::string> fontPaths = {
        "assets/fonts/ARIAL.TTF",
        "assets/fonts/arial.ttf",
        "assets/fonts/Arial.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/Arial.ttf",
        "/System/Library/Fonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "font.ttf",
        "arial.ttf"};

    for (const auto &path : fontPaths)
    {
        if (font.openFromFile(path))
        {
            return true;
        }
    }

    std::cerr << "Warning: Could not load any font. Text will not display properly." << std::endl;
    return false;
}
void UI::setupMaterialButtons() {
    materialButtons.clear();
    
    // Define all material buttons
    std::vector<std::pair<std::string, std::pair<MaterialID, sf::Color>>> materials = {
        {"Sand", {MaterialID::Sand, MAT_COL_SAND}},
        {"Water", {MaterialID::Water, MAT_COL_WATER}},
        {"Salt", {MaterialID::Salt, MAT_COL_SALT}},
        {"Wood", {MaterialID::Wood, MAT_COL_WOOD}},
        {"Fire", {MaterialID::Fire, MAT_COL_FIRE}},
        {"Smoke", {MaterialID::Smoke, MAT_COL_SMOKE}},
        {"Steam", {MaterialID::Steam, MAT_COL_STEAM}},
        {"Gunpowder", {MaterialID::Gunpowder, MAT_COL_GUNPOWDER}},
        {"Oil", {MaterialID::Oil, MAT_COL_OIL}},
        {"Lava", {MaterialID::Lava, MAT_COL_LAVA}},
        {"Stone", {MaterialID::Stone, MAT_COL_STONE}},
        {"Acid", {MaterialID::Acid, MAT_COL_ACID}}
    };
    
    for (size_t i = 0; i < materials.size(); ++i) {
        MaterialButton button;
        button.position = {TEXTURE_WIDTH - UI_PANEL_X_OFFSET, UI_PANEL_BASE + static_cast<int>(i) * UI_PANEL_OFFSET};
        button.size = {UI_PANEL_BUTTON_SIZE, UI_PANEL_BUTTON_SIZE};
        button.color = materials[i].second.second;
        button.name = materials[i].first;
        button.materialID = materials[i].second.first;
        button.selection = static_cast<MaterialSelection>(i);
        materialButtons.push_back(button);
    }
}

void UI::update(const sf::Vector2f& worldMousePos, float frameTime, bool simulationRunning) {
    mousePos = worldMousePos;

    // Update save button hover state
    saveButton.isHovered = isPointInRect(worldMousePos, saveButton.position, saveButton.size);
    
    // Reset press state after a short time
    if (saveButton.isPressed) {
        static sf::Clock pressTimer;
        if (pressTimer.getElapsedTime().asSeconds() > 0.2f) {
            saveButton.isPressed = false;
            pressTimer.restart();
        }
    }

    // Update frame info text content
    if (showFrameCount && fontLoaded) {
        std::string fpsText = "FPS: " + std::to_string(static_cast<int>(1000.0f / std::max(frameTime, 1.0f)));
        std::string radiusText = "Radius: " + std::to_string(static_cast<int>(selectionRadius));
        frameInfoText.setString(fpsText + "\n" + radiusText);
    }

    // Update simulation state text content
    if (showSimulationState && fontLoaded) {
        simulationStateText.setString(simulationRunning ? "Simulation: Running" : "Simulation: Paused");
    }
}
void UI::drawSaveButton() {
    sf::RectangleShape buttonRect;
    buttonRect.setPosition(sf::Vector2f(static_cast<float>(saveButton.position.x), 
                                       static_cast<float>(saveButton.position.y)));
    buttonRect.setSize(sf::Vector2f(static_cast<float>(saveButton.size.x), 
                                   static_cast<float>(saveButton.size.y)));
    
    // Set button color based on state
    if (saveButton.isPressed) {
        buttonRect.setFillColor(sf::Color(50, 100, 150));  // Darker when pressed
    } else if (saveButton.isHovered) {
        buttonRect.setFillColor(saveButton.hoverColor);
    } else {
        buttonRect.setFillColor(saveButton.color);
    }
    
    // Add border
    buttonRect.setOutlineThickness(1);
    buttonRect.setOutlineColor(sf::Color::White);
    
    uiTexture.draw(buttonRect);
    
    // Draw button text if font is loaded
    if (fontLoaded) {
        uiTexture.draw(saveButtonText);
    }
}

void UI::handleKeyPress(sf::Keyboard::Key key) {
    switch (key) {
        case sf::Keyboard::Key::I:
            showMaterialPanel = !showMaterialPanel;
            break;
        case sf::Keyboard::Key::F:
            showFrameCount = !showFrameCount;
            break;
        case sf::Keyboard::Key::H:
            showControls = !showControls;
            break;
        case sf::Keyboard::Key::LBracket:
            selectionRadius = std::max(MIN_SELECTION_RADIUS, selectionRadius - 1.0f);
            break;
        case sf::Keyboard::Key::RBracket:
            selectionRadius = std::min(MAX_SELECTION_RADIUS, selectionRadius + 1.0f);
            break;
        default:
            break;
    }
}

void UI::handleMouseWheel(float delta) {
    if (delta > 0) {
        selectionRadius = std::min(MAX_SELECTION_RADIUS, selectionRadius + 1.0f);
    } else if (delta < 0) {
        selectionRadius = std::max(MIN_SELECTION_RADIUS, selectionRadius - 1.0f);
    }
}

void UI::render(sf::RenderTarget& target) {
    uiTexture.clear(sf::Color::Transparent);

    // Only draw text-based UI elements if font is loaded
    if (fontLoaded) {
        if (showMaterialPanel) {
            drawMaterialPanel();
            drawSaveButton();  // Draw save button
        }

        if (showFrameCount) {
            uiTexture.draw(frameInfoText);
        }
        
        if (showSimulationState) {
            uiTexture.draw(simulationStateText);
        }

        if (showControls) {
            uiTexture.draw(controlsText);
        }
    } else if (showMaterialPanel) {
        // Draw material panel without text if font failed to load
        for (const auto& button : materialButtons) {
            sf::RectangleShape rect;
            rect.setPosition(sf::Vector2f(static_cast<float>(button.position.x), static_cast<float>(button.position.y)));
            rect.setSize(sf::Vector2f(static_cast<float>(button.size.x), static_cast<float>(button.size.y)));
            rect.setFillColor(button.color);
            
            // Highlight selected material
            if (button.selection == currentSelection) {
                sf::RectangleShape border;
                border.setPosition({rect.getPosition().x - 2, rect.getPosition().y - 2});
                border.setSize(sf::Vector2f(rect.getSize().x + 4, rect.getSize().y + 4));
                border.setFillColor(sf::Color(255, 255, 0, 180));
                uiTexture.draw(border);
            }
            
            uiTexture.draw(rect);
        }
        
        // Draw save button even without font
        drawSaveButton();
    }

    // Always draw selection circle (doesn't require font)
    drawSelectionCircle();
    
    uiTexture.display();

    // Scale and position the UI properly to match the game world
    sf::Vector2u windowSize = static_cast<sf::RenderWindow&>(target).getSize();

    // Calculate scaling to maintain aspect ratio (same as renderer)
    float scaleX = static_cast<float>(windowSize.x) / TEXTURE_WIDTH;
    float scaleY = static_cast<float>(windowSize.y) / TEXTURE_HEIGHT;
    float scale = std::min(scaleX, scaleY);

    // Position the UI sprite with same offset as game world
    uiSprite.setScale({scale, scale});
    float offsetX = (windowSize.x - TEXTURE_WIDTH * scale) / 2.0f;
    float offsetY = (windowSize.y - TEXTURE_HEIGHT * scale) / 2.0f;
    uiSprite.setPosition({offsetX, offsetY});
    
    // Set proper texture rect to prevent edge bleeding
    uiSprite.setTextureRect(sf::IntRect({0, 0},{ static_cast<int>(TEXTURE_WIDTH), static_cast<int>(TEXTURE_HEIGHT)}));

    target.draw(uiSprite);
}

MaterialID UI::getCurrentMaterialID() const {
    return materialButtons[static_cast<size_t>(currentSelection)].materialID;
}

float UI::getSelectionRadius() const {
    return selectionRadius;
}

const std::vector<MaterialButton>& UI::getMaterialButtons() const {
    return materialButtons;
}

bool UI::getShowMaterialPanel() const {
    return showMaterialPanel;
}

void UI::setShowMaterialPanel(bool show) {
    showMaterialPanel = show;
}

void UI::setShowFrameCount(bool show) {
    showFrameCount = show;
}

void UI::setShowSimulationState(bool show) {
    showSimulationState = show;
}

void UI::setShowControls(bool show) {
    showControls = show;
}

bool UI::isPointInRect(const sf::Vector2f& point, const sf::Vector2i& rectPos, const sf::Vector2i& rectSize) const {
    return point.x >= rectPos.x && point.x <= rectPos.x + rectSize.x &&
           point.y >= rectPos.y && point.y <= rectPos.y + rectSize.y;
}

void UI::drawMaterialPanel() {
    for (const auto& button : materialButtons) {
        sf::RectangleShape rect;
        rect.setPosition(sf::Vector2f(static_cast<float>(button.position.x), static_cast<float>(button.position.y)));
        rect.setSize(sf::Vector2f(static_cast<float>(button.size.x), static_cast<float>(button.size.y)));
        rect.setFillColor(button.color);
        
        // Highlight selected material with a more visible selection
        if (button.selection == currentSelection) {
            sf::RectangleShape border;
            border.setPosition({rect.getPosition().x - 2, rect.getPosition().y - 2});
            border.setSize(sf::Vector2f(rect.getSize().x + 4, rect.getSize().y + 4));
            border.setFillColor(sf::Color(255, 255, 0, 180)); // Bright yellow selection
            uiTexture.draw(border);
        }
        
        uiTexture.draw(rect);
        
        // Check if mouse is over this button for hover text (only if font loaded)
        if (fontLoaded && isPointInRect(mousePos, button.position, button.size)) {
            // Calculate text position (centered at top of screen)
            float textWidth = button.name.length() * 8.0f; // Approximate character width
            sf::Vector2f textPos(TEXTURE_WIDTH / 2.0f - textWidth / 2.0f, 10);
            
            // Background for text (semi-transparent with border for better visibility)
            sf::RectangleShape textBg;
            textBg.setPosition({textPos.x - 8, textPos.y - 3});
            textBg.setSize(sf::Vector2f(textWidth + 16, 22));
            textBg.setFillColor(sf::Color(0, 0, 0, 200)); // Semi-transparent black
            textBg.setOutlineColor(sf::Color::White);
            textBg.setOutlineThickness(1);
            uiTexture.draw(textBg);
            
            // Draw the hover text
            materialHoverText.setString(button.name);
            materialHoverText.setPosition(textPos);
            uiTexture.draw(materialHoverText);
        }
    }
}

void UI::drawSelectionCircle() {
    sf::CircleShape circle;
    circle.setRadius(selectionRadius);
    circle.setOrigin({selectionRadius, selectionRadius});
    circle.setPosition(mousePos);
    circle.setFillColor(sf::Color::Transparent);
    circle.setOutlineThickness(1.0f);
    circle.setOutlineColor(sf::Color::White);
    uiTexture.draw(circle);
}

} // namespace SandSim