#include "UI.hpp"
#include "ParticleWorld.hpp"
#include "Entities/EntitySystem.hpp"
#include <imgui.h>
#include <imgui-SFML.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include "Particles/ParticleDef.hpp"


UI::UI(sf::RenderWindow& window, ParticleWorld* worldPtr) : world(worldPtr) {
    loadImageAssets();
    loadWeaponAssets();
    loadEntityAssets();
}

void UI::loadImageAssets() {
    imageAssets.clear();
    auto loadFromDir = [&](const std::string& folderPath) {
        if (!std::filesystem::exists(folderPath)) std::filesystem::create_directories(folderPath);
        for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                std::string ext = entry.path().extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                    ImageAsset asset;
                    asset.name = entry.path().stem().string();
                    asset.path = path;
                    if (asset.image.loadFromFile(path)) {
                        if (asset.texture.loadFromImage(asset.image)) {
                            imageAssets.push_back(std::move(asset));
                        }
                    }
                }
            }
        }
    };
    loadFromDir("assets/images/rigidBodies");
    loadFromDir("assets/images/structures");
}

void UI::loadWeaponAssets() {
    weaponAssets.clear();
    std::string folderPath = "assets/images/weapons";
    if (!std::filesystem::exists(folderPath)) std::filesystem::create_directories(folderPath);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            std::string path = entry.path().string();
            std::string ext = entry.path().extension().string();
            if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                std::string stem = entry.path().stem().string();
                if (stem.find("[SHOOT") != std::string::npos) continue; 
                ImageAsset asset;
                asset.name = stem;
                asset.path = path;
                if (asset.image.loadFromFile(path)) {
                    if (asset.texture.loadFromImage(asset.image)) {
                        weaponAssets.push_back(std::move(asset));
                    }
                }
            }
        }
    }
}

void UI::loadEntityAssets() {
    entityAssets.clear();
    if (world && world->getEntitySystem()) {
        const auto& defs = world->getEntitySystem()->getDefinitions();
        for (const auto& def : defs) {
            if (!def.texturePath.empty()) {
                ImageAsset asset;
                asset.name = def.name;
                asset.path = def.texturePath;
                if (asset.image.loadFromFile(def.texturePath)) {
                    if (asset.texture.loadFromImage(asset.image)) {
                        entityAssets.push_back(std::move(asset));
                    }
                }
            }
        }
    }
}

const sf::Image* UI::getSelectedAssetImage() const {
    if (imageAssets.empty() || selectedAssetIndex < 0 || selectedAssetIndex >= imageAssets.size()) return nullptr;
    return &imageAssets[selectedAssetIndex].image;
}

const sf::Texture* UI::getSelectedAssetTexture() const {
    if (imageAssets.empty() || selectedAssetIndex < 0 || selectedAssetIndex >= imageAssets.size()) return nullptr;
    return &imageAssets[selectedAssetIndex].texture;
}

const sf::Image* UI::getSelectedWeaponImage() const {
    if (weaponAssets.empty() || selectedWeaponIndex < 0 || selectedWeaponIndex >= weaponAssets.size()) return nullptr;
    return &weaponAssets[selectedWeaponIndex].image;
}

const sf::Texture* UI::getSelectedWeaponTexture() const {
    if (weaponAssets.empty() || selectedWeaponIndex < 0 || selectedWeaponIndex >= weaponAssets.size()) return nullptr;
    return &weaponAssets[selectedWeaponIndex].texture;
}

std::string UI::getSelectedWeaponName() const {
    if (weaponAssets.empty() || selectedWeaponIndex < 0 || selectedWeaponIndex >= weaponAssets.size()) return "weapon";
    return weaponAssets[selectedWeaponIndex].name;
}

const sf::Image* UI::getSelectedEntityImage() const {
    if (entityAssets.empty() || selectedEntityIndex < 0 || selectedEntityIndex >= entityAssets.size()) return nullptr;
    return &entityAssets[selectedEntityIndex].image;
}

const sf::Texture* UI::getSelectedEntityTexture() const {
    if (entityAssets.empty() || selectedEntityIndex < 0 || selectedEntityIndex >= entityAssets.size()) return nullptr;
    return &entityAssets[selectedEntityIndex].texture;
}

std::string UI::getSelectedEntityName() const {
    if (entityAssets.empty() || selectedEntityIndex < 0 || selectedEntityIndex >= entityAssets.size()) return "";
    return entityAssets[selectedEntityIndex].name;
}

std::string UI::getSelectedEntityPath() const {
    if (entityAssets.empty() || selectedEntityIndex < 0 || selectedEntityIndex >= entityAssets.size()) return "";
    return entityAssets[selectedEntityIndex].path;
}

