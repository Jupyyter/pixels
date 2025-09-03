#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <memory>
#include "Constants.hpp"

namespace SandSim {
    struct LevelInfo {
        std::string filename;
        std::string displayName;
        sf::Texture thumbnail;
        sf::Sprite thumbnailSprite;
        sf::Vector2f position;
        sf::RectangleShape background;
        sf::Text nameText;
        bool isHovered;
        bool thumbnailLoaded;
        
        LevelInfo(sf::Font& font) : isHovered(false), thumbnailLoaded(false),
            thumbnail(), 
            thumbnailSprite(thumbnail),
            nameText(font) {}
    };
    
    class LevelMenu {
    private:
        std::vector<LevelInfo> levels;
        sf::RenderTexture menuTexture;
        sf::Sprite menuSprite;
        sf::Font fonttt;
        bool fontLoaded;
        
        // Scrolling
        float scrollOffset;
        float maxScrollOffset;
        bool isDragging;
        sf::Vector2f dragStartPos;
        float dragStartOffset;
        
        // Dynamic layout parameters
        int levelsPerRow;           // Number of levels per row
        float paddingPercent;       // Padding as percentage of available width
        int thumbnailWidth;         // Calculated thumbnail width
        int thumbnailHeight;        // Calculated thumbnail height
        int thumbnailMargin;        // Calculated margin between thumbnails
        int edgePadding;           // Calculated padding from edges
        
        // Fixed constants
        static const int MENU_HEADER_HEIGHT = 60;
        static const int TEXT_AREA_HEIGHT = 30;
        static const float ASPECT_RATIO; // 4:3 aspect ratio for thumbnails
        
        // Selection
        int selectedLevel;
        
        // Background
        sf::RectangleShape background;
        sf::RectangleShape headerBackground;
        sf::Text titleText;
        sf::Text instructionsText;
        
    public:
        LevelMenu(int levelsPerRow = 3, float paddingPercent = 0.1f);
        ~LevelMenu() = default;
        
        void loadLevels();
        void update(const sf::Vector2f& mousePos);
        bool handleClick(const sf::Vector2f& mousePos);
        void handleMouseDrag(const sf::Vector2f& mousePos, bool pressed);
        void handleMouseWheel(float delta);
        void render(sf::RenderTarget& target);
        // Add this method to LevelMenu class
void refreshLevels();
        
        int getSelectedLevel() const { return selectedLevel; }
        std::string getSelectedLevelFile() const;
        void resetSelection() { selectedLevel = -1; }
        
        // Configuration methods
        void setLevelsPerRow(int count);
        void setPaddingPercent(float percent);
        int getLevelsPerRow() const { return levelsPerRow; }
        float getPaddingPercent() const { return paddingPercent; }
        
        // Coordinate conversion
        sf::Vector2f windowToMenuCoords(const sf::Vector2f& windowPos, const sf::RenderWindow& window) const;
        
    private:
        void generateThumbnail(const std::string& worldFile, sf::Texture& thumbnail);
        void calculateLayout();
        void setupLayout();
        void updateScrollBounds();
        bool loadFont();
        sf::Vector2f getLevelPosition(int index) const;
    };
}