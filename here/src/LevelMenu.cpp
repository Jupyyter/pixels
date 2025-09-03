#include "LevelMenu.hpp"
#include "ParticleWorld.hpp"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace SandSim {

const float LevelMenu::ASPECT_RATIO = 4.0f / 3.0f; // 4:3 aspect ratio

LevelMenu::LevelMenu(int levelsPerRow, float paddingPercent) 
    : scrollOffset(0), maxScrollOffset(0), isDragging(false),
      selectedLevel(-1), fontLoaded(false),
      levelsPerRow(levelsPerRow), paddingPercent(paddingPercent),
      menuTexture(sf::Vector2u(TEXTURE_WIDTH, TEXTURE_HEIGHT)),
      menuSprite(menuTexture.getTexture()), titleText(fonttt), instructionsText(fonttt) {

    // Apply same texture settings as UI and renderer to prevent edge bleeding
    const_cast<sf::Texture&>(menuTexture.getTexture()).setRepeated(false);
    const_cast<sf::Texture&>(menuTexture.getTexture()).setSmooth(false);

    fontLoaded = loadFont();
    calculateLayout(); // Calculate dimensions based on parameters
    
    // Setup background using TEXTURE dimensions
    background.setSize(sf::Vector2f(TEXTURE_WIDTH, TEXTURE_HEIGHT));
    background.setFillColor(sf::Color(30, 30, 40));

    // Setup header background
    headerBackground.setSize(sf::Vector2f(TEXTURE_WIDTH, MENU_HEADER_HEIGHT));
    headerBackground.setPosition(sf::Vector2f(0, 0));
    headerBackground.setFillColor(sf::Color(0, 0, 0, 200));

    if (fontLoaded) {
        // Setup title - smaller size
        titleText.setFont(fonttt);
        titleText.setCharacterSize(24); // Reduced from 36 to 24
        titleText.setFillColor(sf::Color::White);
        titleText.setString("Select Level");

        sf::FloatRect titleBounds = titleText.getLocalBounds();
        titleText.setPosition(sf::Vector2f((TEXTURE_WIDTH - titleBounds.size.x) / 2.0f, 8)); // Reduced from 15 to 8

        // Setup instructions - smaller size  
        instructionsText.setFont(fonttt);
        instructionsText.setCharacterSize(12); // Reduced from 14 to 12
        instructionsText.setFillColor(sf::Color(200, 200, 200));
        instructionsText.setString("Click on a level to start playing");

        sf::FloatRect instrBounds = instructionsText.getLocalBounds();
        instructionsText.setPosition(sf::Vector2f((TEXTURE_WIDTH - instrBounds.size.x) / 2.0f, 38)); // Adjusted for smaller header
    }

    loadLevels();
    setupLayout();
}

void LevelMenu::calculateLayout() {
    // Calculate available width (total width minus padding on both sides)
    float totalPadding = TEXTURE_WIDTH * paddingPercent;
    float availableWidth = TEXTURE_WIDTH - totalPadding;
    
    // Calculate thumbnail width with fixed 20px margins between thumbnails
    float marginsWidth = (levelsPerRow - 1) * 20.0f;
    thumbnailWidth = static_cast<int>((availableWidth - marginsWidth) / levelsPerRow);
    
    // Calculate height based on aspect ratio
    thumbnailHeight = static_cast<int>(thumbnailWidth / ASPECT_RATIO);
    
    // Ensure minimum sizes for usability
    thumbnailWidth = std::max(thumbnailWidth, 120);
    thumbnailHeight = std::max(thumbnailHeight, 90);
    
    // Keep fixed 20px margin between thumbnails
    thumbnailMargin = 20;
    
    // Calculate edge padding to center the BACKGROUND RECTANGLES perfectly
    // Background rectangles are thumbnailWidth + 10 pixels wide
    float backgroundWidth = thumbnailWidth + 10;
    float totalContentWidth = levelsPerRow * backgroundWidth + (levelsPerRow - 1) * thumbnailMargin;
    edgePadding = static_cast<int>((TEXTURE_WIDTH - totalContentWidth) / 2.0f);
    
    std::cout << "Layout calculated: " << levelsPerRow << " per row, " 
              << thumbnailWidth << "x" << thumbnailHeight << " thumbnails, "
              << "backgrounds: " << backgroundWidth << "px wide, "
              << thumbnailMargin << "px margin, " << edgePadding << "px edge padding" << std::endl;
    
    // Debug: verify the spacing
    float lastBackgroundX = edgePadding + (levelsPerRow - 1) * (backgroundWidth + thumbnailMargin);
    float rightEdge = lastBackgroundX + backgroundWidth;
    float rightSpace = TEXTURE_WIDTH - rightEdge;
    std::cout << "Debug: Left space=" << edgePadding << ", Right space=" << rightSpace << std::endl;
}

