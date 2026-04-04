#include "RigidBody.hpp"
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <vector>

constexpr float PI = 3.14159265358979323846f;

RigidBody::RigidBody(b2WorldId worldId, int w, int h, const std::vector<LocalParticle>& parts, b2Vec2 pos, float angle, b2Vec2 linVel, float angVel) {
    this->worldId = worldId;
    this->width = w;
    this->height = h;
    this->needsFixtureRebuild = false;
    
    particles.resize(w * h);
    for (auto& p : particles) p.active = false; 
    
    for (const auto& p : parts) {
        particles[p.localY * w + p.localX] = p;
    }

    b2BodyDef bdef = b2DefaultBodyDef();
    bdef.type = b2_dynamicBody;
    bdef.position = pos;
    
    bdef.rotation = b2MakeRot(angle);
    bdef.linearVelocity = linVel;
    bdef.angularVelocity = angVel;
    bdef.angularDamping = 2.0f;
    bdef.linearDamping = 0.2f;
    bdef.enableSleep = true;
    bdef.isBullet = true; 
    bodyId = b2CreateBody(worldId, &bdef);

    rebuildFixtures();
}

std::vector<std::vector<LocalParticle>> RigidBody::findIslands() {
    std::vector<std::vector<LocalParticle>> islands;
    std::vector<bool> visited(width * height, false);
    
    int dirs[8][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,-1}, {1,-1}, {-1,1}};
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (particles[idx].active && !visited[idx]) {
                std::vector<LocalParticle> island;
                std::vector<int> queue;
                
                queue.push_back(idx);
                visited[idx] = true;
                
                int head = 0;
                while (head < queue.size()) {
                    int currIdx = queue[head++];
                    int cx = currIdx % width;
                    int cy = currIdx / width;
                    
                    island.push_back(particles[currIdx]);
                    
                    for (int d = 0; d < 8; ++d) {
                        int nx = cx + dirs[d][0];
                        int ny = cy + dirs[d][1];
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            int nIdx = ny * width + nx;
                            if (particles[nIdx].active && !visited[nIdx]) {
                                visited[nIdx] = true;
                                queue.push_back(nIdx);
                            }
                        }
                    }
                }
                islands.push_back(island);
            }
        }
    }
    return islands;
}

RigidBody::RigidBody(b2WorldId worldId, const sf::Image& img, int startX, int startY, MaterialID mat, bool weapon) {
    this->worldId = worldId;
    width = img.getSize().x;
    height = img.getSize().y;
    needsFixtureRebuild = false;
    
    isWeapon = weapon;
    isIndestructible = weapon; // Weapons bypass voxel erosion so they can't be melted

    particles.resize(width * height);
    for (auto& p : particles) p.active = false;

    for (unsigned int y = 0; y < height; ++y) {
        for (unsigned int x = 0; x < width; ++x) {
            sf::Color col = img.getPixel({x, y});
            if (col.a > 0) {
                LocalParticle p;
                p.localX = x;
                p.localY = y;
                p.lastWorldX = 0;
                p.lastWorldY = 0;
                p.active = true;
                
                p.base = BaseComponent(mat, col, ParticleFlags());
                p.base.flags.isRigidBodyPart = true;
                
                p.dur = DurabilityComponent(150, 2); 
                p.therm = ThermalComponent(0, 40, 10, 1);
                
                particles[y * width + x] = p;
            }
        }
    }

    b2BodyDef bdef = b2DefaultBodyDef();
    bdef.type = b2_dynamicBody;
    bdef.position = {startX * P2M, startY * P2M};
    bdef.angularDamping = 2.0f; 
    bdef.linearDamping = 0.2f;  
    bdef.enableSleep = true;
    bdef.isBullet = true; 
    bodyId = b2CreateBody(worldId, &bdef);

    rebuildFixtures();
}

void RigidBody::clearFromWorld(ParticleWorld& world) {
    for (auto& dp : drawnPixels) {
        Chunk* c = world.getChunk(dp.wx, dp.wy);
        if (c) {
            uint32_t idx = world.computeLocalIndex(dp.wx, dp.wy);
            if (c->base[idx].compMask != 0 && c->base[idx].flags.isRigidBodyPart) {
                c->base[idx].compMask = 0; 
                world.updateChunkPixel(c, idx, sf::Color::Transparent);
                world.wakeParticle(dp.wx, dp.wy);
            }
        }
    }
    drawnPixels.clear();
}

