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
        
        // In levelmenu.hpp, modify the LevelInfo constructor:
LevelInfo(sf::Font fonttt) : isHovered(false), thumbnailLoaded(false),
    thumbnail(), 
    thumbnailSprite(thumbnail), // Keep this as is for now
    nameText(fonttt) {}
    };
    
    class LevelMenu {
    private:
        std::vector<LevelInfo> levels;
        sf::RenderTexture menuTexture;
        sf::Sprite menuSprite;
        bool fontLoaded;
        
        // Scrolling
        float scrollOffset;
        float maxScrollOffset;
        bool isDragging;
        sf::Vector2f dragStartPos;
        float dragStartOffset;
        
        // Layout constants
        static const int THUMBNAIL_WIDTH = 200;
        static const int THUMBNAIL_HEIGHT = 150;
        static const int THUMBNAIL_MARGIN = 20;
        static const int LEVELS_PER_ROW = 4;
        static const int MENU_PADDING = 50;
        
        // Selection
        int selectedLevel;
        
        // Background
        sf::RectangleShape background;
        sf::Text titleText{fonttt};
        sf::Text instructionsText{fonttt};
        
    public:
        LevelMenu();
        ~LevelMenu() = default;
        
        void loadLevels();
        void update(const sf::Vector2f& mousePos);
        bool handleClick(const sf::Vector2f& mousePos);
        void handleMouseDrag(const sf::Vector2f& mousePos, bool pressed);
        void handleMouseWheel(float delta);
        void render(sf::RenderTarget& target);
        
        int getSelectedLevel() const { return selectedLevel; }
        std::string getSelectedLevelFile() const;
        void resetSelection() { selectedLevel = -1; }
        
    private:
    sf::Font fonttt;
        void generateThumbnail(const std::string& worldFile, sf::Texture& thumbnail);
        void setupLayout();
        void updateScrollBounds();
        bool loadFont();
        sf::Vector2f getLevelPosition(int index) const;
    };
}
