#include "UI.hpp"
#include "ParticleWorld.hpp"
#include <imgui.h>
#include <imgui-SFML.h>

UI::UI(sf::RenderWindow& window, ParticleWorld* worldPtr) : world(worldPtr) {}

void UI::update(sf::RenderWindow& window, sf::Time deltaTime, bool& simRunning, float frameTime) {
    ImGui::Begin("Simulation Control");

    ImGui::Text("Performance: %.1f FPS", 1000.0f / frameTime);
    ImGui::Checkbox("Simulation Running", &simRunning);
    ImGui::Separator();

    // --- SPAWN MODE SELECTOR ---
    ImGui::Text("Spawn Mode");
    int mode = static_cast<int>(spawnMode);
    ImGui::RadioButton("Brush (Particles)", &mode, static_cast<int>(SpawnMode::Particles));
    ImGui::RadioButton("Rigid Body", &mode, static_cast<int>(SpawnMode::RigidBody));
    ImGui::RadioButton("Entity", &mode, static_cast<int>(SpawnMode::Entity));
    ImGui::RadioButton("Weapon", &mode, static_cast<int>(SpawnMode::Weapon)); // New weapon mode
    spawnMode = static_cast<SpawnMode>(mode);
    
    ImGui::Separator();

    // Mode specific options
    if (spawnMode == SpawnMode::RigidBody) {
        ImGui::Text("Rigid Body Settings");
        int shape = static_cast<int>(currentShape);
        ImGui::RadioButton("Box", &shape, static_cast<int>(RigidBodyShape::Box)); ImGui::SameLine();
        ImGui::RadioButton("Circle", &shape, static_cast<int>(RigidBodyShape::Circle));
        currentShape = static_cast<RigidBodyShape>(shape);
        ImGui::SliderFloat("Size", &selectionRadius, MIN_SELECTION_RADIUS, MAX_SELECTION_RADIUS);
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