void UI::update(sf::RenderWindow& window, sf::Time deltaTime, bool& simRunning, float frameTime, sf::Vector2f mouseWorldPos) {
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowBgAlpha(0.6f); 
    ImGuiWindowFlags tooltipFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | 
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | 
                                    ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    
    if (ImGui::Begin("HoverTooltip", nullptr, tooltipFlags)) {
        std::string hoveredName = "Air";
        if (world) {
            int wx = static_cast<int>(mouseWorldPos.x);
            int wy = static_cast<int>(mouseWorldPos.y);
            
            world->setLayer(targetLayer);
            BaseComponent* base = world->get<BaseComponent>(wx, wy);
            world->setLayer(0);
            
            if (base && base->compMask != 0) {
                auto it = GlobalParticleDefs.find(base->id);
                if (it != GlobalParticleDefs.end()) {
                    hoveredName = it->second.name;
                } else {
                    hoveredName = "Missing ID: " + std::to_string(static_cast<int>(base->id));
                }
            }
        }
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Hovering: %s", hoveredName.c_str());
        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();

    ImGui::Begin("Simulation Control");

    ImGui::Text("Performance: %.1f FPS", 1000.0f / frameTime);
    ImGui::Checkbox("Simulation Running", &simRunning);
    ImGui::Separator();

    ImGui::Text("Spawn Mode");
    int mode = static_cast<int>(spawnMode);
    ImGui::RadioButton("Brush (Particles)", &mode, static_cast<int>(SpawnMode::Particles));
    ImGui::RadioButton("Image / Asset", &mode, static_cast<int>(SpawnMode::Image));
    ImGui::RadioButton("Entity", &mode, static_cast<int>(SpawnMode::Entity));
    ImGui::RadioButton("Weapon", &mode, static_cast<int>(SpawnMode::Weapon)); 
    spawnMode = static_cast<SpawnMode>(mode);
    
    ImGui::Separator();
    ImGui::Text("Target Layer");
    ImGui::RadioButton("Foreground (0)", &targetLayer, 0); ImGui::SameLine();
    ImGui::RadioButton("Background (1)", &targetLayer, 1);
    ImGui::Separator();

    if (spawnMode == SpawnMode::Image) {
        ImGui::Text("Image Assets Gallery");
        ImGui::SliderFloat("Scale (%)", &assetScale, 0.1f, 5.0f, "%.2fx");
        
        ImGui::Checkbox("Spawn as Rigid Body", &spawnAsRigidBody);
        if (spawnAsRigidBody) {
            ImGui::Checkbox("Glue to Terrain", &glueToTerrain);
        }

        ImGui::Spacing();
        if (imageAssets.empty()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No images found in:");
            ImGui::TextWrapped("assets/images/rigidBodies/ OR assets/images/structures/");
        } else {
            int columns = std::max(1, static_cast<int>(ImGui::GetWindowWidth() / 80.0f));
            if (ImGui::BeginTable("##assetgrid", columns)) {
                for (size_t i = 0; i < imageAssets.size(); ++i) {
                    ImGui::TableNextColumn();
                    bool selected = (selectedAssetIndex == i);
                    
                    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.8f, 0.0f, 0.5f)); 
                    
                    std::string btnId = "##asset" + std::to_string(i);
                    if (ImGui::ImageButton(btnId.c_str(), imageAssets[i].texture, sf::Vector2f(60, 60))) selectedAssetIndex = i;
                    
                    if (selected) ImGui::PopStyleColor();
                    ImGui::TextWrapped("%s", imageAssets[i].name.c_str());
                }
                ImGui::EndTable();
            }
        }
    } 
    else if (spawnMode == SpawnMode::Entity) {
        ImGui::Text("Entity Assets Gallery");
        ImGui::Checkbox("Is Player", &spawnAsPlayer);
        if (entityAssets.empty()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No entities registered or valid sprites found!");
        } else {
            int columns = std::max(1, static_cast<int>(ImGui::GetWindowWidth() / 80.0f));
            if (ImGui::BeginTable("##entitygrid", columns)) {
                for (size_t i = 0; i < entityAssets.size(); ++i) {
                    ImGui::TableNextColumn();
                    bool selected = (selectedEntityIndex == i);
                    
                    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.8f, 0.0f, 0.5f)); 
                    
                    std::string btnId = "##ent" + std::to_string(i);
                    sf::Sprite thumb(entityAssets[i].texture);
                    int w = std::min(32u, entityAssets[i].texture.getSize().x);
                    int h = std::min(32u, entityAssets[i].texture.getSize().y);
                    thumb.setTextureRect(sf::IntRect({0, 0}, {w, h}));
                    
                    if (ImGui::ImageButton(btnId.c_str(), thumb, sf::Vector2f(60, 60))) {
                        selectedEntityIndex = i;
                    }
                    
                    if (selected) ImGui::PopStyleColor();
                    ImGui::TextWrapped("%s", entityAssets[i].name.c_str());
                }
                ImGui::EndTable();
            }
        }
    }
    else if (spawnMode == SpawnMode::Weapon) {
        ImGui::Text("Weapon Assets Gallery");
        if (weaponAssets.empty()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No images found in assets/images/weapons/");
        } else {
            int columns = std::max(1, static_cast<int>(ImGui::GetWindowWidth() / 80.0f));
            if (ImGui::BeginTable("##weapongrid", columns)) {
                for (size_t i = 0; i < weaponAssets.size(); ++i) {
                    ImGui::TableNextColumn();
                    bool selected = (selectedWeaponIndex == i);
                    
                    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.8f, 0.0f, 0.5f)); 
                    
                    std::string btnId = "##weap" + std::to_string(i);
                    if (ImGui::ImageButton(btnId.c_str(), weaponAssets[i].texture, sf::Vector2f(60, 60))) {
                        selectedWeaponIndex = i;
                    }
                    
                    if (selected) ImGui::PopStyleColor();
                    ImGui::TextWrapped("%s", weaponAssets[i].name.c_str());
                }
                ImGui::EndTable();
            }
        }
    }
    else {
        ImGui::SliderFloat("Brush Radius", &selectionRadius, MIN_SELECTION_RADIUS, MAX_SELECTION_RADIUS);
        ImGui::Spacing();
        ImGui::Text("Brush Shape");
        int shape = static_cast<int>(brushShape);
        ImGui::RadioButton("Circle", &shape, static_cast<int>(BrushShape::Circle)); ImGui::SameLine();
        ImGui::RadioButton("Square", &shape, static_cast<int>(BrushShape::Square)); ImGui::SameLine();
        ImGui::RadioButton("Platform", &shape, static_cast<int>(BrushShape::Platform));
        brushShape = static_cast<BrushShape>(shape);
        ImGui::Spacing();
        ImGui::Checkbox("Line Mode (Drag to draw)", &useLineMode);
    }

    ImGui::Separator();
    if (ImGui::Button("Clear Canvas", ImVec2(-1, 0))) world->clear();
    if (ImGui::Button("Save World", ImVec2(-1, 0))) world->saveWorld("world");

    ImGui::Separator();
    ImGui::Text("Debug Overlays");
    ImGui::Checkbox("Show Chunk Bounds", &showChunkBounds);
    ImGui::Checkbox("Show Colliders", &showColliders);
    
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Materials");
    drawMaterialTabs();
    ImGui::End();
}