void RigidBody::renderPixelated(sf::RenderTarget& target, sf::Vector2f pos, float angleDeg, bool flipX, sf::Color overrideColor) {
    sf::VertexArray va(sf::PrimitiveType::Triangles);

    // Inverse Rotation (negative angle) for iterating over screen pixels mapped BACK to local texture
    float rad = -angleDeg * PI / 180.0f;
    float cs  = std::cos(rad);
    float sn  = std::sin(rad);

    // Determine the maximum possible radius a pixel could be from the center to form a bounds check
    float radius = std::max(width, height) * 0.7071f + 1.0f;
    int maxD = static_cast<int>(std::ceil(radius));

    for (int dy = -maxD; dy <= maxD; ++dy) {
        for (int dx = -maxD; dx <= maxD; ++dx) {
            // 1. Inverse rotate screen space back to local un-rotated space
            float rx = dx * cs - dy * sn;
            float ry = dx * sn + dy * cs;

            // 2. Inverse scale/flip
            if (flipX) rx = -rx;

            // 3. Translate from center to top-left local coordinates
            int lx = static_cast<int>(std::round(rx + width / 2.0f - 0.5f));
            int ly = static_cast<int>(std::round(ry + height / 2.0f - 0.5f));

            if (lx >= 0 && lx < width && ly >= 0 && ly < height) {
                const auto& p = particles[ly * width + lx];
                if (p.active) {
                    float px = pos.x + dx;
                    float py = pos.y + dy;

                    sf::Color col = (overrideColor == sf::Color::Transparent) ? p.base.color : overrideColor;

                    sf::Vector2f tl(px, py);
                    sf::Vector2f tr(px + 1.0f, py);
                    sf::Vector2f br(px + 1.0f, py + 1.0f);
                    sf::Vector2f bl(px, py + 1.0f);

                    va.append(sf::Vertex{tl, col}); va.append(sf::Vertex{tr, col}); va.append(sf::Vertex{br, col});
                    va.append(sf::Vertex{tl, col}); va.append(sf::Vertex{br, col}); va.append(sf::Vertex{bl, col});
                }
            }
        }
    }
    
    if (va.getVertexCount() > 0) target.draw(va);
}

void RigidBodySystem::renderDebug(sf::RenderTarget& target) const {
    float thickness = target.getView().getSize().x / target.getSize().x;
    
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
                
            } else if (type == b2_circleShape) {
                b2Circle circle = b2Shape_GetCircle(shapes[i]);
                b2Vec2 worldCenter = b2TransformPoint(xf, circle.center);
                
                sf::CircleShape sfCircle(circle.radius * M2P);
                sfCircle.setOrigin({circle.radius * M2P, circle.radius * M2P});
                sfCircle.setPosition({worldCenter.x * M2P, worldCenter.y * M2P});
                
                sfCircle.setFillColor(sf::Color(color.r, color.g, color.b, 60));
                sfCircle.setOutlineColor(color);
                sfCircle.setOutlineThickness(thickness);
                target.draw(sfCircle);
                
            } else if (type == b2_segmentShape) {
                // Support rendering the new ground segment lines!
                b2Segment segment = b2Shape_GetSegment(shapes[i]);
                b2Vec2 p1 = b2TransformPoint(xf, segment.point1);
                b2Vec2 p2 = b2TransformPoint(xf, segment.point2);
                
                sf::Vertex line[2];
                line[0].position = sf::Vector2f(p1.x * M2P, p1.y * M2P);
                line[0].color = color;
                line[1].position = sf::Vector2f(p2.x * M2P, p2.y * M2P);
                line[1].color = color;
                target.draw(line, 2, sf::PrimitiveType::Lines);
            }
        }
    };

    for (const auto& pair : chunkBodies) drawBody(pair.second.bodyId, sf::Color::Cyan);
    for (const auto& rb : bodies) drawBody(rb->bodyId, sf::Color::Green);
}

