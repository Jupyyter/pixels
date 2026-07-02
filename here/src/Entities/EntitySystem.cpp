// text/plain
// entitysystem.cpp
#include "Entities/EntitySystem.hpp"
#include "Entities/Entity.hpp"
#include "Entities/ComplexEntity.hpp"
#include "Entities/SimpleEntity.hpp"
#include "Weapon.hpp"
#include "ParticleWorld.hpp"
#include "RigidBody.hpp"
#include "Constants.hpp"
#include <cmath>
#include <iostream>
#include <cstdlib>
#include <queue>
#include <map>
#include <algorithm>
#include <SFML/Window/Mouse.hpp>

constexpr float NO_GROUND = 1e9f;

namespace {
    struct EdgeData {
        int action; 
        float dir;
        std::vector<sf::Vector2f> trajectory;
        float requiredVx = 0.0f;
    };
    std::map<std::pair<int, int>, EdgeData> s_EdgeData;
}

EntitySystem::EntitySystem(b2WorldId physWorld) : physicsWorldId(physWorld) {
    sf::Image dummy;
    dummy.resize(sf::Vector2u(32, 32), sf::Color(100, 100, 100));
    defaultPlayerTexture = std::make_shared<sf::Texture>(dummy);
    
    registerEntities();
}

EntitySystem::~EntitySystem() { 
    clearAll(); 
}

void EntitySystem::registerEntities() {
    EntityDefinition bunny;
    bunny.name = "Bunny";
    bunny.texturePath = "assets/images/entities/bunny.png";
    bunny.isComplex = true;
    bunny.frameWidth = 32;
    bunny.frameHeight = 32;
    
    // Exactly as you specified: 7 pixels wide, 16 pixels tall
    bunny.setCollider({13.0f, 11.0f}, {19.0f, 26.0f});
    
    bunny.uprightMultiplier = 1.0f;
    bunny.hipBaseY = 8.0f;
    bunny.armBaseY = 8.0f;
    bunny.animations["Idle"] = {0, 0, 1, 0.1f};
    bunny.animations["Walk"] = {0, 0, 1, 0.1f};
    bunny.animations["Jump"] = {0, 0, 1, 0.1f};
    entityDefs.push_back(bunny);

    EntityDefinition wolf = bunny;
    wolf.name = "Wolf";
    wolf.texturePath = "assets/images/entities/wolf.png";
    entityDefs.push_back(wolf);

    EntityDefinition wizzard;
    wizzard.name = "Wizzard";
    wizzard.texturePath = "assets/images/entities/wizzard.png";
    wizzard.isComplex = false;
    wizzard.frameWidth = 32;
    wizzard.frameHeight = 32; 
    
    // Wizard coordinates: 32 pixels wide (0 to 31), 31 pixels tall (1 to 31)
    wizzard.setCollider({3.0f, 1.0f}, {29.0f, 31.0f}); 
    wizzard.colliderRadius = 4.0f; // Rounds the Wizard's corners!
    
    wizzard.uprightMultiplier = 15.0f; 
    wizzard.hipBaseY = 8.0f; 
    wizzard.armBaseY = 8.0f; 
    wizzard.animations["Jump"] = {1, 0, 1, 0.1f};
    wizzard.animations["Fall"] = {0, 0, 1, 0.1f};
    wizzard.animations["Idle"] = {0, 1, 1, 0.1f};
    wizzard.animations["Walk"] = {0, 2, 2, 0.1f};
    entityDefs.push_back(wizzard);
}
void EntitySystem::clearAll() {
    entities.clear();
}

void EntitySystem::eraseEntitiesInRadius(sf::Vector2f center, float radius) {
    entities.erase(std::remove_if(entities.begin(), entities.end(), [&](const std::unique_ptr<Entity>& e) {
        b2Vec2 p = b2Body_GetPosition(e->bodyId);
        float dist = std::hypot(p.x * M2P - center.x, p.y * M2P - center.y);
        return dist <= radius + 16.0f;
    }), entities.end());
}

void EntitySystem::eraseEntitiesInSquare(sf::Vector2f center, float radius) {
    entities.erase(std::remove_if(entities.begin(), entities.end(), [&](const std::unique_ptr<Entity>& e) {
        b2Vec2 p = b2Body_GetPosition(e->bodyId);
        return std::abs(p.x * M2P - center.x) <= radius + 16.0f && std::abs(p.y * M2P - center.y) <= radius + 16.0f;
    }), entities.end());
}

