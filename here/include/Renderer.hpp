#pragma once
#include <SFML/Graphics.hpp>
#include "ParticleWorld.hpp"
#include "Constants.hpp"

namespace SandSim {
    class Renderer {
    private:
        sf::Texture particleTexture;
        sf::Sprite particleSprite;
        
        // Post-processing components
        sf::RenderTexture renderTexture;
        sf::Shader blurShader;
        sf::Shader bloomShader;
        sf::Shader enhanceShader;
        bool usePostProcessing;
        
    public:
        Renderer();
        
        void setupShaders();
        void updateTexture(const ParticleWorld& world);
        void render(sf::RenderWindow& window, const ParticleWorld& world);
        void setUsePostProcessing(bool use);
        bool getUsePostProcessing() const;
        void scaleToWindow(sf::RenderWindow& window);
        
    private:
        void renderDirect(sf::RenderWindow& window);
        void renderWithPostProcessing(sf::RenderWindow& window);
    };
}