void RigidBody::rebuildFixtures() {
    int shapeCount = b2Body_GetShapeCount(bodyId);
    if (shapeCount > 0) {
        std::vector<b2ShapeId> shapes(shapeCount);
        b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
        for(int i = 0; i < shapeCount; ++i) {
            b2DestroyShape(shapes[i], true); 
        }
    }

    std::vector<bool> visited(width * height, false);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            
            if (particles[idx].active && !visited[idx]) {
                int w = 0;
                while (x + w < width && particles[y * width + x + w].active && !visited[y * width + x + w]) w++;
                
                int h = 1;
                bool canExpand = true;
                while (y + h < height && canExpand) {
                    for (int i = 0; i < w; ++i) {
                        int nIdx = (y + h) * width + x + i;
                        if (!particles[nIdx].active || visited[nIdx]) {
                            canExpand = false;
                            break;
                        }
                    }
                    if (canExpand) h++;
                }
                
                for (int j = 0; j < h; ++j) {
                    for (int i = 0; i < w; ++i) {
                        visited[(y + j) * width + x + i] = true;
                    }
                }
                
                float radius = 0.02f;
                float hx = (w / 2.0f) * P2M - radius;
                float hy = (h / 2.0f) * P2M - radius;
                if (hx < 0.01f) hx = 0.01f;
                if (hy < 0.01f) hy = 0.01f;
                
                float cx = x + (w / 2.0f) - (width / 2.0f);
                float cy = y + (h / 2.0f) - (height / 2.0f);
                
                b2Polygon box = b2MakeOffsetBox(hx, hy, {cx * P2M, cy * P2M}, b2MakeRot(0.0f));
                box.radius = radius;
                
                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.density = 2.0f;     
                shapeDef.material.friction = 0.1f;    
                shapeDef.material.restitution = 0.05f;
                
                b2CreatePolygonShape(bodyId, &shapeDef, &box); 
            }
        }
    }
    needsFixtureRebuild = false;
}

RigidBodySystem::RigidBodySystem() {
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, 98.0f};
    worldId = b2CreateWorld(&worldDef);
    chunkBodies.clear();
}

RigidBodySystem::~RigidBodySystem() {
    b2DestroyWorld(worldId);
}

void RigidBodySystem::addRigidBodyFromSprite(const sf::Image& img, int x, int y, MaterialID mat) {
    bodies.push_back(std::make_unique<RigidBody>(worldId, img, x, y, mat, false));
}

void RigidBodySystem::addWeapon(const sf::Image& img, int x, int y) {
    bodies.push_back(std::make_unique<RigidBody>(worldId, img, x, y, MaterialID::Sand, true));
}

RigidBody* RigidBodySystem::getNearestWeapon(sf::Vector2f pos, float radius) {
    RigidBody* nearest = nullptr;
    float minDist = radius;

    for (auto& rb : bodies) {
        if (!rb->isWeapon || rb->isEquipped) continue;
        
        b2Vec2 bp = b2Body_GetPosition(rb->bodyId);
        float dist = std::hypot(bp.x * M2P - pos.x, bp.y * M2P - pos.y);
        
        if (dist < minDist) {
            minDist = dist;
            nearest = rb.get();
        }
    }
    return nearest;
}

void RigidBodySystem::renderWeaponsOutline(sf::RenderTarget& target, sf::Vector2f playerPos) {
    for (auto& rb : bodies) {
        if (!rb->isWeapon || rb->isEquipped) continue;
        
        b2Vec2 bp = b2Body_GetPosition(rb->bodyId);
        sf::Vector2f wp(bp.x * M2P, bp.y * M2P);
        
        // Draw outline if player is close enough to pick it up
        if (std::hypot(wp.x - playerPos.x, wp.y - playerPos.y) < 40.0f) {
            b2Transform xf = b2Body_GetTransform(rb->bodyId);
            float ang = std::atan2(xf.q.s, xf.q.c) * 180.f / PI;
            
            // Render 4 times slightly shifted to create a pixel-perfect outline effect
            rb->renderPixelated(target, wp + sf::Vector2f( 1,  0), ang, false, sf::Color::Red);
            rb->renderPixelated(target, wp + sf::Vector2f(-1,  0), ang, false, sf::Color::Red);
            rb->renderPixelated(target, wp + sf::Vector2f( 0,  1), ang, false, sf::Color::Red);
            rb->renderPixelated(target, wp + sf::Vector2f( 0, -1), ang, false, sf::Color::Red);
        }
    }
}

void RigidBodySystem::clearFromWorld(ParticleWorld& world) {
    for (auto& rb : bodies) {
        rb->clearFromWorld(world);
    }
    
    for (auto& dp : orphanedPixels) {
        Chunk* c = world.getChunk(dp.wx, dp.wy);
        if (c) {
            uint32_t idx = world.computeLocalIndex(dp.wx, dp.wy);
            if (c->base[idx].compMask != 0 && c->base[idx].flags.isRigidBodyPart) {
                c->base[idx].compMask = 0; 
                world.updateChunkPixel(c, idx, sf::Color::Transparent);
                world.wakeParticle(dp.wx, dp.wy);
            }
        }
    }
    orphanedPixels.clear(); 
}