void EntitySystem::save(std::ostream& out) const {
    size_t count = entities.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(size_t));
    for (const auto& e : entities) {
        int type = e->getType();
        out.write((const char*)&type, sizeof(int));
        
        b2Vec2 pos = b2Body_GetPosition(e->bodyId);
        out.write((const char*)&pos, sizeof(b2Vec2));
        
        size_t len = e->defName.size();
        out.write((const char*)&len, sizeof(size_t));
        if (len > 0) { out.write(e->defName.c_str(), len); }
        
        e->save(out);
    }
}

void EntitySystem::load(std::istream& in) {
    clearAll(); 
    size_t count = 0;
    if (in.read(reinterpret_cast<char*>(&count), sizeof(size_t))) {
        for (size_t i = 0; i < count; ++i) {
            int type = 0;
            in.read((char*)&type, sizeof(int));
            
            b2Vec2 pos;
            in.read((char*)&pos, sizeof(b2Vec2));
            
            size_t tLen = 0;
            in.read((char*)&tLen, sizeof(size_t));
            std::string dName = "";
            if (tLen > 0) {
                dName.resize(tLen);
                in.read(&dName[0], tLen);
            }
            
            Entity* spawned = spawnEntity(pos.x * M2P, pos.y * M2P, dName, false);
            if (spawned) spawned->load(in);
        }
    }
}

Entity* EntitySystem::spawnEntity(float x, float y, const std::string& defName, bool isPlayer) {
    const EntityDefinition* foundDef = nullptr;
    for (const auto& def : entityDefs) {
        if (def.name == defName) {
            foundDef = &def;
            break;
        }
    }
    
    if (!foundDef && !entityDefs.empty()) {
        foundDef = &entityDefs.front();
    }
    
    if (!foundDef) return nullptr;

    std::unique_ptr<Entity> e;
    if (foundDef->isComplex) {
        e = std::make_unique<ComplexEntity>(physicsWorldId, this, x, y, *foundDef, isPlayer);
    } else {
        e = std::make_unique<SimpleEntity>(physicsWorldId, this, x, y, *foundDef, isPlayer);
    }
    Entity* ptr = e.get();
    entities.push_back(std::move(e));
    return ptr;
}

void EntitySystem::triggerSwing(sf::Vector2f targetWorldPos) {
    for (auto& e : entities) {
        e->triggerSwing(targetWorldPos);
    }
}

void EntitySystem::updateInput(float dt, sf::Vector2f mouseWorldPos, RigidBodySystem& rbs, ParticleWorld& pw) {
    static bool rightClickLastGlobal = false;
    bool currentRightClickGlobal = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
    bool orderGiven = currentRightClickGlobal && !rightClickLastGlobal;
    rightClickLastGlobal = currentRightClickGlobal;

    if (hasDirtyNavRegion) {
        for (auto& e : entities) {
            if (e->pCtrl.hasTarget || e->pCtrl.isWandering) {
                bool compromised = false;
                for (size_t i = e->pCtrl.pathIndex; i < e->pCtrl.path.size(); ++i) {
                    if (dirtyNavRect.contains({e->pCtrl.path[i].pos.x, e->pCtrl.path[i].pos.y})) { compromised = true; break; }
                }
                if (!compromised && dirtyNavRect.contains(e->pCtrl.targetPos)) compromised = true;
                if (!compromised && dirtyNavRect.contains(e->pCtrl.lastPos)) compromised = true;
                if (compromised) e->pCtrl.pathRecalcTimer = 0.0f;
            }
        }
        hasDirtyNavRegion = false;
    }

    if (!globalGraphBuilt || orderGiven) {
        buildGlobalNavGraph(pw);
    }

    debugLines.clear();
    for (auto& e : entities) {
        e->updateInput(dt, mouseWorldPos, rbs, pw, orderGiven);
    }
}

