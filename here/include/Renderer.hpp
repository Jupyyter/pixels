#pragma once
#include <SFML/Graphics.hpp>
#include "ParticleWorld.hpp"
#include "Constants.hpp"

class Renderer {
private:
    sf::Texture particleTexture;
    sf::Sprite particleSprite;
    std::unordered_map<ChunkCoord, sf::Texture, ChunkCoordHash> chunkTextures[2]; // Layered chunk textures
    // Post-processing components
    sf::RenderTexture renderTexture;
    
    sf::RenderTexture bloomTexture;
    sf::RenderTexture blurTexture1;
    sf::RenderTexture blurTexture2;
    
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
    void setShowChunkBounds(bool show);
    void setShowColliders(bool show);
private:
    bool showChunkBounds = false;
    bool showColliders = false;
    void renderDirect(sf::RenderWindow& window, const ParticleWorld& world);
    void renderWithPostProcessing(sf::RenderWindow& window, const ParticleWorld& world);
};