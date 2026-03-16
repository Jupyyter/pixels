#include "Renderer.hpp"
#include <iostream>

Renderer::Renderer()
    // --- THIS IS THE INITIALIZER LIST ---
    // In SFML 3, objects MUST be fully initialized here before the constructor body runs.
    : particleTexture(sf::Vector2u(VIEW_WIDTH, VIEW_HEIGHT)),
      particleSprite(particleTexture), // Sprite is immediately given the texture
      renderTexture(sf::Vector2u(VIEW_WIDTH, VIEW_HEIGHT)),
      bloomTexture(sf::Vector2u(VIEW_WIDTH, VIEW_HEIGHT)),
      blurTexture1(sf::Vector2u(VIEW_WIDTH, VIEW_HEIGHT)),
      blurTexture2(sf::Vector2u(VIEW_WIDTH, VIEW_HEIGHT)),
      usePostProcessing(false)
{
    // Prevent texture repetition/wrapping and keep the pixel art style crisp
    particleTexture.setRepeated(false);
    particleTexture.setSmooth(false);

    // Apply settings directly to render textures (no need for the old const_cast hack in SFML 3)
    renderTexture.setRepeated(false);
    renderTexture.setSmooth(false);

    bloomTexture.setRepeated(false);
    bloomTexture.setSmooth(false);

    blurTexture1.setRepeated(false);
    blurTexture1.setSmooth(false);

    blurTexture2.setRepeated(false);
    blurTexture2.setSmooth(false);

    // Note: particleSprite.setTexture() is no longer needed here
    // because we already assigned it in the initializer list above!

    setupShaders();
}

