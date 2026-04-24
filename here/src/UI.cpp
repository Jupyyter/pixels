#include "UI.hpp"
#include "ParticleWorld.hpp"
#include <imgui.h>
#include <imgui-SFML.h>
#include <filesystem>
#include <iostream>

UI::UI(sf::RenderWindow& window, ParticleWorld* worldPtr) : world(worldPtr) {
    loadImageAssets();
    loadWeaponAssets();
}

void UI::loadImageAssets() {
    imageAssets.clear();
    
    auto loadFromDir = [&](const std::string& folderPath) {
        if (!std::filesystem::exists(folderPath)) {
            std::filesystem::create_directories(folderPath);
        }

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
    
    // Use recursive_directory_iterator to ensure we look inside the /revolver/ and /submachine/ folders
    for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            std::string path = entry.path().string();
            std::string ext = entry.path().extension().string();
            
            if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                std::string stem = entry.path().stem().string();
                
                // Exclude ONLY the animation sprite sheets.
                // The animations contain "[SHOOT", while the base sprites just contain "[width x height]".
                if (stem.find("[SHOOT") != std::string::npos) {
                    continue; 
                }
                
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


void UI::update(sf::RenderWindow& window, sf::Time deltaTime, bool& simRunning, float frameTime, sf::Vector2f mouseWorldPos) {
    
    // --- HIGH VISIBILITY HOVER TOOLTIP ---
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
            BaseComponent* base = world->get<BaseComponent>(wx, wy);
            if (base && base->compMask != 0) {
                for (const auto& mat : ALL_MATERIALS) {
                    if (mat.id == base->id) {
                        hoveredName = mat.name;
                        break;
                    }
                }
            }
        }
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Hovering: %s", hoveredName.c_str());
        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();

    // --- MAIN UI ---
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
                    
                    if (selected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.8f, 0.0f, 0.5f)); 
                    }
                    
                    std::string btnId = "##asset" + std::to_string(i);
                    if (ImGui::ImageButton(btnId.c_str(), imageAssets[i].texture, sf::Vector2f(60, 60))) {
                        selectedAssetIndex = i;
                    }
                    
                    if (selected) {
                        ImGui::PopStyleColor();
                    }
                    ImGui::TextWrapped("%s", imageAssets[i].name.c_str());
                }
                ImGui::EndTable();
            }
        }
    } 
    else if (spawnMode == SpawnMode::Entity) {
        ImGui::Text("Entity Spawner");
        int ent = static_cast<int>(currentEntity);
        ImGui::RadioButton("Player", &ent, static_cast<int>(EntityType::Player));
        currentEntity = static_cast<EntityType>(ent);
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
        ImGui::RadioButton("Square", &shape, static_cast<int>(BrushShape::Square));
        brushShape = static_cast<BrushShape>(shape);
        
        ImGui::Spacing();
        ImGui::Checkbox("Line Mode (Drag to draw)", &useLineMode);
    }

    ImGui::Separator();
    
    if (ImGui::Button("Clear Canvas", ImVec2(-1, 0))) {
        world->clear();
    }
    if (ImGui::Button("Save World", ImVec2(-1, 0))) {
        world->saveWorld("world");
    }

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
    if (ImGui::BeginTabBar("Categories")) {
        auto renderGroup = [&](const char* label, MaterialGroup group) {
            if (ImGui::BeginTabItem(label)) {
                for (const auto& mat : ALL_MATERIALS) {
                    if (mat.group == group) {
                        sf::Color c = sf::Color::Magenta;
                        if (!mat.palette.empty()) c = mat.palette[0]; 

                        ImVec4 col = ImVec4(c.r/255.f, c.g/255.f, c.b/255.f, 1.0f);
                        ImGui::PushStyleColor(ImGuiCol_Button, col);
                        
                        if (ImGui::Button(mat.name.c_str(), ImVec2(80, 40))) {
                            currentMaterial = mat.id;
                        }
                        
                        if (currentMaterial == mat.id) {
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