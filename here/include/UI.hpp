#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "Constants.hpp"

namespace SandSim {
    enum class MaterialSelection {
        Sand = 0,
        Water,
        Salt,
        Wood,
        Fire,
        Smoke,
        Steam,
        Gunpowder,
        Oil,
        Lava,
        Stone,
        Acid
    };
    
    struct MaterialButton {
        sf::Vector2i position;
        sf::Vector2i size;
        sf::Color color;
        std::string name;
        MaterialID materialID;
        MaterialSelection selection;
    };
    
    class UI {
    private:
        sf::RenderTexture uiTexture;
        sf::Sprite uiSprite;
        
        MaterialSelection currentSelection;
        std::vector<MaterialButton> materialButtons;
        
        bool showMaterialPanel;
        bool showFrameCount;
        bool showSimulationState;
        bool showControls;
        
        sf::Vector2f mousePos;
        float selectionRadius;
        
        sf::Font font;
        sf::Text frameInfoText{font};
        sf::Text materialHoverText{font};
        sf::Text simulationStateText{font};
        sf::Text controlsText{font};
        bool fontLoaded;
        
    public:
        UI();
        
        // Public methods
        void setupMaterialButtons();
        void update(const sf::Vector2f& worldMousePos, float frameTime, bool simulationRunning);
        bool handleClick(const sf::Vector2f& worldMousePos);
        void handleKeyPress(sf::Keyboard::Key key);
        void handleMouseWheel(float delta);
        void render(sf::RenderTarget& target);
        
        // Getters
        MaterialID getCurrentMaterialID() const;
        float getSelectionRadius() const;
        const std::vector<MaterialButton>& getMaterialButtons() const;
        bool getShowMaterialPanel() const;
        
        // Setters
        void setShowMaterialPanel(bool show);
        void setShowFrameCount(bool show);
        void setShowSimulationState(bool show);
        void setShowControls(bool show);
        
    private:
        // Helper methods
        bool isPointInRect(const sf::Vector2f& point, const sf::Vector2i& rectPos, const sf::Vector2i& rectSize) const;
        void drawMaterialPanel();
        void drawSelectionCircle();
        bool loadFont();
    };
}