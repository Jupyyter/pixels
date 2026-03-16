#include "RigidBody.hpp"
#include <cmath>
#include <unordered_set>
RigidBody::RigidBody(b2WorldId worldId, int w, int h, const std::vector<LocalParticle>& parts, b2Vec2 pos, float angle, b2Vec2 linVel, float angVel) {
    this->worldId = worldId;
    this->width = w;
    this->height = h;
    this->needsFixtureRebuild = false;
    
    // Initialize empty grid
    particles.resize(w * h);
    for (auto& p : particles) p.active = false; 
    
    // Populate the new fragment's grid
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
    bodyId = b2CreateBody(worldId, &bdef);

    rebuildFixtures();
}

std::vector<std::vector<LocalParticle>> RigidBody::findIslands() {
    std::vector<std::vector<LocalParticle>> islands;
    std::vector<bool> visited(width * height, false);
    
    // 8-way directional checks so diagonal pixels stick together!
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
// No changes to RigidBody::RigidBody or RigidBody::rebuildFixtures
RigidBody::RigidBody(b2WorldId worldId, const sf::Image& img, int startX, int startY, MaterialID mat) {
    this->worldId = worldId;
    width = img.getSize().x;
    height = img.getSize().y;
    needsFixtureRebuild = false;

    // FIX 1: Initialize the exact grid size so 2D indexing maps perfectly!
    particles.resize(width * height);
    for (auto& p : particles) p.active = false;

    // Convert Sprite to Local Particle Grid
    for (unsigned int y = 0; y < height; ++y) {
        for (unsigned int x = 0; x < width; ++x) {
            sf::Color col = img.getPixel({x, y});
            if (col.a > 0) {
                LocalParticle p;
                p.localX = x;
                p.localY = y;
                p.active = true;
                
                p.base = BaseComponent(mat, col, ParticleFlags());
                p.base.flags.isRigidBodyPart = true;
                
                p.dur = DurabilityComponent(150, 2); 
                p.therm = ThermalComponent(0, 40, 10, 1);
                
                // Assign to the correct memory slot instead of push_back
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
    bodyId = b2CreateBody(worldId, &bdef);

    rebuildFixtures();
}
void RigidBodySystem::renderDebug(sf::RenderTarget& target) const {
    // Keep outlines consistent regardless of camera zoom
    float thickness = target.getView().getSize().x / target.getSize().x;
    
    auto drawBody = [&](b2BodyId bodyId, sf::Color color) {
        if (!b2Body_IsValid(bodyId)) return;
        
        // FIX: Box2D 3.x only requires the bodyId here!
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
                
                sfShape.setFillColor(sf::Color(color.r, color.g, color.b, 60)); // Transparent fill
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
            }
        }
    };

    // 1. Draw Static Terrain Chunks (Cyan)
    for (const auto& pair : chunkBodies) drawBody(pair.second, sf::Color::Cyan);
    
    // 2. Draw Dynamic Rigid Bodies (Green)
    for (const auto& rb : bodies) drawBody(rb->bodyId, sf::Color::Green);
}
void RigidBody::rebuildFixtures() {
    // 1. Clear old Box2D shapes from the body
    int shapeCount = b2Body_GetShapeCount(bodyId);
    if (shapeCount > 0) {
        std::vector<b2ShapeId> shapes(shapeCount);
        b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
        for(int i = 0; i < shapeCount; ++i) {
            // FIX: Box2D 3.x requires a boolean to update the body mass when deleting a shape
            b2DestroyShape(shapes[i], true); 
        }
    }

    // 2. Build local 2D grid
    std::vector<bool> localGrid(width * height, false);
    for (const auto& p : particles) {
        if (p.active) localGrid[p.localY * width + p.localX] = true;
    }

    // 3. Run-Length Encoding to combine adjacent particles
    for (int ly = 0; ly < height; ++ly) {
        int startX = -1;
        for (int lx = 0; lx <= width; ++lx) {
            bool solid = (lx < width) && localGrid[ly * width + lx];
            if (solid && startX == -1) {
                startX = lx;
            } else if (!solid && startX != -1) {
                float w = (lx - startX);
                float h = 1.0f;
                
                // Box2D uses Half-Width and Half-Height!
                float hx = (w / 2.0f) * P2M;
                float hy = (h / 2.0f) * P2M;
                
                // Center point of this specific box
                float cx = startX + (w / 2.0f) - (width / 2.0f);
                float cy = ly + (h / 2.0f) - (height / 2.0f);
                
                b2Polygon box = b2MakeOffsetBox(hx, hy, {cx * P2M, cy * P2M}, b2MakeRot(0.0f));
                
                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.density = 5.0f;
                shapeDef.material.friction = 0.8f;
                shapeDef.material.restitution = 0.05f;
                
                // FIX: Box2D 3.x API uses b2CreatePolygonShape
                b2CreatePolygonShape(bodyId, &shapeDef, &box); 
                
                startX = -1;
            }
        }
    }
    
    needsFixtureRebuild = false;
}

// No change to RigidBodySystem constructor
RigidBodySystem::RigidBodySystem() {
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, 98.0f};
    worldId = b2CreateWorld(&worldDef);
    chunkBodies.clear();
}

// --- FINAL FIX ---
RigidBodySystem::~RigidBodySystem() {
    // We own the worldId, and it's valid for our entire lifetime.
    // No check is needed (or possible); just destroy it.
    b2DestroyWorld(worldId);
}

// No changes to any of the following functions
void RigidBodySystem::addRigidBodyFromSprite(const sf::Image& img, int x, int y, MaterialID mat) {
    bodies.push_back(std::make_unique<RigidBody>(worldId, img, x, y, mat));
}

void RigidBodySystem::clearFromWorld(ParticleWorld& world) {
    // 1. Clear active bodies
    for (auto& rb : bodies) {
        for (auto& dp : rb->drawnPixels) {
            Chunk* c = world.getChunk(dp.wx, dp.wy);
            if (c) {
                uint32_t idx = world.computeLocalIndex(dp.wx, dp.wy);
                // Only clear it if it is STILL a rigid body pixel
                if (c->base[idx].compMask != 0 && c->base[idx].flags.isRigidBodyPart) {
                    c->base[idx].compMask = 0; // Wipe from world
                    world.updateChunkPixel(c, idx, sf::Color::Transparent);
                    world.wakeParticle(dp.wx, dp.wy);
                }
            }
        }
        rb->drawnPixels.clear();
    }
    
    // 2. NEW: Clear orphaned ghost pixels (from bodies that split or died last frame)
    for (auto& dp : orphanedPixels) {
        Chunk* c = world.getChunk(dp.wx, dp.wy);
        if (c) {
            uint32_t idx = world.computeLocalIndex(dp.wx, dp.wy);
            // Only clear it if it is STILL a rigid body pixel (prevents erasing new sand that fell here)
            if (c->base[idx].compMask != 0 && c->base[idx].flags.isRigidBodyPart) {
                c->base[idx].compMask = 0; 
                world.updateChunkPixel(c, idx, sf::Color::Transparent);
                world.wakeParticle(dp.wx, dp.wy);
            }
        }
    }
    orphanedPixels.clear(); // Empty the trash bin!
}


void RigidBodySystem::stepPhysics(float dt, ParticleWorld& world) {
    std::unordered_set<ChunkCoord, ChunkCoordHash> overlappingChunks;
    
    // 1. Gather all chunks that bodies currently touch and wake them if necessary
    for (auto& rb : bodies) {
        b2Transform transform = b2Body_GetTransform(rb->bodyId);
        float radiusM = std::max(rb->width, rb->height) * P2M * 0.707f + 2.0f; // Padding
        
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
                // FIX FLOATING: If sand is actively moving or was brushed, wake the physics body!
                if (c && (!c->isSleeping || c->visualDirty)) {
                    needsWake = true;
                }
            }
        }

        if (needsWake && !b2Body_IsAwake(rb->bodyId)) {
            b2Body_SetAwake(rb->bodyId, true);
        }
    }
    
    // 2. Cleanup Box2D terrain for chunks that no bodies are touching anymore
    for (auto it = chunkBodies.begin(); it != chunkBodies.end(); ) {
        if (overlappingChunks.find(it->first) == overlappingChunks.end()) {
            b2DestroyBody(it->second);
            it = chunkBodies.erase(it);
        } else {
            ++it;
        }
    }

    // 3. Rebuild Box2D terrain for overlapping chunks 
    for (auto coord : overlappingChunks) {
        Chunk* c = world.getChunk(coord.x << 6, coord.y << 6);
        bool hasTerrain = chunkBodies.count(coord) > 0;
        // Only rebuild if it doesn't exist, OR if sand is actively modifying it
        if (!hasTerrain || (c && (!c->isSleeping || c->visualDirty))) {
            rebuildChunkTerrain(coord, c);
        }
    }
    
    b2World_Step(worldId, dt, 4);
}