void Renderer::setupShaders()
{
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
    if (!blurShader.loadFromMemory(blurVertexShader, blurFragmentShader))
    {
        std::cerr << "Warning: Could not load blur shader. Post-processing disabled." << std::endl;
        usePostProcessing = false;
    }
    else
    {
        // Offset uses the VIEW size now
        blurShader.setUniform("offset", sf::Vector2f(1.0f / VIEW_WIDTH, 1.0f / VIEW_HEIGHT));
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

    if (!bloomShader.loadFromMemory(blurVertexShader, bloomFragmentShader))
    {
        std::cerr << "Warning: Could not load bloom shader. Post-processing disabled." << std::endl;
        usePostProcessing = false;
    }
    else
    {
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

    if (!enhanceShader.loadFromMemory(blurVertexShader, enhanceFragmentShader))
    {
        std::cerr << "Warning: Could not load enhancement shader." << std::endl;
    }
    else
    {
        enhanceShader.setUniform("brightness", 0.05f);
        enhanceShader.setUniform("contrast", 1.1f);
    }

    // Only enable post-processing if bloom and blur shaders loaded successfully
    if (!blurShader.isAvailable() || !bloomShader.isAvailable())
    {
        usePostProcessing = false;
    }
}

void Renderer::updateTexture(const ParticleWorld &world)
{
    // Because both particleTexture and world's pixelBuffer are now VIEW_WIDTH * VIEW_HEIGHT,
    // this will safely copy memory without crashing!
    particleTexture.update(world.getPixelBuffer());
}

void Renderer::render(sf::RenderWindow &window, const ParticleWorld &world)
{
    if (usePostProcessing && blurShader.isAvailable() && bloomShader.isAvailable())
    {
        renderWithPostProcessing(window, world);
    }
    else
    {
        renderDirect(window, world);
    }
}

void Renderer::setUsePostProcessing(bool use)
{
    usePostProcessing = use && blurShader.isAvailable() && bloomShader.isAvailable();
}

bool Renderer::getUsePostProcessing() const
{
    return usePostProcessing;
}

void Renderer::scaleToWindow(sf::RenderWindow &window)
{
    particleSprite.setScale({1.f, 1.f});
    particleSprite.setPosition({0.f, 0.f});

    particleSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(VIEW_WIDTH), static_cast<int>(VIEW_HEIGHT)}));
}
void Renderer::setShowChunkBounds(bool show) { showChunkBounds = show; }
void Renderer::setShowColliders(bool show) { showColliders = show; }
void Renderer::renderDirect(sf::RenderWindow &window, const ParticleWorld &world)
{
    // SFML handles zooming completely automatically based on the View!
    sf::FloatRect cameraView = world.getRenderBounds();
    for (const auto &[coord, chunk] : world.getActiveChunks())
    {
        if (!chunk->isActive)
            continue;
            
        sf::FloatRect chunkRect(
            {static_cast<float>(coord.x * CHUNK_SIZE), static_cast<float>(coord.y * CHUNK_SIZE)}, // Position
            {static_cast<float>(CHUNK_SIZE), static_cast<float>(CHUNK_SIZE)}                      // Size
        );

        // If the chunk is completely off-screen, don't draw it!
        if (!cameraView.findIntersection(chunkRect))
        {
            continue;
        }
        
        // Ensure texture exists for this chunk
        auto [it, inserted] = chunkTextures.try_emplace(coord, sf::Vector2u(CHUNK_SIZE, CHUNK_SIZE));
        sf::Texture &tex = it->second;

        if (inserted)
            tex.setSmooth(false); // Pixel art style

        // Only push data to GPU if it actually changed
        if (chunk->visualDirty)
        {
            tex.update(chunk->pixelData.data());
            chunk->visualDirty = false;
        }

        // Draw it exactly where it belongs in world coordinates
        sf::Sprite sprite(tex);
        sprite.setPosition(sf::Vector2f(coord.x * CHUNK_SIZE, coord.y * CHUNK_SIZE));
        window.draw(sprite);

        if (showChunkBounds) {
            // Draw Red Line bounds perfectly
            sf::RectangleShape outline(sf::Vector2f(CHUNK_SIZE, CHUNK_SIZE));
            outline.setPosition(sf::Vector2f(coord.x * CHUNK_SIZE, coord.y * CHUNK_SIZE));
            outline.setFillColor(sf::Color::Transparent);
            outline.setOutlineColor(sf::Color(255, 0, 0, 100));

            // Keep outline 1 pixel thick regardless of zoom
            float thickness = window.getView().getSize().x / window.getSize().x;
            outline.setOutlineThickness(thickness);
            window.draw(outline);

            // Draw Yellow Active Bounding Box
            if (!chunk->isSleeping) {
                float activeWidth = chunk->activeMaxX - chunk->activeMinX + 1;
                float activeHeight = chunk->activeMaxY - chunk->activeMinY + 1;
                
                sf::RectangleShape activeOutline(sf::Vector2f(activeWidth, activeHeight));
                activeOutline.setPosition(sf::Vector2f(
                    coord.x * CHUNK_SIZE + chunk->activeMinX, 
                    coord.y * CHUNK_SIZE + chunk->activeMinY
                ));
                activeOutline.setFillColor(sf::Color::Transparent);
                activeOutline.setOutlineColor(sf::Color(255, 255, 0, 200)); // Yellow
                activeOutline.setOutlineThickness(thickness);
                window.draw(activeOutline);
            }
        }
    }
    
    // Draw Box2D Colliders over the chunks
    if (showColliders) {
        world.renderDebugColliders(window);
    }
}

