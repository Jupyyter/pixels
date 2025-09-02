#include "LevelMenu.hpp"
#include "ParticleWorld.hpp"
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace SandSim
{

    LevelMenu::LevelMenu() : scrollOffset(0), maxScrollOffset(0), isDragging(false),
                             selectedLevel(-1), fontLoaded(false),
                             menuTexture(sf::Vector2u(TEXTURE_WIDTH, TEXTURE_HEIGHT)),
                             menuSprite(menuTexture.getTexture())
    {

        fontLoaded = loadFont();

        // Setup background
        background.setSize(sf::Vector2f(TEXTURE_WIDTH, TEXTURE_HEIGHT));
        background.setFillColor(sf::Color(30, 30, 40));

        if (fontLoaded)
        {
            // Setup title
            titleText.setFont(fonttt);
            titleText.setCharacterSize(48);
            titleText.setFillColor(sf::Color::White);
            titleText.setString("Select Level");

            sf::FloatRect titleBounds = titleText.getLocalBounds();
            titleText.setPosition(sf::Vector2f((TEXTURE_WIDTH - titleBounds.size.x) / 2.0f, 30));

            // Setup instructions
            instructionsText.setFont(fonttt);
            instructionsText.setCharacterSize(18);
            instructionsText.setFillColor(sf::Color(200, 200, 200));
            instructionsText.setString("Click on a level to start playing");

            sf::FloatRect instrBounds = instructionsText.getLocalBounds();
            instructionsText.setPosition(sf::Vector2f((TEXTURE_WIDTH - instrBounds.size.x) / 2.0f, 90));
        }

        loadLevels();
        setupLayout();
    }

    bool LevelMenu::loadFont()
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
            if (fonttt.openFromFile(path))
            {
                std::cout << "Font loaded successfully from: " << path << std::endl;
                return true;
            }
        }

        std::cerr << "Warning: Could not load any font. Text will not display properly." << std::endl;
        std::cerr << "Please ensure you have a font file in one of these locations:" << std::endl;
        for (const auto &path : fontPaths)
        {
            std::cerr << "  - " << path << std::endl;
        }

        return false;
    }

    void LevelMenu::loadLevels()
    {
        levels.clear();

        // Scan for .rrr files
        try
        {
            for (const auto &entry : std::filesystem::directory_iterator("."))
            {
                if (entry.path().extension() == ".rrr")
                {
                    LevelInfo level(fonttt);
                    level.filename = entry.path().string();
                    level.displayName = entry.path().stem().string(); // Filename without extension

                    // Generate thumbnail
                    // In loadLevels(), after generateThumbnail():
                    generateThumbnail(level.filename, level.thumbnail);

                    // Check if thumbnail was generated successfully
                    sf::Vector2u thumbSize = level.thumbnail.getSize();
                    std::cout << "Thumbnail size for " << level.displayName << ": " << thumbSize.x << "x" << thumbSize.y << std::endl;

                    level.thumbnailLoaded = (thumbSize.x > 0 && thumbSize.y > 0);
                    // In loadLevels(), after setting up the thumbnail sprite:
                    // In loadLevels(), when setting up the sprite:
                    // In loadLevels():
                    if (level.thumbnailLoaded)
                    {
                        level.thumbnailSprite.setTexture(level.thumbnail);
                        level.thumbnailSprite.setColor(sf::Color::White); // Ensure no color tinting

                        // Scale to fit
                        sf::Vector2u thumbSize = level.thumbnail.getSize();
                        float scaleX = static_cast<float>(THUMBNAIL_WIDTH) / thumbSize.x;
                        float scaleY = static_cast<float>(THUMBNAIL_HEIGHT) / thumbSize.y;
                        float scale = std::min(scaleX, scaleY);
                        level.thumbnailSprite.setScale({scale, scale});
                    }
                    else
                    {
                        std::cout << "Thumbnail NOT loaded for " << level.displayName << std::endl;
                    }

                    if (level.thumbnailLoaded)
                    {
                        level.thumbnailSprite = sf::Sprite(level.thumbnail); // Create sprite with texture
                        level.thumbnailSprite.setColor(sf::Color::White);    // Ensure white tint

                        // Scale and position as before
                        float scaleX = static_cast<float>(THUMBNAIL_WIDTH) / thumbSize.x;
                        float scaleY = static_cast<float>(THUMBNAIL_HEIGHT) / thumbSize.y;
                        float scale = std::min(scaleX, scaleY);
                        level.thumbnailSprite.setScale({scale, scale});
                    }
                    else
                    {
                        std::cout << "Failed to load thumbnail for " << level.displayName << std::endl;
                    }

                    // Setup background
                    level.background.setSize(sf::Vector2f(THUMBNAIL_WIDTH + 10, THUMBNAIL_HEIGHT + 40));
                    level.background.setFillColor(sf::Color(50, 50, 60));
                    level.background.setOutlineThickness(2);
                    level.background.setOutlineColor(sf::Color(70, 70, 80));

                    // Setup text
                    if (fontLoaded)
                    {
                        level.nameText.setFont(fonttt);
                        level.nameText.setCharacterSize(16);
                        level.nameText.setFillColor(sf::Color::White);
                        level.nameText.setString(level.displayName);
                    }

                    levels.push_back(std::move(level));
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error loading levels: " << e.what() << std::endl;
        }

        std::cout << "Loaded " << levels.size() << " levels" << std::endl;
    }

// In your src/LevelMenu.cpp file:


    void LevelMenu::generateThumbnail(const std::string &worldFile, sf::Texture &thumbnail)
    {
        try
        {
            ParticleWorld tempWorld(TEXTURE_WIDTH, TEXTURE_HEIGHT);

            if (tempWorld.loadWorld(worldFile))
            {
                const std::uint8_t *pixelBuffer = tempWorld.getPixelBuffer();

                if (pixelBuffer)
                {
                    sf::Vector2u imageSize(tempWorld.getWidth(), tempWorld.getHeight());

                    // Use the ACTUAL world data, not test pattern
                    std::vector<std::uint8_t> fixedPixelBuffer(imageSize.x * imageSize.y * 4);
                    for (unsigned int i = 0; i < imageSize.x * imageSize.y; ++i)
                    {
                        unsigned int idx = i * 4;

                        fixedPixelBuffer[idx] = pixelBuffer[idx];         // R
                        fixedPixelBuffer[idx + 1] = pixelBuffer[idx + 1]; // G
                        fixedPixelBuffer[idx + 2] = pixelBuffer[idx + 2]; // B

                        // Set alpha based on whether pixel has content
                        if (pixelBuffer[idx] == 0 && pixelBuffer[idx + 1] == 0 && pixelBuffer[idx + 2] == 0)
                        {
                            fixedPixelBuffer[idx + 3] = 0; // Keep empty pixels transparent
                        }
                        else
                        {
                            fixedPixelBuffer[idx + 3] = 255; // Make non-empty pixels fully opaque
                        }
                    }

                    sf::Image worldImage(imageSize, fixedPixelBuffer.data());

                    thumbnail.loadFromImage(worldImage);
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error generating thumbnail: " << e.what() << std::endl;
        }
    }
    sf::Vector2f LevelMenu::getLevelPosition(int index) const
    {
        int row = index / LEVELS_PER_ROW;
        int col = index % LEVELS_PER_ROW;

        float x = MENU_PADDING + col * (THUMBNAIL_WIDTH + THUMBNAIL_MARGIN);
        float y = 150 + row * (THUMBNAIL_HEIGHT + 60) + scrollOffset; // 60 for text space

        return sf::Vector2f(x, y);
    }

    void LevelMenu::setupLayout()
    {
        for (size_t i = 0; i < levels.size(); ++i)
        {
            sf::Vector2f pos = getLevelPosition(static_cast<int>(i));
            levels[i].position = pos;
            levels[i].background.setPosition(pos);
            levels[i].thumbnailSprite.setPosition({pos.x + 5, pos.y + 5});

            if (fontLoaded)
            {
                sf::FloatRect textBounds = levels[i].nameText.getLocalBounds();
                levels[i].nameText.setPosition(
                    sf::Vector2f(
                        pos.x + (THUMBNAIL_WIDTH - textBounds.size.x) / 2.0f,
                        pos.y + THUMBNAIL_HEIGHT + 10.0f));
            }
        }

        updateScrollBounds();
    }

    void LevelMenu::updateScrollBounds()
    {
        if (levels.empty())
        {
            maxScrollOffset = 0;
            return;
        }

        int totalRows = (static_cast<int>(levels.size()) + LEVELS_PER_ROW - 1) / LEVELS_PER_ROW;
        float totalHeight = totalRows * (THUMBNAIL_HEIGHT + 60);
        float visibleHeight = TEXTURE_HEIGHT - 200; // Account for title and padding

        maxScrollOffset = std::max(0.0f, totalHeight - visibleHeight);
    }

    void LevelMenu::update(const sf::Vector2f &mousePos)
    {
        // Update hover states
        for (auto &level : levels)
        {
            sf::FloatRect bounds = level.background.getGlobalBounds();
            level.isHovered = bounds.contains(mousePos);

            if (level.isHovered)
            {
                level.background.setFillColor(sf::Color(70, 70, 90));
                level.background.setOutlineColor(sf::Color(100, 150, 200));
            }
            else
            {
                level.background.setFillColor(sf::Color(50, 50, 60));
                level.background.setOutlineColor(sf::Color(70, 70, 80));
            }
        }
    }

    bool LevelMenu::handleClick(const sf::Vector2f &mousePos)
    {
        for (size_t i = 0; i < levels.size(); ++i)
        {
            sf::FloatRect bounds = levels[i].background.getGlobalBounds();
            if (bounds.contains(mousePos))
            {
                selectedLevel = static_cast<int>(i);
                std::cout << "Selected level: " << levels[i].displayName << std::endl;
                return true; // Level selected
            }
        }
        return false;
    }

    void LevelMenu::handleMouseDrag(const sf::Vector2f &mousePos, bool pressed)
    {
        if (pressed && !isDragging)
        {
            isDragging = true;
            dragStartPos = mousePos;
            dragStartOffset = scrollOffset;
        }
        else if (!pressed)
        {
            isDragging = false;
        }

        if (isDragging)
        {
            float deltaY = mousePos.y - dragStartPos.y;
            scrollOffset = std::clamp(dragStartOffset + deltaY, -maxScrollOffset, 0.0f);
            setupLayout(); // Update positions
        }
    }

    void LevelMenu::handleMouseWheel(float delta)
    {
        scrollOffset = std::clamp(scrollOffset + delta * 30.0f, -maxScrollOffset, 0.0f);
        setupLayout();
    }

    std::string LevelMenu::getSelectedLevelFile() const
    {
        if (selectedLevel >= 0 && selectedLevel < static_cast<int>(levels.size()))
        {
            return levels[selectedLevel].filename;
        }
        return "";
    }

    void LevelMenu::render(sf::RenderTarget &target)
    {
        menuTexture.clear(sf::Color::Transparent);

        // Draw background
        menuTexture.draw(background);

        // Draw title and instructions
        if (fontLoaded)
        {
            menuTexture.draw(titleText);
            menuTexture.draw(instructionsText);
        }

        // Draw levels
        for (const auto &level : levels)
        {
            // Only draw if visible
            if (level.position.y + THUMBNAIL_HEIGHT > 0 && level.position.y < TEXTURE_HEIGHT)
            {
                menuTexture.draw(level.background);

                // In the render function, instead of drawing the sprite, draw the texture directly:
                // This code is already valid for SFML 3.
                if (level.thumbnailLoaded)
                {
                    // A RenderStates object allows you to bundle transform, texture, shader,
                    // and blend mode for a custom draw call. This is the correct SFML 3 approach.
                    sf::RenderStates states;

                    // The sf::Transform class is used to build a transformation matrix.
                    sf::Transform transform;

                    // --- 1. Calculate Position and Scale (No change) ---
                    sf::Vector2u thumbSize = level.thumbnail.getSize();
                    float scaleX = static_cast<float>(THUMBNAIL_WIDTH) / thumbSize.x;
                    float scaleY = static_cast<float>(THUMBNAIL_HEIGHT) / thumbSize.y;
                    float scale = std::min(scaleX, scaleY);

                    // --- 2. Build the Transform (No change) ---
                    transform.translate({level.position.x + 5, level.position.y + 5});
                    transform.scale({scale, scale});

                    // --- 3. Configure Render States (No change) ---
                    states.transform = transform;
                    states.texture = &level.thumbnail; // Assign the texture to be used

                    // --- 4. Define the Geometry (No change) ---
                    // A VertexArray holds the geometry to be drawn. Here, it's a quad.
                    sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);

                    // Define the corners of the quad and their corresponding texture coordinates.
                    quad[0].position = sf::Vector2f(0, 0);
                    quad[0].texCoords = sf::Vector2f(0, 0);

                    quad[1].position = sf::Vector2f(thumbSize.x, 0);
                    quad[1].texCoords = sf::Vector2f(thumbSize.x, 0);

                    quad[2].position = sf::Vector2f(0, thumbSize.y);
                    quad[2].texCoords = sf::Vector2f(0, thumbSize.y);

                    quad[3].position = sf::Vector2f(thumbSize.x, thumbSize.y);
                    quad[3].texCoords = sf::Vector2f(thumbSize.x, thumbSize.y);

                    // --- 5. Draw the VertexArray (No change) ---
                    // The draw call takes the geometry (quad) and the states that define how to render it.
                    menuTexture.draw(quad, states);
                }
                else
                {
                    // Draw placeholder with a different color to distinguish
                    sf::RectangleShape placeholder;
                    placeholder.setPosition(level.thumbnailSprite.getPosition());
                    placeholder.setSize(sf::Vector2f(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT));
                    placeholder.setFillColor(sf::Color::Red); // Make it red instead of gray
                    placeholder.setOutlineThickness(1);
                    placeholder.setOutlineColor(sf::Color::Yellow);
                    menuTexture.draw(placeholder);

                    std::cout << "Drawing placeholder for " << level.displayName << std::endl;
                }

                if (fontLoaded)
                {
                    menuTexture.draw(level.nameText);
                }
            }
        }

        menuTexture.display();

        // Scale and position like the game world
        sf::Vector2u windowSize = static_cast<sf::RenderWindow &>(target).getSize();
        float scaleX = static_cast<float>(windowSize.x) / TEXTURE_WIDTH;
        float scaleY = static_cast<float>(windowSize.y) / TEXTURE_HEIGHT;
        float scale = std::min(scaleX, scaleY);

        menuSprite.setScale({scale, scale});
        float offsetX = (windowSize.x - TEXTURE_WIDTH * scale) / 2.0f;
        float offsetY = (windowSize.y - TEXTURE_HEIGHT * scale) / 2.0f;
        menuSprite.setPosition({offsetX, offsetY});

        target.draw(menuSprite);
    }

} // namespace SandSim