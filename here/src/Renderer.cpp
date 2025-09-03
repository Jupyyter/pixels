#include "Renderer.hpp"
#include <iostream>

namespace SandSim {

Renderer::Renderer() : usePostProcessing(false), 
                       renderTexture(sf::Vector2u(TEXTURE_WIDTH, TEXTURE_HEIGHT)),
                       particleSprite(particleTexture) {
    
    // Create texture for particle data with proper settings
    particleTexture = sf::Texture(sf::Vector2u(TEXTURE_WIDTH, TEXTURE_HEIGHT));
    
    // CRITICAL: Prevent texture repetition/wrapping
    particleTexture.setRepeated(false);
    particleTexture.setSmooth(false); // Pixel art style - no smoothing
    
    // Apply same settings to renderTexture used for post-processing
    const_cast<sf::Texture&>(renderTexture.getTexture()).setRepeated(false);
    const_cast<sf::Texture&>(renderTexture.getTexture()).setSmooth(false);
    
    // Set texture to sprite
    particleSprite.setTexture(particleTexture);
    
    // Initialize shaders
    setupShaders();
}

void Renderer::setupShaders() {
    // Simple blur shader with improved kernel
    const std::string blurVertexShader = R"(
        #version 120
        void main() {
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
            gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
        }
    )";
    
    // Enhanced blur shader with larger kernel for better bloom effect
    const std::string blurFragmentShader = R"(
        #version 120
        uniform sampler2D texture;
        uniform vec2 offset;
        
        void main() {
            vec2 offx = vec2(offset.x, 0.0);
            vec2 offy = vec2(0.0, offset.y);
            vec2 offx2 = vec2(offset.x * 2.0, 0.0);
            vec2 offy2 = vec2(0.0, offset.y * 2.0);
            
            vec4 pixel = texture2D(texture, gl_TexCoord[0].xy) * 6.0;
            
            // First ring
            pixel += texture2D(texture, gl_TexCoord[0].xy - offx) * 4.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offx) * 4.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy - offy) * 4.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offy) * 4.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy - offx - offy) * 2.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy - offx + offy) * 2.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offx - offy) * 2.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offx + offy) * 2.0;
            
            // Second ring for larger bloom
            pixel += texture2D(texture, gl_TexCoord[0].xy - offx2) * 1.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offx2) * 1.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy - offy2) * 1.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offy2) * 1.0;
            
            gl_FragColor = pixel / 32.0;
        }
    )";
    
    // Try to load blur shader
    if (!blurShader.loadFromMemory(blurVertexShader, blurFragmentShader)) {
        std::cerr << "Warning: Could not load blur shader. Post-processing disabled." << std::endl;
        usePostProcessing = false;
    } else {
        blurShader.setUniform("offset", sf::Vector2f(1.0f / TEXTURE_WIDTH, 1.0f / TEXTURE_HEIGHT));
    }
    
    // Enhanced bloom/brightness shader with lower threshold and intensity boost
    const std::string bloomFragmentShader = R"(
        #version 120
        uniform sampler2D texture;
        uniform float threshold;
        uniform float intensity;
        
        void main() {
            vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
            float brightness = dot(pixel.rgb, vec3(0.299, 0.587, 0.114));
            
            if(brightness > threshold) {
                // Boost the bloom intensity for more visible effect
                vec4 bloom = pixel * intensity;
                // Add some color saturation to make bloom more vibrant
                bloom.rgb = mix(vec3(brightness), bloom.rgb, 1.2);
                gl_FragColor = bloom;
            } else {
                gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
            }
        }
    )";
    
    if (!bloomShader.loadFromMemory(blurVertexShader, bloomFragmentShader)) {
        std::cerr << "Warning: Could not load bloom shader. Post-processing disabled." << std::endl;
        usePostProcessing = false;
    } else {
        bloomShader.setUniform("threshold", 0.4f);
        bloomShader.setUniform("intensity", 2.0f);
    }
    
    // Color enhancement shader for better bloom visibility
    const std::string enhanceFragmentShader = R"(
        #version 120
        uniform sampler2D texture;
        uniform float brightness;
        uniform float contrast;
        
        void main() {
            vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
            
            // Apply brightness and contrast
            pixel.rgb = (pixel.rgb - 0.5) * contrast + 0.5 + brightness;
            
            // Enhance bright colors for better bloom effect
            float luminance = dot(pixel.rgb, vec3(0.299, 0.587, 0.114));
            if(luminance > 0.6) {
                pixel.rgb *= 1.1; // Boost bright colors
            }
            
            gl_FragColor = pixel;
        }
    )";
    
    if (!enhanceShader.loadFromMemory(blurVertexShader, enhanceFragmentShader)) {
        std::cerr << "Warning: Could not load enhancement shader." << std::endl;
    } else {
        enhanceShader.setUniform("brightness", 0.05f);
        enhanceShader.setUniform("contrast", 1.1f);
    }
    
    // Only enable post-processing if bloom and blur shaders loaded successfully
    if (!blurShader.isAvailable() || !bloomShader.isAvailable()) {
        usePostProcessing = false;
    }
}

void Renderer::updateTexture(const ParticleWorld& world) {
    // Update texture from particle world pixel buffer
    particleTexture.update(world.getPixelBuffer());
}

void Renderer::render(sf::RenderWindow& window, const ParticleWorld& world) {
    // Update texture with latest particle data
    updateTexture(world);
    
    if (usePostProcessing && blurShader.isAvailable() && bloomShader.isAvailable()) {
        renderWithPostProcessing(window);
    } else {
        renderDirect(window);
    }
}