void Renderer::renderWithPostProcessing(sf::RenderWindow &window, const ParticleWorld &world)
{
    // 1. Draw chunks to renderTexture using Game View (Handles Zooming)
    renderTexture.setView(window.getView());
    renderTexture.clear(sf::Color::Black);

    sf::FloatRect cameraView = world.getRenderBounds();

    for (const auto &[coord, chunk] : world.getActiveChunks())
    {
        if (!chunk->isActive)
            continue;

        sf::FloatRect chunkRect(
            {static_cast<float>(coord.x * CHUNK_SIZE), static_cast<float>(coord.y * CHUNK_SIZE)},
            {static_cast<float>(CHUNK_SIZE), static_cast<float>(CHUNK_SIZE)}
        );

        if (!cameraView.findIntersection(chunkRect))
        {
            continue;
        }

        auto[it, inserted] = chunkTextures.try_emplace(coord, sf::Vector2u(CHUNK_SIZE, CHUNK_SIZE));
        sf::Texture &tex = it->second;
        if (inserted)
            tex.setSmooth(false);

        if (chunk->visualDirty)
        {
            tex.update(chunk->pixelData.data());
            chunk->visualDirty = false;
        }

        sf::Sprite sprite(tex);
        sprite.setPosition(sf::Vector2f(coord.x * CHUNK_SIZE, coord.y * CHUNK_SIZE));
        renderTexture.draw(sprite);

        if (showChunkBounds) {
            // Draw Red Line bounds perfectly
            sf::RectangleShape outline(sf::Vector2f(CHUNK_SIZE, CHUNK_SIZE));
            outline.setPosition(sf::Vector2f(coord.x * CHUNK_SIZE, coord.y * CHUNK_SIZE));
            outline.setFillColor(sf::Color::Transparent);
            outline.setOutlineColor(sf::Color(255, 0, 0, 100));
            
            float thickness = renderTexture.getView().getSize().x / renderTexture.getSize().x;
            outline.setOutlineThickness(thickness);
            renderTexture.draw(outline);

            if (!chunk->isSleeping) {
                float activeWidth = chunk->activeMaxX - chunk->activeMinX + 1;
                float activeHeight = chunk->activeMaxY - chunk->activeMinY + 1;
                
                sf::RectangleShape activeOutline(sf::Vector2f(activeWidth, activeHeight));
                activeOutline.setPosition(sf::Vector2f(
                    coord.x * CHUNK_SIZE + chunk->activeMinX, 
                    coord.y * CHUNK_SIZE + chunk->activeMinY
                ));
                activeOutline.setFillColor(sf::Color::Transparent);
                activeOutline.setOutlineColor(sf::Color(255, 255, 0, 200)); // Yellow
                activeOutline.setOutlineThickness(thickness);
                renderTexture.draw(activeOutline);
            }
        }
    }

    // Draw Box2D Colliders over the chunks before applying post-processing
    if (showColliders) {
        world.renderDebugColliders(renderTexture);
    }

    renderTexture.display();

    // 2. Run post processing on the flat output
    sf::View screenView = renderTexture.getDefaultView();

    bloomTexture.setView(screenView);
    bloomTexture.clear(sf::Color::Transparent);
    sf::Sprite bloomSprite(renderTexture.getTexture());
    bloomTexture.draw(bloomSprite, &bloomShader);
    bloomTexture.display();

    blurTexture1.setView(screenView);
    blurTexture1.clear(sf::Color::Transparent);
    sf::Sprite blurSprite1(bloomTexture.getTexture());
    blurTexture1.draw(blurSprite1, &blurShader);
    blurTexture1.display();

    blurTexture2.setView(screenView);
    blurTexture2.clear(sf::Color::Transparent);
    sf::Sprite blurSprite2(blurTexture1.getTexture());
    blurTexture2.draw(blurSprite2, &blurShader);
    blurTexture2.display();

    // Composite bloom atop original
    renderTexture.setView(screenView);
    sf::Sprite bloomedSprite(blurTexture2.getTexture());
    sf::RenderStates additiveState;
    additiveState.blendMode = sf::BlendAdd;
    renderTexture.draw(bloomedSprite, additiveState);

    sf::Sprite softBloomSprite(blurTexture2.getTexture());
    softBloomSprite.setColor(sf::Color(255, 255, 255, 128));
    renderTexture.draw(softBloomSprite, additiveState);
    renderTexture.display();

    // 3. Draw final result to the actual window
    sf::View oldView = window.getView();
    window.setView(window.getDefaultView()); // Reset View so texture maps 1:1

    sf::Sprite finalSprite(renderTexture.getTexture());
    float scaleX = static_cast<float>(window.getSize().x) / renderTexture.getSize().x;
    float scaleY = static_cast<float>(window.getSize().y) / renderTexture.getSize().y;
    finalSprite.setScale(sf::Vector2f(scaleX, scaleY));

    window.draw(finalSprite);
    window.setView(oldView); // Restore game view
}