void LevelMenu::setLevelsPerRow(int count) {
    if (count > 0 && count != levelsPerRow) {
        levelsPerRow = count;
        calculateLayout();
        setupLayout();
    }
}

void LevelMenu::setPaddingPercent(float percent) {
    if (percent >= 0.0f && percent <= 0.5f && percent != paddingPercent) {
        paddingPercent = percent;
        calculateLayout();
        setupLayout();
    }
}

bool LevelMenu::loadFont() {
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
        "arial.ttf"
    };

    for (const auto& path : fontPaths) {
        if (fonttt.openFromFile(path)) {
            std::cout << "Font loaded successfully from: " << path << std::endl;
            return true;
        }
    }

    std::cerr << "Warning: Could not load any font. Text will not display properly." << std::endl;
    return false;
}

void LevelMenu::loadLevels() {
    levels.clear();

    // Change from "." to "worlds" directory
    std::string worldsDir = "worlds";
    
    try {
        // Check if worlds directory exists
        if (!std::filesystem::exists(worldsDir)) {
            std::cerr << "Worlds directory does not exist: " << worldsDir << std::endl;
            return;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(worldsDir)) {
            if (entry.path().extension() == ".rrr") {
                LevelInfo level(fonttt);
                level.filename = entry.path().string();
                level.displayName = entry.path().stem().string();

                // Generate thumbnail
                generateThumbnail(level.filename, level.thumbnail);

                // Check if thumbnail was generated successfully
                sf::Vector2u thumbSize = level.thumbnail.getSize();
                level.thumbnailLoaded = (thumbSize.x > 0 && thumbSize.y > 0);

                if (level.thumbnailLoaded) {
                    level.thumbnailSprite.setTexture(level.thumbnail);
                    level.thumbnailSprite.setColor(sf::Color::White);

                    // Scale to fit calculated thumbnail size
                    float scaleX = static_cast<float>(thumbnailWidth) / thumbSize.x;
                    float scaleY = static_cast<float>(thumbnailHeight) / thumbSize.y;
                    float scale = std::min(scaleX, scaleY);
                    level.thumbnailSprite.setScale({scale, scale});
                }

                // Setup background with calculated dimensions
                level.background.setSize(sf::Vector2f(thumbnailWidth + 10, thumbnailHeight + TEXT_AREA_HEIGHT + 10));
                level.background.setFillColor(sf::Color(50, 50, 60));
                level.background.setOutlineThickness(2);
                level.background.setOutlineColor(sf::Color(70, 70, 80));

                // Setup text with adaptive font size
                if (fontLoaded) {
                    level.nameText.setFont(fonttt);
                    // Scale font size based on thumbnail width
                    int fontSize = std::max(12, std::min(20, thumbnailWidth / 12));
                    level.nameText.setCharacterSize(fontSize);
                    level.nameText.setFillColor(sf::Color::White);
                    level.nameText.setString(level.displayName);
                }

                levels.push_back(std::move(level));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading levels from " << worldsDir << ": " << e.what() << std::endl;
    }

    std::cout << "Loaded " << levels.size() << " levels from " << worldsDir << " directory" << std::endl;
}

void LevelMenu::refreshLevels() {
    loadLevels();
    setupLayout();
    std::cout << "Level menu refreshed with " << levels.size() << " levels" << std::endl;
}

void LevelMenu::generateThumbnail(const std::string& worldFile, sf::Texture& thumbnail) {
    try {
        ParticleWorld tempWorld(TEXTURE_WIDTH, TEXTURE_HEIGHT);

        if (tempWorld.loadWorld(worldFile)) {
            const std::uint8_t* pixelBuffer = tempWorld.getPixelBuffer();

            if (pixelBuffer) {
                sf::Vector2u imageSize(tempWorld.getWidth(), tempWorld.getHeight());

                std::vector<std::uint8_t> fixedPixelBuffer(imageSize.x * imageSize.y * 4);
                for (unsigned int i = 0; i < imageSize.x * imageSize.y; ++i) {
                    unsigned int idx = i * 4;

                    fixedPixelBuffer[idx] = pixelBuffer[idx];         // R
                    fixedPixelBuffer[idx + 1] = pixelBuffer[idx + 1]; // G
                    fixedPixelBuffer[idx + 2] = pixelBuffer[idx + 2]; // B

                    if (pixelBuffer[idx] == 0 && pixelBuffer[idx + 1] == 0 && pixelBuffer[idx + 2] == 0) {
                        fixedPixelBuffer[idx + 3] = 0;
                    } else {
                        fixedPixelBuffer[idx + 3] = 255;
                    }
                }

                sf::Image worldImage(imageSize, fixedPixelBuffer.data());
                thumbnail.loadFromImage(worldImage);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error generating thumbnail: " << e.what() << std::endl;
    }
}

sf::Vector2f LevelMenu::getLevelPosition(int index) const {
    int row = index / levelsPerRow;
    int col = index % levelsPerRow;

    // Calculate position for the BACKGROUND RECTANGLE (which is what's visually centered)
    // Background rectangles are (thumbnailWidth + 10) wide with 20px margins between them
    float backgroundWidth = thumbnailWidth + 10;
    float x = edgePadding + col * (backgroundWidth + thumbnailMargin);
    float y = MENU_HEADER_HEIGHT + 20 + row * (thumbnailHeight + TEXT_AREA_HEIGHT + 20) + scrollOffset;

    // Debug output for first row to verify positioning
    if (row == 0) {
        float backgroundRight = x + backgroundWidth;
        if (col == levelsPerRow - 1) {
            float rightSpace = TEXTURE_WIDTH - backgroundRight;
        }
    }

    return sf::Vector2f(x, y);
}

void LevelMenu::setupLayout() {
    for (size_t i = 0; i < levels.size(); ++i) {
        sf::Vector2f pos = getLevelPosition(static_cast<int>(i));
        levels[i].position = pos;
        
        // Background is positioned at the calculated position
        levels[i].background.setPosition(pos);
        
        // Thumbnail sprite is positioned 5px inside the background (centered)
        levels[i].thumbnailSprite.setPosition({pos.x + 5, pos.y + 5});

        if (fontLoaded) {
            sf::FloatRect textBounds = levels[i].nameText.getLocalBounds();
            // Center text within the background width
            float backgroundWidth = thumbnailWidth + 10;
            levels[i].nameText.setPosition(
                sf::Vector2f(
                    pos.x + (backgroundWidth - textBounds.size.x) / 2.0f,
                    pos.y + thumbnailHeight + 10.0f));
        }
    }

    updateScrollBounds();
}

void LevelMenu::updateScrollBounds() {
    if (levels.empty()) {
        maxScrollOffset = 0;
        return;
    }

    int totalRows = (static_cast<int>(levels.size()) + levelsPerRow - 1) / levelsPerRow;
    float totalHeight = totalRows * (thumbnailHeight + TEXT_AREA_HEIGHT + 20);
    float visibleHeight = TEXTURE_HEIGHT - MENU_HEADER_HEIGHT - 20;

    maxScrollOffset = std::max(0.0f, totalHeight - visibleHeight);
}

sf::Vector2f LevelMenu::windowToMenuCoords(const sf::Vector2f& windowPos, const sf::RenderWindow& window) const {
    sf::Vector2u windowSize = window.getSize();
    
    float scaleX = static_cast<float>(windowSize.x) / TEXTURE_WIDTH;
    float scaleY = static_cast<float>(windowSize.y) / TEXTURE_HEIGHT;
    float scale = std::min(scaleX, scaleY);
    
    float offsetX = (windowSize.x - TEXTURE_WIDTH * scale) / 2.0f;
    float offsetY = (windowSize.y - TEXTURE_HEIGHT * scale) / 2.0f;
    
    float menuX = (windowPos.x - offsetX) / scale;
    float menuY = (windowPos.y - offsetY) / scale;
    
    return sf::Vector2f(menuX, menuY);
}

void LevelMenu::update(const sf::Vector2f& mousePos) {
    for (auto& level : levels) {
        sf::FloatRect bounds = level.background.getGlobalBounds();
        level.isHovered = bounds.contains(mousePos);

        if (level.isHovered) {
            level.background.setFillColor(sf::Color(70, 70, 90));
            level.background.setOutlineColor(sf::Color(100, 150, 200));
        } else {
            level.background.setFillColor(sf::Color(50, 50, 60));
            level.background.setOutlineColor(sf::Color(70, 70, 80));
        }
    }
}

bool LevelMenu::handleClick(const sf::Vector2f& mousePos) {
    for (size_t i = 0; i < levels.size(); ++i) {
        sf::FloatRect bounds = levels[i].background.getGlobalBounds();
        if (bounds.contains(mousePos)) {
            selectedLevel = static_cast<int>(i);
            std::cout << "Selected level: " << levels[i].displayName << std::endl;
            return true;
        }
    }
    return false;
}

void LevelMenu::handleMouseDrag(const sf::Vector2f& mousePos, bool pressed) {
    if (pressed && !isDragging) {
        isDragging = true;
        dragStartPos = mousePos;
        dragStartOffset = scrollOffset;
    } else if (!pressed) {
        isDragging = false;
    }

    if (isDragging) {
        float deltaY = mousePos.y - dragStartPos.y;
        scrollOffset = std::clamp(dragStartOffset + deltaY, -maxScrollOffset, 0.0f);
        setupLayout();
    }
}

void LevelMenu::handleMouseWheel(float delta) {
    scrollOffset = std::clamp(scrollOffset + delta * 30.0f, -maxScrollOffset, 0.0f);
    setupLayout();
}

std::string LevelMenu::getSelectedLevelFile() const {
    if (selectedLevel >= 0 && selectedLevel < static_cast<int>(levels.size())) {
        return levels[selectedLevel].filename;
    }
    return "";
}

void LevelMenu::render(sf::RenderTarget& target) {
    menuTexture.clear(sf::Color::Transparent);
    menuTexture.draw(background);

    // Draw levels
    for (const auto& level : levels) {
        if (level.position.y + thumbnailHeight > 0 && level.position.y < TEXTURE_HEIGHT) {
            menuTexture.draw(level.background);

            if (level.thumbnailLoaded) {
                sf::RenderStates states;
                sf::Transform transform;

                sf::Vector2u thumbSize = level.thumbnail.getSize();
                float scaleX = static_cast<float>(thumbnailWidth) / thumbSize.x;
                float scaleY = static_cast<float>(thumbnailHeight) / thumbSize.y;
                float scale = std::min(scaleX, scaleY);

                transform.translate({level.position.x + 5, level.position.y + 5});
                transform.scale({scale, scale});

                states.transform = transform;
                states.texture = &level.thumbnail;

                sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
                quad[0].position = sf::Vector2f(0, 0);
                quad[0].texCoords = sf::Vector2f(0, 0);
                quad[1].position = sf::Vector2f(thumbSize.x, 0);
                quad[1].texCoords = sf::Vector2f(thumbSize.x, 0);
                quad[2].position = sf::Vector2f(0, thumbSize.y);
                quad[2].texCoords = sf::Vector2f(0, thumbSize.y);
                quad[3].position = sf::Vector2f(thumbSize.x, thumbSize.y);
                quad[3].texCoords = sf::Vector2f(thumbSize.x, thumbSize.y);

                menuTexture.draw(quad, states);
            } else {
                sf::RectangleShape placeholder;
                placeholder.setPosition({level.position.x + 5, level.position.y + 5});
                placeholder.setSize(sf::Vector2f(thumbnailWidth, thumbnailHeight));
                placeholder.setFillColor(sf::Color::Red);
                placeholder.setOutlineThickness(1);
                placeholder.setOutlineColor(sf::Color::Yellow);
                menuTexture.draw(placeholder);
            }

            if (fontLoaded) {
                menuTexture.draw(level.nameText);
            }
        }
    }

    // Draw header
    menuTexture.draw(headerBackground);
    if (fontLoaded) {
        menuTexture.draw(titleText);
        menuTexture.draw(instructionsText);
    }

    menuTexture.display();

    // Render to target with proper scaling
    sf::Vector2u windowSize = static_cast<sf::RenderWindow&>(target).getSize();
    float scaleX = static_cast<float>(windowSize.x) / TEXTURE_WIDTH;
    float scaleY = static_cast<float>(windowSize.y) / TEXTURE_HEIGHT;
    float scale = std::min(scaleX, scaleY);

    menuSprite.setScale({scale, scale});
    float offsetX = (windowSize.x - TEXTURE_WIDTH * scale) / 2.0f;
    float offsetY = (windowSize.y - TEXTURE_HEIGHT * scale) / 2.0f;
    menuSprite.setPosition({offsetX, offsetY});
    menuSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(TEXTURE_WIDTH), static_cast<int>(TEXTURE_HEIGHT)}));

    target.draw(menuSprite);
}

} // namespace SandSim