void RigidBodySystem::stepPhysics(float dt, ParticleWorld& world) {
    std::unordered_set<ChunkCoord, ChunkCoordHash> overlappingChunks;
    
    for (auto& rb : bodies) {
        if (rb->isEquipped) continue; // Skip physical calculation if carried by a player

        b2Transform transform = b2Body_GetTransform(rb->bodyId);
        float radiusM = std::max(rb->width, rb->height) * P2M * 0.707f + 2.0f; 
        
        int minCx = static_cast<int>((transform.p.x - radiusM) * M2P) >> 6;
        int maxCx = static_cast<int>((transform.p.x + radiusM) * M2P) >> 6;
        int minCy = static_cast<int>((transform.p.y - radiusM) * M2P) >> 6;
        int maxCy = static_cast<int>((transform.p.y + radiusM) * M2P) >> 6;
        
        bool needsWake = false;

        for (int cy = minCy; cy <= maxCy; ++cy) {
            for (int cx = minCx; cx <= maxCx; ++cx) {
                ChunkCoord coord{cx, cy};
                overlappingChunks.insert(coord);
                
                Chunk* c = world.getChunk(cx << 6, cy << 6);
                if (c && (!c->isSleeping || c->visualDirty)) {
                    needsWake = true;
                }
            }
        }

        if (needsWake && !b2Body_IsAwake(rb->bodyId)) {
            b2Body_SetAwake(rb->bodyId, true);
        }
    }
    
    for (auto it = chunkBodies.begin(); it != chunkBodies.end(); ) {
        if (overlappingChunks.find(it->first) == overlappingChunks.end()) {
            b2DestroyBody(it->second.bodyId);
            it = chunkBodies.erase(it);
        } else {
            ++it;
        }
    }

    for (auto coord : overlappingChunks) {
        Chunk* c = world.getChunk(coord.x << 6, coord.y << 6);
        bool hasTerrain = chunkBodies.count(coord) > 0;
        if (!hasTerrain || (c && (!c->isSleeping || c->visualDirty))) {
            rebuildChunkTerrain(coord, c, world);
        }
    }
    
    b2World_Step(worldId, dt, 8);
}

