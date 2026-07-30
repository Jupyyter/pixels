#include "Renderer.hpp"
#include <iostream>

Renderer::Renderer()
    : particleTexture(sf::Vector2u(VIEW_WIDTH, VIEW_HEIGHT)),
      particleSprite(particleTexture),
      renderTexture(sf::Vector2u(VIEW_WIDTH, VIEW_HEIGHT)),
      bloomTexture(sf::Vector2u(VIEW_WIDTH, VIEW_HEIGHT)),
      blurTexture1(sf::Vector2u(VIEW_WIDTH, VIEW_HEIGHT)),
      blurTexture2(sf::Vector2u(VIEW_WIDTH, VIEW_HEIGHT)),
      usePostProcessing(false)
{
    particleTexture.setRepeated(false);
    particleTexture.setSmooth(false);

    renderTexture.setRepeated(false);
    renderTexture.setSmooth(false);

    bloomTexture.setRepeated(false);
    bloomTexture.setSmooth(false);

    blurTexture1.setRepeated(false);
    blurTexture1.setSmooth(false);

    blurTexture2.setRepeated(false);
    blurTexture2.setSmooth(false);

    setupShaders();
}

void Renderer::setupShaders()
{
    const std::string blurVertexShader = R"(
        #version 120
        void main() {
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
            gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
        }
    )";

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
            
            pixel += texture2D(texture, gl_TexCoord[0].xy - offx) * 4.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offx) * 4.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy - offy) * 4.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offy) * 4.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy - offx - offy) * 2.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy - offx + offy) * 2.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offx - offy) * 2.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offx + offy) * 2.0;
            
            pixel += texture2D(texture, gl_TexCoord[0].xy - offx2) * 1.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offx2) * 1.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy - offy2) * 1.0;
            pixel += texture2D(texture, gl_TexCoord[0].xy + offy2) * 1.0;
            
            gl_FragColor = pixel / 32.0;
        }
    )";

    if (!blurShader.loadFromMemory(blurVertexShader, blurFragmentShader))
    {
        std::cerr << "Warning: Could not load blur shader. Post-processing disabled." << std::endl;
        usePostProcessing = false;
    }
    else
    {
        blurShader.setUniform("offset", sf::Vector2f(1.0f / VIEW_WIDTH, 1.0f / VIEW_HEIGHT));
    }

    const std::string bloomFragmentShader = R"(
        #version 120
        uniform sampler2D texture;
        uniform float threshold;
        uniform float intensity;
        
        void main() {
            vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
            float brightness = dot(pixel.rgb, vec3(0.299, 0.587, 0.114));
            
            if(brightness > threshold) {
                vec4 bloom = pixel * intensity;
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

    const std::string enhanceFragmentShader = R"(
        #version 120
        uniform sampler2D texture;
        uniform float brightness;
        uniform float contrast;
        
        void main() {
            vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
            pixel.rgb = (pixel.rgb - 0.5) * contrast + 0.5 + brightness;
            float luminance = dot(pixel.rgb, vec3(0.299, 0.587, 0.114));
            if(luminance > 0.6) {
                pixel.rgb *= 1.1;
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

    if (!blurShader.isAvailable() || !bloomShader.isAvailable())
    {
        usePostProcessing = false;
    }
}

void Renderer::updateTexture(const ParticleWorld &world)
{
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
    sf::FloatRect cameraView = world.getRenderBounds();
    for (int l = 1; l >= 0; --l) {
        for (const auto &[coord, chunk] : world.getActiveChunks(l))
        {
            if (!chunk->isActive)
                continue;
                
            sf::FloatRect chunkRect(
                {static_cast<float>(coord.x * CHUNK_SIZE), static_cast<float>(coord.y * CHUNK_SIZE)},
                {static_cast<float>(CHUNK_SIZE), static_cast<float>(CHUNK_SIZE)}
            );

            if (!cameraView.findIntersection(chunkRect)) continue;
            
            auto [it, inserted] = chunkTextures[l].try_emplace(coord, sf::Vector2u(CHUNK_SIZE, CHUNK_SIZE));
            sf::Texture &tex = it->second;

            if (inserted) tex.setSmooth(false);

            if (chunk->visualDirty)
            {
                tex.update(chunk->pixelData.data());
                chunk->visualDirty = false;
            }

            sf::Sprite sprite(tex);
            sprite.setPosition(sf::Vector2f(coord.x * CHUNK_SIZE, coord.y * CHUNK_SIZE));
            if (l == 1) sprite.setColor(sf::Color(128, 128, 128, 255));
            window.draw(sprite);

            if (showChunkBounds) {
                sf::RectangleShape outline(sf::Vector2f(CHUNK_SIZE, CHUNK_SIZE));
                outline.setPosition(sf::Vector2f(coord.x * CHUNK_SIZE, coord.y * CHUNK_SIZE));
                outline.setFillColor(sf::Color::Transparent);
                outline.setOutlineColor(sf::Color(255, 0, 0, 100));
                float thickness = window.getView().getSize().x / window.getSize().x;
                outline.setOutlineThickness(thickness);
                window.draw(outline);
                
                if (chunk->activeMaxX >= chunk->activeMinX && chunk->activeMaxY >= chunk->activeMinY && !chunk->isSleeping) {
                    float w = chunk->activeMaxX - chunk->activeMinX + 1;
                    float h = chunk->activeMaxY - chunk->activeMinY + 1;
                    sf::RectangleShape activeOutline(sf::Vector2f(w, h));
                    activeOutline.setPosition(sf::Vector2f(coord.x * CHUNK_SIZE + chunk->activeMinX, coord.y * CHUNK_SIZE + chunk->activeMinY));
                    activeOutline.setFillColor(sf::Color::Transparent);
                    activeOutline.setOutlineColor(sf::Color(255, 255, 0, 150));
                    activeOutline.setOutlineThickness(thickness);
                    window.draw(activeOutline);
                }
            }
        }
    }
    if (showColliders) world.renderDebugColliders(window);
}

void Renderer::renderWithPostProcessing(sf::RenderWindow &window, const ParticleWorld &world)
{
    renderTexture.setView(window.getView());
    renderTexture.clear(sf::Color::Black);

    sf::FloatRect cameraView = world.getRenderBounds();

    for (int l = 1; l >= 0; --l) {
        for (const auto &[coord, chunk] : world.getActiveChunks(l))
        {
            if (!chunk->isActive) continue;

            sf::FloatRect chunkRect(
                {static_cast<float>(coord.x * CHUNK_SIZE), static_cast<float>(coord.y * CHUNK_SIZE)},
                {static_cast<float>(CHUNK_SIZE), static_cast<float>(CHUNK_SIZE)}
            );

            if (!cameraView.findIntersection(chunkRect)) continue;

            auto[it, inserted] = chunkTextures[l].try_emplace(coord, sf::Vector2u(CHUNK_SIZE, CHUNK_SIZE));
            sf::Texture &tex = it->second;
            if (inserted) tex.setSmooth(false);

            if (chunk->visualDirty) {
                tex.update(chunk->pixelData.data());
                chunk->visualDirty = false;
            }

            sf::Sprite sprite(tex);
            sprite.setPosition(sf::Vector2f(coord.x * CHUNK_SIZE, coord.y * CHUNK_SIZE));
            if (l == 1) sprite.setColor(sf::Color(128, 128, 128, 255));
            renderTexture.draw(sprite);
        }
    }

    if (showColliders) world.renderDebugColliders(renderTexture);
    renderTexture.display();

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

    renderTexture.setView(screenView);
    sf::Sprite bloomedSprite(blurTexture2.getTexture());
    sf::RenderStates additiveState;
    additiveState.blendMode = sf::BlendAdd;
    renderTexture.draw(bloomedSprite, additiveState);

    sf::Sprite softBloomSprite(blurTexture2.getTexture());
    softBloomSprite.setColor(sf::Color(255, 255, 255, 128));
    renderTexture.draw(softBloomSprite, additiveState);
    renderTexture.display();

    sf::View oldView = window.getView();
    window.setView(window.getDefaultView());
    sf::Sprite finalSprite(renderTexture.getTexture());
    float scaleX = static_cast<float>(window.getSize().x) / renderTexture.getSize().x;
    float scaleY = static_cast<float>(window.getSize().y) / renderTexture.getSize().y;
    finalSprite.setScale(sf::Vector2f(scaleX, scaleY));
    window.draw(finalSprite);
    window.setView(oldView);
}