float EntitySystem::groundCastY(float worldX, float castFromY, float maxDown, ParticleWorld& pw, bool ignorePlatforms, float bodyPosY) {
    int px     = static_cast<int>(std::round(worldX));
    int startY = static_cast<int>(std::floor(castFromY));

    for (int i = 0; i <= static_cast<int>(maxDown); ++i) {
        int py = startY + i;
        if (!pw.isEmpty(px, py)) {
            BaseComponent* base = pw.get<BaseComponent>(px, py);
            if (base && base->compMask != 0) {
                if (base->compMask & COMP_PLATFORM) {
                    if (ignorePlatforms) continue;
                    if (py < castFromY - 1.0f) continue; 
                }
                Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
                if (logic) {
                    MaterialGroup group = logic->getGroup();
                    if (group != MaterialGroup::Liquid && group != MaterialGroup::Gas) {
                        debugLines.push_back({{worldX, castFromY}, {worldX, static_cast<float>(py - 1)}, sf::Color::Green});
                        return static_cast<float>(py - 1);
                    }
                }
            }
        }
    }
    debugLines.push_back({{worldX, castFromY}, {worldX, castFromY + maxDown}, sf::Color::Red});
    return NO_GROUND;
}

void EntitySystem::updateProceduralAnimations(float dt, ParticleWorld& pw) {
    for (auto& e : entities) {
        e->updateAnimations(dt, pw);
    }
}

bool EntitySystem::isSolid(int cx, int cy, ParticleWorld& pw, bool ignorePlatforms) {
    if (pw.isEmpty(cx, cy)) return false;
    BaseComponent* b = pw.get<BaseComponent>(cx, cy);
    if (b && b->compMask != 0 && !b->flags.isRigidBodyPart) {
        if (ignorePlatforms && (b->compMask & COMP_PLATFORM)) return false;
        Particle* p = MaterialRegistry[static_cast<int>(b->id)];
        if (p && p->getGroup() != MaterialGroup::Liquid && p->getGroup() != MaterialGroup::Gas) return true;
    }
    return false;
}

sf::Vector2f EntitySystem::resolveTargetPos(sf::Vector2f clickPos, ParticleWorld& pw) {
    int x = static_cast<int>(clickPos.x);
    int w = static_cast<int>(WORLD_WIDTH);
    int h = static_cast<int>(WORLD_HEIGHT);
    if (x < 0) x = 0;
    if (x >= w) x = w - 1;
    int y = static_cast<int>(clickPos.y);
    if (y < 0) y = 0;
    if (y >= h) y = h - 1;
    
    if (isSolid(x, y, pw, false)) {
        for (int cy = y; cy > 0; --cy) {
            if (!isSolid(x, cy - 1, pw, true) && isSolid(x, cy, pw, false)) {
                return sf::Vector2f(x, cy - 1);
            }
        }
    } else {
        for (int cy = y; cy < h - 1; ++cy) {
            if (!isSolid(x, cy, pw, true) && isSolid(x, cy + 1, pw, false)) {
                return sf::Vector2f(x, cy);
            }
        }
    }
    return sf::Vector2f(x, y); 
}