void RigidBodySystem::rasterizeToWorld(ParticleWorld& world) {
    
    auto isBlocking = [&](int x, int y) {
        BaseComponent* base = world.get<BaseComponent>(x, y);
        if (!base || base->compMask == 0 || base->flags.isRigidBodyPart) return false;
        Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
        return logic && logic->getGroup() == MaterialGroup::ImmovableSolid;
    };

    for (auto& rb : bodies) {
        if (rb->isEquipped) continue; // Don't put pixels on the map if carried

        rb->drawnPixels.clear();
        
        b2Transform transform = b2Body_GetTransform(rb->bodyId);
        b2Vec2 b2vel = b2Body_GetLinearVelocity(rb->bodyId);
        float velX = b2vel.x * M2P;
        float velY = b2vel.y * M2P;
        
        float radiusP = std::max(rb->width, rb->height) * 0.7071f + 1.0f; 
        
        int minWx = static_cast<int>(std::floor(transform.p.x * M2P - radiusP));
        int maxWx = static_cast<int>(std::ceil(transform.p.x * M2P + radiusP));
        int minWy = static_cast<int>(std::floor(transform.p.y * M2P - radiusP));
        int maxWy = static_cast<int>(std::ceil(transform.p.y * M2P + radiusP));

        float cs = transform.q.c;
        float sn = transform.q.s;

        int waterOverlap = 0;
        int sandOverlap = 0;

        for (int wy = minWy; wy <= maxWy; ++wy) {
            for (int wx = minWx; wx <= maxWx; ++wx) {
                float dx = wx / M2P - transform.p.x;
                float dy = wy / M2P - transform.p.y;
                
                float rx = dx * cs + dy * sn;
                float ry = -dx * sn + dy * cs;
                
                int lx = static_cast<int>(std::round(rx * M2P + rb->width / 2.0f - 0.5f));
                int ly = static_cast<int>(std::round(ry * M2P + rb->height / 2.0f - 0.5f));
                
                if (lx >= 0 && lx < rb->width && ly >= 0 && ly < rb->height) {
                    int localIdx = ly * rb->width + lx;
                    LocalParticle& p = rb->particles[localIdx];
                    if (p.active) {
                        Chunk* c = world.getOrCreateChunk(wx, wy);
                        if (c) {
                            uint32_t idx = world.computeLocalIndex(wx, wy);
                            bool canPlace = false;
                            
                            if (c->base[idx].compMask == 0) {
                                canPlace = true;
                            } else if (!c->base[idx].flags.isRigidBodyPart) {
                                MaterialID matId = c->base[idx].id;
                                Particle* logic = MaterialRegistry[static_cast<int>(matId)];
                                
                                if (logic) {
                                    MaterialGroup group = logic->getGroup();
                                    
                                    if (group == MaterialGroup::Liquid || group == MaterialGroup::MovableSolid || group == MaterialGroup::Gas) {
                                        
                                        if (group == MaterialGroup::Liquid) waterOverlap++;
                                        else if (group == MaterialGroup::MovableSolid) sandOverlap++;

                                        int spawnX = wx, spawnY = wy;
                                        bool found = false;
                                        
                                        for (int d = 1; d <= 2; ++d) {
                                            if (world.isEmpty(wx, wy - d)) { spawnX = wx; spawnY = wy - d; found = true; break; }
                                            if (world.isEmpty(wx - d, wy)) { spawnX = wx - d; spawnY = wy; found = true; break; }
                                            if (world.isEmpty(wx + d, wy)) { spawnX = wx + d; spawnY = wy; found = true; break; }
                                        }
                                        
                                        if (!found) {
                                            int upY = wy - 1;
                                            for(int i = 0; i < 150; i++) {
                                                if (world.isEmpty(wx, upY)) { 
                                                    spawnX = wx; spawnY = upY; found = true; break; 
                                                } else {
                                                    BaseComponent* b = world.get<BaseComponent>(wx, upY);
                                                    if (b && !b->flags.isRigidBodyPart) {
                                                        Particle* pLogic = MaterialRegistry[static_cast<int>(b->id)];
                                                        if (pLogic && pLogic->getGroup() == MaterialGroup::ImmovableSolid) break; 
                                                    }
                                                }
                                                upY--;
                                            }
                                        }

                                        if (!found) {
                                            for (int dir : {-1, 1}) {
                                                int sideX = wx + dir;
                                                for(int i = 0; i < 30; i++) {
                                                    if (world.isEmpty(sideX, wy)) { 
                                                        spawnX = sideX; spawnY = wy; found = true; break; 
                                                    } else {
                                                        BaseComponent* b = world.get<BaseComponent>(sideX, wy);
                                                        if (b && !b->flags.isRigidBodyPart) {
                                                            Particle* pLogic = MaterialRegistry[static_cast<int>(b->id)];
                                                            if (pLogic && pLogic->getGroup() == MaterialGroup::ImmovableSolid) break; 
                                                        }
                                                    }
                                                    sideX += dir;
                                                }
                                                if (found) break;
                                            }
                                        }

                                        if (found) {
                                            world.moveParticle(wx, wy, spawnX, spawnY);
                                            if (auto* kin = world.get<KinematicsComponent>(spawnX, spawnY)) {
                                                kin->isFreeFalling = true;
                                            }
                                            canPlace = true; 
                                        } else {
                                            world.removeParticle(wx, wy);
                                            canPlace = true;
                                        }
                                    }
                                }
                            }

                            if (canPlace) {
                                p.base.flags.isRigidBodyPart = true;
                                world.add<BaseComponent>(wx, wy, p.base);
                                world.add<DurabilityComponent>(wx, wy, p.dur);
                                world.add<ThermalComponent>(wx, wy, p.therm);
                                world.setParticleColor(wx, wy, p.base.color);
                                world.wakeParticle(wx, wy);
                                
                                rb->drawnPixels.push_back({wx, wy, localIdx});
                            }
                        }
                    }
                }
            }
        }
        
        float totalPixels = rb->width * rb->height;
        float waterRatio = std::min((float)waterOverlap / totalPixels, 1.0f);
        float sandRatio = std::min((float)sandOverlap / totalPixels, 1.0f);
        
        float linearDrag = 0.2f;   
        float angularDrag = 2.0f;  

        if (waterRatio > 0.0f) {
            linearDrag += waterRatio * 15.0f; 
            angularDrag += waterRatio * 20.0f;
            float upwardAccel = waterRatio * 85.0f;
            b2Vec2 force = {0.0f, -upwardAccel * b2Body_GetMass(rb->bodyId)};
            b2Body_ApplyForceToCenter(rb->bodyId, force, true);
        }

        if (sandRatio > 0.0f) {
            linearDrag += sandRatio * 0.1f; 
            angularDrag += sandRatio * 0.5f;
        }

        b2Body_SetLinearDamping(rb->bodyId, linearDrag);
        b2Body_SetAngularDamping(rb->bodyId, angularDrag);
    }
}