void RigidBodySystem::rasterizeToWorld(ParticleWorld& world) {
    for (auto& rb : bodies) {
        rb->drawnPixels.clear();
        
        b2Transform transform = b2Body_GetTransform(rb->bodyId);
        float radiusP = std::max(rb->width, rb->height) * 0.7071f + 1.0f; 
        
        // Find bounding box in world space
        int minWx = static_cast<int>(std::floor(transform.p.x * M2P - radiusP));
        int maxWx = static_cast<int>(std::ceil(transform.p.x * M2P + radiusP));
        int minWy = static_cast<int>(std::floor(transform.p.y * M2P - radiusP));
        int maxWy = static_cast<int>(std::ceil(transform.p.y * M2P + radiusP));

        float cs = transform.q.c;
        float sn = transform.q.s;

        // FIX HOLES: Inverse Rasterization. Look at every world pixel and find its local pixel!
        for (int wy = minWy; wy <= maxWy; ++wy) {
            for (int wx = minWx; wx <= maxWx; ++wx) {
                float dx = wx / M2P - transform.p.x;
                float dy = wy / M2P - transform.p.y;
                
                // Inverse rotate
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
                            if (c->base[idx].compMask == 0) {
                                p.base.flags.isRigidBodyPart = true;
                                world.add<BaseComponent>(wx, wy, p.base);
                                world.add<DurabilityComponent>(wx, wy, p.dur);
                                world.add<ThermalComponent>(wx, wy, p.therm);
                                world.setParticleColor(wx, wy, p.base.color);
                                world.wakeParticle(wx, wy);
                                
                                // Record successful draw for syncing later
                                rb->drawnPixels.push_back({wx, wy, localIdx});
                            }
                        }
                    }
                }
            }
        }
    }
}
void RigidBodySystem::syncFromWorld(ParticleWorld& world) {
    std::vector<std::unique_ptr<RigidBody>> newBodies;
    
    for (auto it = bodies.begin(); it != bodies.end(); ) {
        RigidBody* rb = it->get();
        bool needsRebuild = false;
        
        for (auto& dp : rb->drawnPixels) {
            LocalParticle& p = rb->particles[dp.localIdx];
            if (!p.active) continue; 
            
            Chunk* c = world.getChunk(dp.wx, dp.wy);
            if (c) {
                uint32_t idx = world.computeLocalIndex(dp.wx, dp.wy);
                if (c->base[idx].compMask == 0 || !c->base[idx].flags.isRigidBodyPart) {
                    p.active = false;
                    needsRebuild = true;
                } else {
                    p.base = c->base[idx];
                    if (c->base[idx].compMask & COMP_DURABILITY) p.dur = c->durability[idx];
                    if (c->base[idx].compMask & COMP_THERMAL) p.therm = c->thermal[idx];
                }
            } else {
                p.active = false;
                needsRebuild = true;
            }
        }
        
        if (needsRebuild) {
            std::vector<std::vector<LocalParticle>> islands = rb->findIslands();
            
            if (islands.empty()) {
                // FIX: Dump pixels to the trash bin instead of immediately wiping them!
                orphanedPixels.insert(orphanedPixels.end(), rb->drawnPixels.begin(), rb->drawnPixels.end());
                b2DestroyBody(rb->bodyId);
                it = bodies.erase(it);
                continue;
            } else if (islands.size() == 1) {
                // The object was damaged but remained in one piece
                rb->rebuildFixtures();
            } else {
                // The object was CUT IN HALF! Split it into independent bodies.
                
                // FIX: Dump original body pixels to the trash bin to be cleaned next frame!
                orphanedPixels.insert(orphanedPixels.end(), rb->drawnPixels.begin(), rb->drawnPixels.end());
                
                b2Transform xf = b2Body_GetTransform(rb->bodyId);
                b2Vec2 linVel = b2Body_GetLinearVelocity(rb->bodyId);
                float angVel = b2Body_GetAngularVelocity(rb->bodyId);
                
                for (const auto& island : islands) {
                    // 1. Find the bounds of this specific fragment
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
    
    // Append any newly spawned fragments
    for (auto& nb : newBodies) {
        bodies.push_back(std::move(nb));
    }
}
void RigidBodySystem::rebuildChunkTerrain(ChunkCoord coord, Chunk* chunk) {
    if (chunkBodies.count(coord)) {
        b2DestroyBody(chunkBodies[coord]);
        chunkBodies.erase(coord);
    }

    if (!chunk) return;

    b2BodyDef bdef = b2DefaultBodyDef();
    bdef.type = b2_staticBody;
    bdef.position = { (coord.x * CHUNK_SIZE) * P2M, (coord.y * CHUNK_SIZE) * P2M };
    b2BodyId bodyId = b2CreateBody(worldId, &bdef);

    bool hasFixtures = false;
    for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
        int startX = -1;
        for (int lx = 0; lx <= CHUNK_SIZE; ++lx) {
            bool solid = false;
            if (lx < CHUNK_SIZE) {
                uint32_t idx = (ly << 6) | lx;
                if (chunk->base[idx].compMask != 0 && !chunk->base[idx].flags.isRigidBodyPart) solid = true;
            }
            if (solid && startX == -1) {
                startX = lx;
            } else if (!solid && startX != -1) {
                float w = (lx - startX);
                float h = 1.0f;
                
                // Box2D uses Half-Width and Half-Height!
                float hx = (w / 2.0f) * P2M;
                float hy = (h / 2.0f) * P2M;
                
                float cx = startX + (w / 2.0f);
                float cy = ly + (h / 2.0f);
                
                b2Polygon box = b2MakeOffsetBox(hx, hy, {cx * P2M, cy * P2M}, b2MakeRot(0.0f));
                
                b2ShapeDef shapeDef = b2DefaultShapeDef();
                
                // FIX: Box2D 3.x API uses b2CreatePolygonShape
                b2CreatePolygonShape(bodyId, &shapeDef, &box); 
                
                hasFixtures = true;
                startX = -1;
            }
        }
    }
    
    if (hasFixtures) chunkBodies[coord] = bodyId;
    else b2DestroyBody(bodyId);
}