void EntitySystem::buildGlobalNavGraph(ParticleWorld& pw) {
    globalNavGraph.clear();
    s_EdgeData.clear();
    
    int width = static_cast<int>(WORLD_WIDTH);
    int height = static_cast<int>(WORLD_HEIGHT);
    const int STEP = 16;
    const int CLEARANCE_H = 32; 
    const int CLEARANCE_W = 4;  

    std::map<int, std::vector<int>> columnNodes; 
    
    for (int x = STEP; x < width - STEP; x += STEP) {
        for (int y = 1; y < height - 1; ++y) {
            if (!isSolid(x, y - 1, pw, true) && isSolid(x, y, pw, false)) {
                bool fits = true;
                for (int cy = y - CLEARANCE_H; cy <= y - 6; ++cy) {
                    for (int cx = x - CLEARANCE_W; cx <= x + CLEARANCE_W; ++cx) {
                        if (isSolid(cx, cy, pw, true)) { fits = false; break; }
                    }
                    if (!fits) break;
                }
                
                if (fits) {
                    AINode node;
                    node.pos = sf::Vector2f(x, y - 1); 
                    globalNavGraph.push_back(node);
                    columnNodes[x].push_back(globalNavGraph.size() - 1);
                }
            }
        }
    }

    for (const auto& [x, indices] : columnNodes) {
        if (columnNodes.find(x + STEP) != columnNodes.end()) {
            const auto& nextIndices = columnNodes[x + STEP];
            for (int i : indices) {
                for (int j : nextIndices) {
                    sf::Vector2f p1 = globalNavGraph[i].pos;
                    sf::Vector2f p2 = globalNavGraph[j].pos;
                    if (std::abs(p1.y - p2.y) > 16.0f) continue; 
                    
                    bool blocked = false;
                    for (int s = 1; s < STEP; ++s) {
                        float t = (float)s / STEP;
                        int cx = static_cast<int>(std::round(p1.x + (p2.x - p1.x) * t));
                        float groundY = p1.y + (p2.y - p1.y) * t;
                        for (int cy = static_cast<int>(groundY) - CLEARANCE_H; cy <= static_cast<int>(groundY) - 4; ++cy) {
                            if (isSolid(cx, cy, pw, true)) { blocked = true; break; }
                        }
                        if (blocked) break;
                    }
                    if (!blocked) {
                        globalNavGraph[i].neighbors.push_back(j);
                        globalNavGraph[j].neighbors.push_back(i);
                        EdgeData walkData;
                        walkData.action = 0;
                        s_EdgeData[{i, j}] = walkData;
                        s_EdgeData[{j, i}] = walkData;
                    }
                }
            }
        }
    }

    float sim_dt = 1.0f / 60.0f;
    int max_steps = 75;

    for (size_t i = 0; i < globalNavGraph.size(); ++i) {
        sf::Vector2f startP = globalNavGraph[i].pos;
        for (int isJump = 0; isJump <= 1; ++isJump) {
            float speeds[] = {-70.0f, -45.0f, -25.0f, 25.0f, 45.0f, 70.0f}; 
            for (float vx : speeds) {
                sf::Vector2f p = startP;
                float currentVx = vx;
                float currentVy = isJump ? -380.0f : 0.0f; 
                std::vector<sf::Vector2f> traj;
                traj.push_back(p);
                
                bool isAirborne = (isJump == 1); 
                bool hitGround = false;
                
                for (int step = 0; step < max_steps; ++step) {
                    if (isAirborne) currentVy += 980.0f * sim_dt; 
                    else {
                        currentVy = 0.0f;
                        int checkX = static_cast<int>(std::round(p.x));
                        int groundY = static_cast<int>(std::round(startP.y + 1.0f));
                        if (checkX < 0 || checkX >= width) break;
                        if (!isSolid(checkX, groundY, pw, false)) isAirborne = true;
                    }
                    
                    currentVx *= std::max(0.0f, 1.0f - 1.0f * sim_dt);
                    if (isAirborne) currentVy *= std::max(0.0f, 1.0f - 1.0f * sim_dt);
                    
                    p.x += currentVx * sim_dt;
                    p.y += currentVy * sim_dt;
                    if (!isAirborne) p.y = startP.y; 
                    
                    if (p.x < 0 || p.x >= width || p.y < 0 || p.y >= height) break;
                    
                    bool blocked = false;
                    for (int cy = static_cast<int>(p.y) - CLEARANCE_H+8; cy <= static_cast<int>(p.y) - 6; cy += 4) {
                        if (isSolid(p.x, cy, pw, true) || isSolid(p.x - 2, cy, pw, true) || isSolid(p.x + 2, cy, pw, true)) {
                            blocked = true; break;
                        }
                    }
                    if (blocked) break;
                    traj.push_back(p);
                    
                    if (isAirborne && currentVy > 0.0f) {
                        if (isSolid(p.x, p.y + 1, pw, false)) { hitGround = true; break; }
                    }
                }
                
                if (hitGround) {
                    int closest = -1;
                    float minDist = 16.0f; 
                    for (size_t j = 0; j < globalNavGraph.size(); ++j) {
                        if (i == j) continue;
                        float dist = std::hypot(globalNavGraph[j].pos.x - p.x, globalNavGraph[j].pos.y - p.y);
                        if (dist < minDist) { minDist = dist; closest = j; }
                    }
                    
                    if (closest != -1) {
                        auto edgeKey = std::make_pair(static_cast<int>(i), closest);
                        bool isRedundant = (s_EdgeData.find(edgeKey) != s_EdgeData.end());

                        if (!isRedundant && std::abs(startP.y - globalNavGraph[closest].pos.y) <= 32.0f) {
                            bool gapFound = false;
                            float minX = std::min(startP.x, globalNavGraph[closest].pos.x);
                            float maxX = std::max(startP.x, globalNavGraph[closest].pos.x);
                            
                            if (maxX - minX > 16.0f) {
                                for (float px = minX + 8.0f; px < maxX; px += 8.0f) {
                                    bool groundFound = false;
                                    for (int cy = startP.y - 16; cy <= startP.y + 48; ++cy) {
                                        if (isSolid(px, cy, pw, false)) { groundFound = true; break; }
                                    }
                                    if (!groundFound) { gapFound = true; break; }
                                }
                                if (!gapFound) isRedundant = true;
                            } else isRedundant = true;
                        }

                        if (!isRedundant) {
                            globalNavGraph[i].neighbors.push_back(closest);
                            EdgeData data;
                            data.action = isJump ? 1 : 2;
                            data.dir = (vx > 0) ? 1.0f : -1.0f;
                            data.trajectory = traj;
                            data.requiredVx = vx;
                            s_EdgeData[edgeKey] = data;
                        }
                    }
                }
            }
        }
    }
    
    globalGraphBuilt = true;
    std::cout << "NavMesh Built! Total Nodes: " << globalNavGraph.size() << "\n";
}