void RigidBodySystem::syncFromWorld(ParticleWorld& world) {
    std::vector<std::unique_ptr<RigidBody>> newBodies;
    
    for (auto it = bodies.begin(); it != bodies.end(); ) {
        RigidBody* rb = it->get();
        if (rb->isEquipped) {
            ++it;
            continue; // Skip synchronizing destruction if it's currently held
        }

        bool needsRebuild = false;
        
        for (auto& dp : rb->drawnPixels) {
            LocalParticle& p = rb->particles[dp.localIdx];
            if (!p.active) continue; 
            
            Chunk* c = world.getChunk(dp.wx, dp.wy);
            if (c) {
                uint32_t idx = world.computeLocalIndex(dp.wx, dp.wy);
                if (c->base[idx].compMask == 0 || !c->base[idx].flags.isRigidBodyPart) {
                    if (!rb->isIndestructible) {
                        p.active = false;
                        needsRebuild = true;
                    }
                } else {
                    p.base = c->base[idx];
                    if (c->base[idx].compMask & COMP_DURABILITY) p.dur = c->durability[idx];
                    if (c->base[idx].compMask & COMP_THERMAL) p.therm = c->thermal[idx];
                }
            } else {
                if (!rb->isIndestructible) {
                    p.active = false;
                    needsRebuild = true;
                }
            }
        }
        
        if (needsRebuild) {
            std::vector<std::vector<LocalParticle>> islands = rb->findIslands();
            
            if (islands.empty()) {
                orphanedPixels.insert(orphanedPixels.end(), rb->drawnPixels.begin(), rb->drawnPixels.end());
                b2DestroyBody(rb->bodyId);
                it = bodies.erase(it);
                continue;
            } else if (islands.size() == 1) {
                rb->rebuildFixtures();
            } else {
                orphanedPixels.insert(orphanedPixels.end(), rb->drawnPixels.begin(), rb->drawnPixels.end());
                
                b2Transform xf = b2Body_GetTransform(rb->bodyId);
                b2Vec2 linVel = b2Body_GetLinearVelocity(rb->bodyId);
                float angVel = b2Body_GetAngularVelocity(rb->bodyId);
                
                for (const auto& island : islands) {
                    int minX = rb->width, maxX = 0;
                    int minY = rb->height, maxY = 0;
                    for (const auto& p : island) {
                        if (p.localX < minX) minX = p.localX;
                        if (p.localX > maxX) maxX = p.localX;
                        if (p.localY < minY) minY = p.localY;
                        if (p.localY > maxY) maxY = p.localY;
                    }
                    
                    int newW = maxX - minX + 1;
                    int newH = maxY - minY + 1;
                    
                    std::vector<LocalParticle> newParts = island;
                    for (auto& p : newParts) {
                        p.localX -= minX;
                        p.localY -= minY;
                    }
                    
                    float dx_m = ((float)minX + newW / 2.0f - rb->width / 2.0f) * P2M;
                    float dy_m = ((float)minY + newH / 2.0f - rb->height / 2.0f) * P2M;
                    
                    float cs = xf.q.c;
                    float sn = xf.q.s;
                    float ox_world = dx_m * cs - dy_m * sn;
                    float oy_world = dx_m * sn + dy_m * cs;
                    
                    b2Vec2 newPos = { xf.p.x + ox_world, xf.p.y + oy_world };
                    
                    b2Vec2 newLinVel = {
                        linVel.x - angVel * oy_world,
                        linVel.y + angVel * ox_world
                    };
                    
                    float currentAngle = std::atan2(xf.q.s, xf.q.c);
                    
                    newBodies.push_back(std::make_unique<RigidBody>(
                        worldId, newW, newH, newParts, newPos, currentAngle, newLinVel, angVel
                    ));
                }
                
                b2DestroyBody(rb->bodyId);
                it = bodies.erase(it);
                continue; 
            }
        }
        ++it;
    }
    
    for (auto& nb : newBodies) {
        bodies.push_back(std::move(nb));
    }
}

// --- HELPER MATH FOR SIMPLIFICATION ---
namespace {
    struct IntPoint {
        int x, y;
        bool operator==(const IntPoint& o) const { return x == o.x && y == o.y; }
    };

    struct IntPointHash {
        size_t operator()(const IntPoint& p) const {
            return std::hash<int>()(p.x) ^ (std::hash<int>()(p.y) << 1);
        }
    };