void Renderer::setUsePostProcessing(bool use) {
    usePostProcessing = use && blurShader.isAvailable() && bloomShader.isAvailable();
}

bool Renderer::getUsePostProcessing() const {
    return usePostProcessing;
}

void Renderer::scaleToWindow(sf::RenderWindow& window) {
    sf::Vector2u windowSize = window.getSize();
    float scaleX = static_cast<float>(windowSize.x) / TEXTURE_WIDTH;
    float scaleY = static_cast<float>(windowSize.y) / TEXTURE_HEIGHT;
    
    // Use the smaller scale to maintain aspect ratio
    float scale = std::min(scaleX, scaleY);
    
    particleSprite.setScale({scale, scale});
    
    // Center the sprite
    float offsetX = (windowSize.x - TEXTURE_WIDTH * scale) / 2.0f;
    float offsetY = (windowSize.y - TEXTURE_HEIGHT * scale) / 2.0f;
    particleSprite.setPosition({offsetX, offsetY});
    
    // CRITICAL: Set exact texture rect to prevent edge bleeding into black bars
    particleSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(TEXTURE_WIDTH), static_cast<int>(TEXTURE_HEIGHT)}));
}

void Renderer::renderDirect(sf::RenderWindow& window) {
    scaleToWindow(window);
    window.draw(particleSprite);
}

void Renderer::renderWithPostProcessing(sf::RenderWindow& window) {
    // Step 1: Render original to texture with slight enhancement
    renderTexture.clear();
    sf::Sprite tempSprite(particleTexture);
    tempSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(TEXTURE_WIDTH), static_cast<int>(TEXTURE_HEIGHT)}));
    
    // Apply enhancement if available
    if (enhanceShader.isAvailable()) {
        renderTexture.draw(tempSprite, &enhanceShader);
    } else {
        renderTexture.draw(tempSprite);
    }
    renderTexture.display();
    
    // Step 2: Extract bright areas for bloom
    sf::RenderTexture bloomTexture(sf::Vector2u(TEXTURE_WIDTH, TEXTURE_HEIGHT));
    const_cast<sf::Texture&>(bloomTexture.getTexture()).setRepeated(false);
    const_cast<sf::Texture&>(bloomTexture.getTexture()).setSmooth(false);
    
    bloomTexture.clear(sf::Color::Transparent);
    sf::Sprite bloomSprite(renderTexture.getTexture());
    bloomSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(TEXTURE_WIDTH), static_cast<int>(TEXTURE_HEIGHT)}));
    bloomTexture.draw(bloomSprite, &bloomShader);
    bloomTexture.display();
    
    // Step 3: Apply multiple blur passes for better bloom spread
    sf::RenderTexture blurTexture1(sf::Vector2u(TEXTURE_WIDTH, TEXTURE_HEIGHT));
    sf::RenderTexture blurTexture2(sf::Vector2u(TEXTURE_WIDTH, TEXTURE_HEIGHT));
    
    const_cast<sf::Texture&>(blurTexture1.getTexture()).setRepeated(false);
    const_cast<sf::Texture&>(blurTexture1.getTexture()).setSmooth(false);
    const_cast<sf::Texture&>(blurTexture2.getTexture()).setRepeated(false);
    const_cast<sf::Texture&>(blurTexture2.getTexture()).setSmooth(false);
    
    // First blur pass
    blurTexture1.clear();
    sf::Sprite blurSprite1(bloomTexture.getTexture());
    blurSprite1.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(TEXTURE_WIDTH), static_cast<int>(TEXTURE_HEIGHT)}));
    blurTexture1.draw(blurSprite1, &blurShader);
    blurTexture1.display();
    
    // Second blur pass for smoother bloom
    blurTexture2.clear();
    sf::Sprite blurSprite2(blurTexture1.getTexture());
    blurSprite2.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(TEXTURE_WIDTH), static_cast<int>(TEXTURE_HEIGHT)}));
    blurTexture2.draw(blurSprite2, &blurShader);
    blurTexture2.display();
    
    // Step 4: Composite original + bloom
    renderTexture.clear();
    
    // Draw original
    sf::Sprite originalSprite(particleTexture);
    originalSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(TEXTURE_WIDTH), static_cast<int>(TEXTURE_HEIGHT)}));
    renderTexture.draw(originalSprite);
    
    // Add bloom with additive blending (stronger effect)
    sf::Sprite bloomedSprite(blurTexture2.getTexture());
    bloomedSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(TEXTURE_WIDTH), static_cast<int>(TEXTURE_HEIGHT)}));
    sf::RenderStates additiveState;
    additiveState.blendMode = sf::BlendAdd;
    renderTexture.draw(bloomedSprite, additiveState);
    
    // Add a second, softer bloom layer for more glow
    sf::Sprite softBloomSprite(blurTexture2.getTexture());
    softBloomSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(TEXTURE_WIDTH), static_cast<int>(TEXTURE_HEIGHT)}));
    sf::RenderStates softAdditiveState;
    softAdditiveState.blendMode = sf::BlendAdd;
    softBloomSprite.setColor(sf::Color(255, 255, 255, 128)); // 50% opacity for softer effect
    renderTexture.draw(softBloomSprite, softAdditiveState);
    
    renderTexture.display();
    
    // Step 5: Draw final result to window
    particleSprite.setTexture(renderTexture.getTexture());
    scaleToWindow(window);
    window.draw(particleSprite);
    
    // Reset texture to original
    particleSprite.setTexture(particleTexture);
}

} // namespace SandSim