void UI::drawMaterialTabs() {
    std::vector<std::pair<MaterialID, ParticleDef>> sortedMaterials(
        GlobalParticleDefs.begin(), GlobalParticleDefs.end()
    );
    std::sort(sortedMaterials.begin(), sortedMaterials.end(), 
        [](const auto& a, const auto& b) {
            return static_cast<uint8_t>(a.first) < static_cast<uint8_t>(b.first);
        });

    if (ImGui::BeginTabBar("Categories")) {
        auto renderGroup = [&](const char* label, MaterialGroup group) {
            if (ImGui::BeginTabItem(label)) {
                
                for (const auto& [id, def] : sortedMaterials) {
                    if (def.group == group) {
                        
                        sf::Color c = def.colors.empty() ? sf::Color::Magenta : def.colors[0];

                        ImVec4 col = ImVec4(c.r/255.f, c.g/255.f, c.b/255.f, 1.0f);
                        ImGui::PushStyleColor(ImGuiCol_Button, col);
                        
                        if (ImGui::Button(def.name.c_str(), ImVec2(80, 40))) {
                            currentMaterial = id;
                        }
                        
                        if (currentMaterial == id) {
                            ImGui::GetWindowDrawList()->AddRect(
                                ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), 
                                IM_COL32(255, 255, 0, 255), 0, 0, 2.0f
                            );
                        }
                        ImGui::PopStyleColor();
                        
                        if (ImGui::GetItemRectMax().x < ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 90)
                            ImGui::SameLine();
                    }
                }
                ImGui::EndTabItem();
            }
        };

        renderGroup("Solids", MaterialGroup::MovableSolid);
        renderGroup("Static", MaterialGroup::ImmovableSolid);
        renderGroup("Liquids", MaterialGroup::Liquid);
        renderGroup("Gases", MaterialGroup::Gas);
        renderGroup("Special", MaterialGroup::Special);
        ImGui::EndTabBar();
    }
}

void UI::render(sf::RenderWindow& window) {}

bool UI::isMouseOverUI() const {
    return ImGui::GetIO().WantCaptureMouse;
}