    float perpendicularDistance(IntPoint p, IntPoint p1, IntPoint p2) {
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float mag = std::sqrt(dx*dx + dy*dy);
        if (mag > 0.0f) {
            return std::abs((p2.x - p1.x)*(p1.y - p.y) - (p1.x - p.x)*(p2.y - p1.y)) / mag;
        }
        dx = p.x - p1.x;
        dy = p.y - p1.y;
        return std::sqrt(dx*dx + dy*dy);
    }

    void simplifyPath(const std::vector<IntPoint>& path, int start, int end, float epsilon, std::vector<bool>& keep) {
        float maxDist = 0.0f;
        int index = start;
        for (int i = start + 1; i < end; ++i) {
            float dist = perpendicularDistance(path[i], path[start], path[end]);
            if (dist > maxDist) {
                maxDist = dist;
                index = i;
            }
        }
        if (maxDist > epsilon) {
            keep[index] = true;
            simplifyPath(path, start, index, epsilon, keep);
            simplifyPath(path, index, end, epsilon, keep);
        }
    }

    std::vector<IntPoint> simplify(const std::vector<IntPoint>& path, float epsilon) {
        if (path.size() < 3) return path;
        std::vector<bool> keep(path.size(), false);
        keep[0] = true;
        keep[path.size() - 1] = true;
        simplifyPath(path, 0, path.size() - 1, epsilon, keep);
        
        std::vector<IntPoint> result;
        for (size_t i = 0; i < path.size(); ++i) {
            if (keep[i]) result.push_back(path[i]);
        }
        return result;
    }
}
// ----------------------------------------

void RigidBodySystem::rebuildChunkTerrain(ChunkCoord coord, Chunk* chunk, ParticleWorld& world) {
    uint64_t currentHash = 0;
    if (chunk) {
        for (int i = 0; i < CHUNK_AREA; ++i) {
            if (chunk->base[i].compMask != 0 && !chunk->base[i].flags.isRigidBodyPart) {
                Particle* logic = MaterialRegistry[static_cast<int>(chunk->base[i].id)];
                if (logic && logic->getGroup() == MaterialGroup::ImmovableSolid) {
                    currentHash ^= (uint64_t)(i + 1) * 2654435761ull; 
                }
            }
        }
    }

    if (chunkBodies.count(coord) && chunkBodies[coord].hash == currentHash) return; 

    if (chunkBodies.count(coord)) {
        b2DestroyBody(chunkBodies[coord].bodyId);
        chunkBodies.erase(coord);
    }

    if (!chunk || currentHash == 0) return;

    b2BodyDef bdef = b2DefaultBodyDef();
    bdef.type = b2_staticBody;
    bdef.position = { 0.0f, 0.0f }; 
    b2BodyId bodyId = b2CreateBody(worldId, &bdef);

    auto isSolid = [&](int wx, int wy) {
        BaseComponent* base = world.get<BaseComponent>(wx, wy);
        if (!base || base->compMask == 0 || base->flags.isRigidBodyPart) return false;
        Particle* logic = MaterialRegistry[static_cast<int>(base->id)];
        return logic && logic->getGroup() == MaterialGroup::ImmovableSolid;
    };

    struct Edge { IntPoint p1, p2; };
    std::vector<Edge> all_edges;
    int chunkWx = coord.x * CHUNK_SIZE;
    int chunkWy = coord.y * CHUNK_SIZE;

    for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            int wx = chunkWx + lx;
            int wy = chunkWy + ly;
            
            if (isSolid(wx, wy)) {
                if (!isSolid(wx, wy - 1))     all_edges.push_back({{wx, wy}, {wx + 1, wy}});
                if (!isSolid(wx + 1, wy))     all_edges.push_back({{wx + 1, wy}, {wx + 1, wy + 1}});
                if (!isSolid(wx, wy + 1))     all_edges.push_back({{wx + 1, wy + 1}, {wx, wy + 1}});
                if (!isSolid(wx - 1, wy))     all_edges.push_back({{wx, wy + 1}, {wx, wy}});
            }
        }
    }

    std::unordered_map<IntPoint, int, IntPointHash> inDegree;
    std::unordered_map<IntPoint, int, IntPointHash> outDegree;
    std::unordered_multimap<IntPoint, IntPoint, IntPointHash> adj;

    for (const auto& edge : all_edges) {
        adj.insert({edge.p1, edge.p2});
        outDegree[edge.p1]++;
        inDegree[edge.p2]++;
    }

    std::vector<std::vector<IntPoint>> polylines;

    for (auto& pair : outDegree) {
        IntPoint start = pair.first;
        while (outDegree[start] > inDegree[start]) {
            std::vector<IntPoint> path;
            path.push_back(start);
            IntPoint curr = start;
            while (true) {
                auto it = adj.find(curr);
                if (it == adj.end()) break;
                IntPoint next = it->second;
                path.push_back(next);
                adj.erase(it);
                outDegree[curr]--;
                inDegree[next]--;
                curr = next;
            }
            polylines.push_back(path);
        }
    }

    for (auto& pair : outDegree) {
        IntPoint start = pair.first;
        while (outDegree[start] > 0) {
            std::vector<IntPoint> path;
            path.push_back(start);
            IntPoint curr = start;
            while (true) {
                auto it = adj.find(curr);
                if (it == adj.end()) break;
                IntPoint next = it->second;
                path.push_back(next);
                adj.erase(it);
                outDegree[curr]--;
                inDegree[next]--;
                curr = next;
                if (curr == start) break;
            }
            polylines.push_back(path);
        }
    }

    bool hasFixtures = false;

    for (const auto& poly : polylines) {
        std::vector<IntPoint> simplified = simplify(poly, 0.8f);
        if (simplified.size() < 2) continue;

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.material.friction = 0.5f; 

        for (size_t i = 0; i < simplified.size() - 1; ++i) {
            b2Segment segment = { 
                {simplified[i].x * P2M, simplified[i].y * P2M}, 
                {simplified[i+1].x * P2M, simplified[i+1].y * P2M} 
            };
            b2CreateSegmentShape(bodyId, &shapeDef, &segment);
            hasFixtures = true;
        }
    }

    if (hasFixtures) {
        chunkBodies[coord] = {bodyId, currentHash};
    } else {
        b2DestroyBody(bodyId);
    }
}

