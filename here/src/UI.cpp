#include "UI.hpp"
#include "ParticleWorld.hpp"
#include <imgui.h>
#include <imgui-SFML.h>

UI::UI(sf::RenderWindow& window, ParticleWorld* worldPtr) : world(worldPtr) {
    // ImGui-SFML initialization is handled in SandSimApp
}

void UI::update(sf::RenderWindow& window, sf::Time deltaTime, bool& simRunning, float frameTime) {

    // Create a Window that can be moved and resized
    ImGui::Begin("Simulation Control");

    ImGui::Text("Performance: %.1f FPS", 1000.0f / frameTime);
    ImGui::Checkbox("Simulation Running", &simRunning);
    
    ImGui::Separator();
    
    ImGui::SliderFloat("Brush Radius", &selectionRadius, MIN_SELECTION_RADIUS, MAX_SELECTION_RADIUS);
ImGui::Separator();
    ImGui::Checkbox("Spawn as Rigid Body", &spawnAsRigidBody);
    if (spawnAsRigidBody) {
        int shape = static_cast<int>(currentShape);
        ImGui::RadioButton("Box", &shape, static_cast<int>(RigidBodyShape::Box)); ImGui::SameLine();
        ImGui::RadioButton("Circle", &shape, static_cast<int>(RigidBodyShape::Circle));
        currentShape = static_cast<RigidBodyShape>(shape);
    }
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
    ImGui::Text("Elements");
    
    drawMaterialTabs();

    ImGui::End();
}

void UI::drawMaterialTabs() {
    if (ImGui::BeginTabBar("Categories")) {
        
        auto renderGroup = [&](const char* label, MaterialGroup group) {
            if (ImGui::BeginTabItem(label)) {
                for (const auto& mat : ALL_MATERIALS) {
                    if (mat.group == group) {
                        
                        // --- MODIFIED SECTION START ---
                        // 1. Safely get the first color from the palette
                        sf::Color c = sf::Color::Magenta; // Fallback
                        if (!mat.palette.empty()) {
                            c = mat.palette[0]; 
                        }

                        // 2. Convert SFML color to ImGui color
                        ImVec4 col = ImVec4(c.r/255.f, c.g/255.f, c.b/255.f, 1.0f);
                        // --- MODIFIED SECTION END ---

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

void UI::render(sf::RenderWindow& window) {
}

bool UI::isMouseOverUI() const {
    return ImGui::GetIO().WantCaptureMouse;
}