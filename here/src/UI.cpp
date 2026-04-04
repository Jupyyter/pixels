#include "UI.hpp"
#include "ParticleWorld.hpp"
#include <imgui.h>
#include <imgui-SFML.h>
#include <filesystem>
#include <iostream>

UI::UI(sf::RenderWindow& window, ParticleWorld* worldPtr) : world(worldPtr) {
    loadRigidBodyAssets();
}

void UI::loadRigidBodyAssets() {
    std::string folderPath = "assets/images/rigidBodies";
    
    if (!std::filesystem::exists(folderPath)) {
        std::filesystem::create_directories(folderPath);
    }

    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            std::string path = entry.path().string();
            std::string ext = entry.path().extension().string();
            
            if (ext == ".png" || ext == ".jpg" || ext == ".bmp") {
                RigidBodyAsset asset;
                asset.name = entry.path().stem().string();
                asset.path = path;
                
                if (asset.image.loadFromFile(path)) {
                    if (asset.texture.loadFromImage(asset.image)) {
                        rigidBodyAssets.push_back(std::move(asset));
                    }
                }
            }
        }
    }
}

const sf::Image* UI::getSelectedRigidBodyImage() const {
    if (rigidBodyAssets.empty() || selectedRigidBodyIndex < 0 || selectedRigidBodyIndex >= rigidBodyAssets.size()) return nullptr;
    return &rigidBodyAssets[selectedRigidBodyIndex].image;
}

const sf::Texture* UI::getSelectedRigidBodyTexture() const {
    if (rigidBodyAssets.empty() || selectedRigidBodyIndex < 0 || selectedRigidBodyIndex >= rigidBodyAssets.size()) return nullptr;
    return &rigidBodyAssets[selectedRigidBodyIndex].texture;
}

void UI::update(sf::RenderWindow& window, sf::Time deltaTime, bool& simRunning, float frameTime) {
    ImGui::Begin("Simulation Control");

    ImGui::Text("Performance: %.1f FPS", 1000.0f / frameTime);
    ImGui::Checkbox("Simulation Running", &simRunning);
    ImGui::Separator();

    ImGui::Text("Spawn Mode");
    int mode = static_cast<int>(spawnMode);
    ImGui::RadioButton("Brush (Particles)", &mode, static_cast<int>(SpawnMode::Particles));
    ImGui::RadioButton("Rigid Body", &mode, static_cast<int>(SpawnMode::RigidBody));
    ImGui::RadioButton("Entity", &mode, static_cast<int>(SpawnMode::Entity));
    ImGui::RadioButton("Weapon", &mode, static_cast<int>(SpawnMode::Weapon)); 
    spawnMode = static_cast<SpawnMode>(mode);
    
    ImGui::Separator();

    if (spawnMode == SpawnMode::RigidBody) {
        ImGui::Text("Rigid Body Assets");
        ImGui::SliderFloat("Scale (%)", &rigidBodyScale, 0.1f, 5.0f, "%.2fx");
        ImGui::Checkbox("Glue to Terrain", &glueToTerrain);
        
        if (rigidBodyAssets.empty()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No images found in:");
            ImGui::TextWrapped("assets/images/rigidBodies/");
        } else {
            int columns = std::max(1, static_cast<int>(ImGui::GetWindowWidth() / 80.0f));
            if (ImGui::BeginTable("##rbgrid", columns)) {
                for (size_t i = 0; i < rigidBodyAssets.size(); ++i) {
                    ImGui::TableNextColumn();
                    bool selected = (selectedRigidBodyIndex == i);
                    
                    if (selected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.8f, 0.0f, 0.5f)); 
                    }
                    
                    std::string btnId = "##rb" + std::to_string(i);
                    if (ImGui::ImageButton(btnId.c_str(), rigidBodyAssets[i].texture, sf::Vector2f(60, 60))) {
                        selectedRigidBodyIndex = i;
                    }
                    
                    if (selected) {
                        ImGui::PopStyleColor();
                    }
                    
                    ImGui::TextWrapped("%s", rigidBodyAssets[i].name.c_str());
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
        ImGui::Text("Weapon Spawner");
        ImGui::TextDisabled("Click to drop a weapon.");
        ImGui::TextDisabled("Press 'E' near it to equip/drop.");
    }
    else {
        ImGui::SliderFloat("Brush Radius", &selectionRadius, MIN_SELECTION_RADIUS, MAX_SELECTION_RADIUS);
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