void RigidBodySystem::save(std::ostream& out) const {
    size_t count = bodies.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    for (const auto& rb : bodies) {
        b2Transform xf = b2Body_GetTransform(rb->bodyId);
        b2Vec2 linVel = b2Body_GetLinearVelocity(rb->bodyId);
        float angVel = b2Body_GetAngularVelocity(rb->bodyId);
        float angle = std::atan2(xf.q.s, xf.q.c);
        
        out.write(reinterpret_cast<const char*>(&xf.p), sizeof(b2Vec2));
        out.write(reinterpret_cast<const char*>(&angle), sizeof(float));
        out.write(reinterpret_cast<const char*>(&linVel), sizeof(b2Vec2));
        out.write(reinterpret_cast<const char*>(&angVel), sizeof(float));
        
        out.write(reinterpret_cast<const char*>(&rb->width), sizeof(int));
        out.write(reinterpret_cast<const char*>(&rb->height), sizeof(int));
        
        size_t pCount = rb->particles.size();
        out.write(reinterpret_cast<const char*>(&pCount), sizeof(pCount));
        
        if (pCount > 0) {
            out.write(reinterpret_cast<const char*>(rb->particles.data()), pCount * sizeof(LocalParticle));
        }
    }
}

void RigidBodySystem::load(std::istream& in) {
    clearAll();
    size_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    for (size_t i = 0; i < count; ++i) {
        b2Vec2 pos, linVel;
        float angle, angVel;
        int w, h;
        
        in.read(reinterpret_cast<char*>(&pos), sizeof(b2Vec2));
        in.read(reinterpret_cast<char*>(&angle), sizeof(float));
        in.read(reinterpret_cast<char*>(&linVel), sizeof(b2Vec2));
        in.read(reinterpret_cast<char*>(&angVel), sizeof(float));
        in.read(reinterpret_cast<char*>(&w), sizeof(int));
        in.read(reinterpret_cast<char*>(&h), sizeof(int));
        
        size_t pCount = 0;
        in.read(reinterpret_cast<char*>(&pCount), sizeof(pCount));
        
        std::vector<LocalParticle> parts(pCount);
        if (pCount > 0) {
            in.read(reinterpret_cast<char*>(parts.data()), pCount * sizeof(LocalParticle));
        }
        
        bodies.push_back(std::make_unique<RigidBody>(worldId, w, h, parts, pos, angle, linVel, angVel));
    }
}

void RigidBodySystem::clearAll() {
    for (auto& rb : bodies) {
        if (b2Body_IsValid(rb->bodyId)) b2DestroyBody(rb->bodyId);
    }
    bodies.clear();
    
    for (auto& pair : chunkBodies) {
        if (b2Body_IsValid(pair.second.bodyId)) b2DestroyBody(pair.second.bodyId);
    }
    chunkBodies.clear();
    orphanedPixels.clear();
}