int EntitySystem::getClosestNode(sf::Vector2f pos) {
    int bestIdx = -1;
    float bestDist = 1e9f;
    for (size_t i = 0; i < globalNavGraph.size(); ++i) {
        float dist = std::hypot(globalNavGraph[i].pos.x - pos.x, globalNavGraph[i].pos.y - pos.y);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}

std::vector<PathNodeData> EntitySystem::findPath(sf::Vector2f start, sf::Vector2f target) {
    if (globalNavGraph.empty()) return {};
    int startIdx = getClosestNode(start);
    int targetIdx = getClosestNode(target);
    if (startIdx == -1 || targetIdx == -1) return {};

    std::vector<float> gScore(globalNavGraph.size(), 1e9f);
    std::vector<int> cameFrom(globalNavGraph.size(), -1);
    std::vector<bool> closed(globalNavGraph.size(), false); 
    gScore[startIdx] = 0.0f;

    auto cmp = [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; };
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, decltype(cmp)> pq(cmp);
    pq.push({0.0f, startIdx});

    int closestReachableIdx = startIdx;
    float minH = std::hypot(globalNavGraph[startIdx].pos.x - globalNavGraph[targetIdx].pos.x, 
                            globalNavGraph[startIdx].pos.y - globalNavGraph[targetIdx].pos.y);

    while (!pq.empty()) {
        int curr = pq.top().second;
        pq.pop();
        if (closed[curr]) continue;
        closed[curr] = true;

        if (curr == targetIdx) {
            closestReachableIdx = targetIdx;
            break;
        }

        float hCurr = std::hypot(globalNavGraph[curr].pos.x - globalNavGraph[targetIdx].pos.x, 
                                 globalNavGraph[curr].pos.y - globalNavGraph[targetIdx].pos.y);
        if (hCurr < minH) {
            minH = hCurr;
            closestReachableIdx = curr;
        }

        for (int nxt : globalNavGraph[curr].neighbors) {
            float dist = std::hypot(globalNavGraph[nxt].pos.x - globalNavGraph[curr].pos.x, 
                                    globalNavGraph[nxt].pos.y - globalNavGraph[curr].pos.y);
            auto edgeIt = s_EdgeData.find({curr, nxt});
            if (edgeIt != s_EdgeData.end() && edgeIt->second.action != 0) dist += 150.0f; 
            float tentative = gScore[curr] + dist;
            if (tentative < gScore[nxt]) {
                gScore[nxt] = tentative;
                cameFrom[nxt] = curr;
                float h = std::hypot(globalNavGraph[nxt].pos.x - globalNavGraph[targetIdx].pos.x, 
                                     globalNavGraph[nxt].pos.y - globalNavGraph[targetIdx].pos.y);
                pq.push({tentative + h, nxt});
            }
        }
    }

    std::vector<int> pathIndices;
    int curr = closestReachableIdx;
    while (curr != -1) {
        pathIndices.push_back(curr);
        curr = cameFrom[curr];
    }
    std::reverse(pathIndices.begin(), pathIndices.end());

    std::vector<PathNodeData> finalPath;
    if (!pathIndices.empty()) finalPath.push_back(PathNodeData{globalNavGraph[pathIndices[0]].pos, false, false, false, 0.0f});

    for (size_t i = 1; i < pathIndices.size(); ++i) {
        int prev = pathIndices[i - 1];
        int nxt = pathIndices[i];
        bool isJump = false, isFall = false; float reqVx = 0.0f;
        auto edgeIt = s_EdgeData.find({prev, nxt});
        if (edgeIt != s_EdgeData.end() && edgeIt->second.action != 0) {
            if (edgeIt->second.action == 1) isJump = true;
            if (edgeIt->second.action == 2) isFall = true;
            reqVx = edgeIt->second.requiredVx;
            finalPath[i - 1].isJumpTakeoff = true; 
        }
        finalPath.push_back(PathNodeData{globalNavGraph[nxt].pos, isJump, isFall, false, reqVx});
    }
    return finalPath;
}

void EntitySystem::renderDebug(sf::RenderTarget& target) {
    float thickness = target.getView().getSize().x / target.getSize().x;
    if (thickness < 1.0f) thickness = 1.0f;
    
    // Draw Box2D Physics Bodies
    auto drawBody = [&](b2BodyId bodyId, sf::Color color) {
        if (!b2Body_IsValid(bodyId)) return;
        
        b2Transform xf = b2Body_GetTransform(bodyId);
        int shapeCount = b2Body_GetShapeCount(bodyId);
        if (shapeCount == 0) return;
        
        std::vector<b2ShapeId> shapes(shapeCount);
        b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
        
        for (int i = 0; i < shapeCount; ++i) {
            b2ShapeType type = b2Shape_GetType(shapes[i]);
            
            if (type == b2_polygonShape) {
                b2Polygon poly = b2Shape_GetPolygon(shapes[i]);
                sf::ConvexShape sfShape;
                sfShape.setPointCount(poly.count);
                
                for (int j = 0; j < poly.count; ++j) {
                    b2Vec2 worldPoint = b2TransformPoint(xf, poly.vertices[j]);
                    sfShape.setPoint(j, sf::Vector2f(worldPoint.x * M2P, worldPoint.y * M2P));
                }
                
                sfShape.setFillColor(sf::Color(color.r, color.g, color.b, 60)); 
                sfShape.setOutlineColor(color);
                sfShape.setOutlineThickness(thickness);
                target.draw(sfShape);
                
                // Draw rounded corners and expanded edges perfectly if the radius is > 0
                if (poly.radius > 0.0f) {
                    for (int j = 0; j < poly.count; ++j) {
                        b2Vec2 worldPoint = b2TransformPoint(xf, poly.vertices[j]);
                        sf::CircleShape corner(poly.radius * M2P);
                        corner.setOrigin({poly.radius * M2P, poly.radius * M2P});
                        corner.setPosition({worldPoint.x * M2P, worldPoint.y * M2P});
                        corner.setFillColor(sf::Color(color.r, color.g, color.b, 60));
                        corner.setOutlineColor(color);
                        corner.setOutlineThickness(thickness);
                        target.draw(corner);
                        
                        int next = (j + 1) % poly.count;
                        b2Vec2 p1 = poly.vertices[j];
                        b2Vec2 p2 = poly.vertices[next];
                        b2Vec2 normal = poly.normals[j];
                        
                        b2Vec2 edgeP1 = {p1.x + normal.x * poly.radius, p1.y + normal.y * poly.radius};
                        b2Vec2 edgeP2 = {p2.x + normal.x * poly.radius, p2.y + normal.y * poly.radius};
                        
                        b2Vec2 wp1 = b2TransformPoint(xf, edgeP1);
                        b2Vec2 wp2 = b2TransformPoint(xf, edgeP2);
                        
                        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
                        line[0].position = sf::Vector2f(wp1.x * M2P, wp1.y * M2P);
                        line[0].color = color;
                        line[1].position = sf::Vector2f(wp2.x * M2P, wp2.y * M2P);
                        line[1].color = color;
                        target.draw(line);
                    }
                }
                
            } else if (type == b2_capsuleShape) {
                b2Capsule capsule = b2Shape_GetCapsule(shapes[i]);
                b2Vec2 p1 = b2TransformPoint(xf, capsule.center1);
                b2Vec2 p2 = b2TransformPoint(xf, capsule.center2);
                
                sf::VertexArray line(sf::PrimitiveType::Lines, 2);
                line[0].position = sf::Vector2f(p1.x * M2P, p1.y * M2P);
                line[0].color = color;
                line[1].position = sf::Vector2f(p2.x * M2P, p2.y * M2P);
                line[1].color = color;
                target.draw(line);
                
                sf::CircleShape c1(capsule.radius * M2P);
                c1.setOrigin({capsule.radius * M2P, capsule.radius * M2P});
                c1.setPosition({p1.x * M2P, p1.y * M2P});
                c1.setFillColor(sf::Color(color.r, color.g, color.b, 60));
                c1.setOutlineColor(color);
                c1.setOutlineThickness(thickness);
                target.draw(c1);

                sf::CircleShape c2(capsule.radius * M2P);
                c2.setOrigin({capsule.radius * M2P, capsule.radius * M2P});
                c2.setPosition({p2.x * M2P, p2.y * M2P});
                c2.setFillColor(sf::Color(color.r, color.g, color.b, 60));
                c2.setOutlineColor(color);
                c2.setOutlineThickness(thickness);
                target.draw(c2);
            } else if (type == b2_circleShape) {
                b2Circle circle = b2Shape_GetCircle(shapes[i]);
                b2Vec2 center = b2TransformPoint(xf, circle.center);
                
                sf::CircleShape c(circle.radius * M2P);
                c.setOrigin({circle.radius * M2P, circle.radius * M2P});
                c.setPosition({center.x * M2P, center.y * M2P});
                c.setFillColor(sf::Color(color.r, color.g, color.b, 60));
                c.setOutlineColor(color);
                c.setOutlineThickness(thickness);
                target.draw(c);
            }
        }
    };

    for (const auto& e : entities) {
        drawBody(e->bodyId, sf::Color(255, 165, 0)); // Orange for main body
        drawBody(e->handA.ragdollBodyId, sf::Color::Magenta);
        drawBody(e->handB.ragdollBodyId, sf::Color::Magenta);
        
        if (e->getType() == 0) { // 0 = ComplexEntity
            ComplexEntity* ce = static_cast<ComplexEntity*>(e.get());
            drawBody(ce->legA.ragdollBodyId, sf::Color::Magenta);
            drawBody(ce->legB.ragdollBodyId, sf::Color::Magenta);
        }
    }

    // Draw NavMesh
    sf::VertexArray lines(sf::PrimitiveType::Lines);
    for (size_t i = 0; i < globalNavGraph.size(); ++i) {
        sf::Vector2f p1 = globalNavGraph[i].pos;
        for (int neighborIdx : globalNavGraph[i].neighbors) {
            auto it = s_EdgeData.find({static_cast<int>(i), neighborIdx});
            if (it != s_EdgeData.end() && it->second.action != 0 && !it->second.trajectory.empty()) {
                for (size_t t = 0; t + 1 < it->second.trajectory.size(); ++t) {
                    lines.append(sf::Vertex{it->second.trajectory[t], sf::Color(255, 0, 255, 120)});
                    lines.append(sf::Vertex{it->second.trajectory[t+1], sf::Color(255, 0, 255, 120)});
                }
            } else {
                sf::Vector2f p2 = globalNavGraph[neighborIdx].pos;
                lines.append(sf::Vertex{p1, sf::Color(0, 255, 0, 80)});
                lines.append(sf::Vertex{p2, sf::Color(0, 255, 0, 80)});
            }
        }
        lines.append(sf::Vertex{p1 + sf::Vector2f(-1, 0), sf::Color::Cyan});
        lines.append(sf::Vertex{p1 + sf::Vector2f(1, 0), sf::Color::Cyan});
    }
    for (const auto& line : debugLines) {
        lines.append(sf::Vertex{line.p1, line.color});
        lines.append(sf::Vertex{line.p2, line.color});
    }
    if (lines.getVertexCount() > 0) target.draw(lines);
}

void EntitySystem::drawPixelatedHand(sf::RenderTarget& target, const sf::Vector2f& center, sf::Color col) {
    int cx = static_cast<int>(std::round(center.x));
    int cy = static_cast<int>(std::round(center.y));
    sf::VertexArray pixels(sf::PrimitiveType::Triangles);
    auto addPixel = [&](int px, int py) {
        sf::Vector2f tl(static_cast<float>(px), static_cast<float>(py));
        sf::Vector2f tr(static_cast<float>(px + 1), static_cast<float>(py));
        sf::Vector2f br(static_cast<float>(px + 1), static_cast<float>(py + 1));
        sf::Vector2f bl(static_cast<float>(px), static_cast<float>(py + 1));
        pixels.append(sf::Vertex{tl, col}); pixels.append(sf::Vertex{tr, col}); pixels.append(sf::Vertex{br, col});
        pixels.append(sf::Vertex{tl, col}); pixels.append(sf::Vertex{br, col}); pixels.append(sf::Vertex{bl, col});
    };
    addPixel(cx, cy - 1); addPixel(cx - 1, cy); addPixel(cx, cy); addPixel(cx + 1, cy); addPixel(cx, cy + 1);
    target.draw(pixels);
}

void EntitySystem::drawPixelatedLeg(sf::RenderTarget& target, const sf::Vector2f& hip, const sf::Vector2f& foot, sf::Color col) {
    int x0 = static_cast<int>(std::round(hip.x)); int y0 = static_cast<int>(std::round(hip.y));
    int x1 = static_cast<int>(std::round(foot.x)); int y1 = static_cast<int>(std::round(foot.y));
    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1,  sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    sf::VertexArray pixels(sf::PrimitiveType::Triangles);
    while (true) {
        sf::Vector2f tl(static_cast<float>(x0), static_cast<float>(y0));
        sf::Vector2f tr(static_cast<float>(x0 + 1), static_cast<float>(y0));
        sf::Vector2f br(static_cast<float>(x0 + 1), static_cast<float>(y0 + 1));
        sf::Vector2f bl(static_cast<float>(x0), static_cast<float>(y0 + 1));
        pixels.append(sf::Vertex{tl, col}); pixels.append(sf::Vertex{tr, col}); pixels.append(sf::Vertex{br, col});
        pixels.append(sf::Vertex{tl, col}); pixels.append(sf::Vertex{br, col}); pixels.append(sf::Vertex{bl, col});
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
    target.draw(pixels);
}

void EntitySystem::renderEntities(sf::RenderTarget& target) {
    for (auto& e : entities) e->render(target);
}

sf::Vector2f EntitySystem::getPlayerPos() const {
    if (!entities.empty()) {
        b2Vec2 p = b2Body_GetPosition(entities.front()->bodyId);
        return {p.x * M2P, p.y * M2P};
    }
    return {0.f, 0.f};
}

void EntitySystem::killAndRagdollEntity(Entity* e, ParticleWorld& pw, uint8_t meatMaterial) {
    if (!e) return;
    b2Vec2 pos = b2Body_GetPosition(e->bodyId);
    sf::Image img;
    img.resize({5, 16}, sf::Color::Transparent);
    for (unsigned py = 0; py < 16; ++py)
        for (unsigned px = 0; px < 5; ++px)
            img.setPixel({px, py}, sf::Color::Red);
    pw.addRigidBodyFromSprite(img, int(pos.x * M2P - 2.5f), int(pos.y * M2P - e->colHalfH), static_cast<MaterialID>(meatMaterial));
    
    auto it = std::find_if(entities.begin(), entities.end(), [&](const std::unique_ptr<Entity>& ptr) { return ptr.get() == e; });
    if (it != entities.end()) entities.erase(it);
}

void EntitySystem::notifyTerrainChanged(sf::Vector2f center, float radius) {
    float r = radius + 20.0f;
    sf::FloatRect altered({center.x - r, center.y - r}, {r*2, r*2});
    if (!hasDirtyNavRegion) {
        dirtyNavRect = altered;
        hasDirtyNavRegion = true;
    } else {
        float left = std::min(dirtyNavRect.position.x, altered.position.x);
        float top = std::min(dirtyNavRect.position.y, altered.position.y);
        float right = std::max(dirtyNavRect.position.x + dirtyNavRect.size.x, altered.position.x + altered.size.x);
        float bottom = std::max(dirtyNavRect.position.y + dirtyNavRect.size.y, altered.position.y + altered.size.y);
        dirtyNavRect.position.x = left; dirtyNavRect.position.y = top;
        dirtyNavRect.size.x = right - left; dirtyNavRect.size.y = bottom - top;
    }
    globalGraphBuilt = false;
}