#include "RigidBody.hpp"
#include "Weapon.hpp"
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>

constexpr float PI = 3.14159265358979323846f;

RigidBody::RigidBody(b2WorldId worldId, int w, int h, const std::vector<LocalParticle>& parts, b2Vec2 pos, float angle, b2Vec2 linVel, float angVel, bool weapon, bool glued, int sX, int sY, sf::Vector2f customPivot, float angleOffset) {
    this->worldId = worldId;
    this->width = w;
    this->height = h;
    this->needsFixtureRebuild = false;
    this->isGlued = glued;
    this->startX = sX;
    this->startY = sY;
    this->isWeapon = weapon;
    this->isIndestructible = weapon;
    this->visualAngleOffset = angleOffset;
    
    if (customPivot.x == -1 && customPivot.y == -1) {
        pivot = {width / 2.0f - 0.5f, height / 2.0f - 0.5f};
    } else {
        pivot = customPivot;
    }
    
    particles.resize(w * h);
    for (auto& p : particles) p.active = false; 
    
    for (const auto& p : parts) {
        particles[p.localY * w + p.localX] = p;
    }

    if (isGlued) {
        bodyId = b2_nullBodyId;
    } else {
        b2BodyDef bdef = b2DefaultBodyDef();
        bdef.type = b2_dynamicBody;
        bdef.position = pos;
        
        bdef.rotation = b2MakeRot(angle);
        bdef.linearVelocity = linVel;
        bdef.angularVelocity = angVel;
        bdef.angularDamping = 2.0f;
        bdef.linearDamping = 0.2f;
        bdef.enableSleep = true;
        bdef.isBullet = true; // Essential for continuous collision detection (CCD)
        bodyId = b2CreateBody(worldId, &bdef);

        rebuildFixtures();
    }
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

RigidBody::RigidBody(b2WorldId worldId, const sf::Image& img, int startX, int startY, MaterialID mat, bool weapon, bool glued, sf::Vector2f customPivot, float angleOffset) {
    this->worldId = worldId;
    width = img.getSize().x;
    height = img.getSize().y;
    needsFixtureRebuild = false;
    
    isWeapon = weapon;
    isIndestructible = weapon;
    this->isGlued = glued;
    this->startX = startX;
    this->startY = startY;
    this->visualAngleOffset = angleOffset;

    if (customPivot.x == -1 && customPivot.y == -1) {
        pivot = {width / 2.0f - 0.5f, height / 2.0f - 0.5f};
    } else {
        pivot = customPivot;
    }

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

    if (isGlued) {
        bodyId = b2_nullBodyId;
    } else {
        b2BodyDef bdef = b2DefaultBodyDef();
        bdef.type = b2_dynamicBody;

        float centerX = static_cast<float>(startX) + pivot.x;
        float centerY = static_cast<float>(startY) + pivot.y;
        bdef.position = {centerX * P2M, centerY * P2M};

        bdef.angularDamping = 2.0f; 
        bdef.linearDamping = 0.2f;  
        bdef.enableSleep = true;
        bdef.isBullet = true; // Essential for Continuous Collision Detection (CCD)
        bodyId = b2CreateBody(worldId, &bdef);

        rebuildFixtures();
    }
}

void RigidBody::clearFromWorld(ParticleWorld& world) {
    if (isGlued) return; 

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

void RigidBody::renderPixelated(sf::RenderTarget& target, sf::Vector2f pos, float angleDeg, bool flipX, sf::Color overrideColor, bool applyVisualOffset) {
    sf::VertexArray va(sf::PrimitiveType::Triangles);

    float finalAngle = angleDeg;
    if (applyVisualOffset) {
        finalAngle += (flipX ? -visualAngleOffset : visualAngleOffset);
    }

    float rad = -finalAngle * PI / 180.0f;
    float cs  = std::cos(rad);
    float sn  = std::sin(rad);

    float radius = std::max(width, height) * 0.7071f + std::max(pivot.x, pivot.y) + 1.0f;
    int maxD = static_cast<int>(std::ceil(radius));

    for (int dy = -maxD; dy <= maxD; ++dy) {
        for (int dx = -maxD; dx <= maxD; ++dx) {
            float rx = dx * cs - dy * sn;
            float ry = dx * sn + dy * cs;

            if (flipX) rx = -rx;

            int lx = static_cast<int>(std::round(rx + pivot.x));
            int ly = static_cast<int>(std::round(ry + pivot.y));

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

void RigidBody::rebuildFixtures() {
    if (isGlued) return;

    int shapeCount = b2Body_GetShapeCount(bodyId);
    if (shapeCount > 0) {
        std::vector<b2ShapeId> shapes(shapeCount);
        b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
        for(int i = 0; i < shapeCount; ++i) {
            b2DestroyShape(shapes[i], true); 
        }
    }

    std::vector<bool> visited(width * height, false);
    
    // OPTIMIZED GREEDY RECTANGLE GENERATION
    // This creates perfect axis-aligned rectangles which Box2D processes extremely fast
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
                
                float hx = (w / 2.0f) * P2M;
                float hy = (h / 2.0f) * P2M;
                
                float cx = x + (w / 2.0f) - pivot.x;
                float cy = y + (h / 2.0f) - pivot.y;
                
                b2Polygon box = b2MakeOffsetBox(hx, hy, {cx * P2M, cy * P2M}, b2MakeRot(0.0f));
                
                // CRITICAL FIX: Radius 0.0f stops "bumpy seams" between rectangles.
                // It ensures flat walls act like perfect flat walls in the physics engine.
                box.radius = 0.0f; 
                
                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.density = 2.0f;     
                shapeDef.material.friction = 0.1f;    
                shapeDef.material.restitution = 0.0f; // Bouncy components easily glitch. Keep at 0.
                
                b2CreatePolygonShape(bodyId, &shapeDef, &box); 
            }
        }
    }
    needsFixtureRebuild = false;
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
            }
        }
    };

    for (const auto& pair : chunkBodies) drawBody(pair.second.bodyId, sf::Color::Cyan);
    for (const auto& rb : bodies) drawBody(rb->bodyId, sf::Color::Green);
}

void RigidBodySystem::addBody(std::unique_ptr<RigidBody> rb) {
    bodies.push_back(std::move(rb));
}

void RigidBodySystem::applyMeleeHit(sf::Vector2f pos, sf::Vector2f dir, float range, float force, bool shatter, ParticleWorld& world) {
    for (auto& rb : bodies) {
        if (rb->isEquipped || rb->isGlued || rb->isIndestructible) continue;
        
        b2Vec2 bp = b2Body_GetPosition(rb->bodyId);
        sf::Vector2f wp(bp.x * M2P, bp.y * M2P);
        float dist = std::hypot(wp.x - pos.x, wp.y - pos.y);
        
        if (dist <= range) {
            sf::Vector2f toBody = {wp.x - pos.x, wp.y - pos.y};
            float dot = (toBody.x * dir.x + toBody.y * dir.y) / dist;
            
            if (dot > 0.4f) { 
                b2Body_ApplyLinearImpulseToCenter(rb->bodyId, {dir.x * force, dir.y * force}, true);
            }
        }
    }

    if (shatter) {
        for (float d = 0; d < range; d += 2.0f) {
            world.eraseCircle(pos.x + dir.x * d, pos.y + dir.y * d, 4.0f);
        }
    }
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

void RigidBodySystem::addRigidBodyFromSprite(const sf::Image& img, int x, int y, MaterialID mat, bool glue, ParticleWorld& world) {
    bool touchesTerrain = false;
    if (glue) {
        for (unsigned int ly = 0; ly < img.getSize().y; ++ly) {
            for (unsigned int lx = 0; lx < img.getSize().x; ++lx) {
                if (img.getPixel({lx, ly}).a > 0) {
                    int wx = x + lx;
                    int wy = y + ly;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (std::abs(dx) + std::abs(dy) == 1) {
                                BaseComponent* b = world.get<BaseComponent>(wx + dx, wy + dy);
                                if (b && !b->flags.isRigidBodyPart) {
                                    Particle* logic = MaterialRegistry[static_cast<int>(b->id)];
                                    if (logic && logic->getGroup() == MaterialGroup::ImmovableSolid) {
                                        touchesTerrain = true;
                                        break;
                                    }
                                }
                            }
                        }
                        if (touchesTerrain) break;
                    }
                }
                if (touchesTerrain) break;
            }
        }
    }

    auto rb = std::make_unique<RigidBody>(worldId, img, x, y, mat, false, touchesTerrain);
    
    if (touchesTerrain) {
        for (int ly = 0; ly < rb->height; ++ly) {
            for (int lx = 0; lx < rb->width; ++lx) {
                int localIdx = ly * rb->width + lx;
                LocalParticle& p = rb->particles[localIdx];
                if (p.active) {
                    int wx = x + lx;
                    int wy = y + ly;
                    
                    p.base.flags.isRigidBodyPart = false; 
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
    
    bodies.push_back(std::move(rb));
}

void RigidBodySystem::addWeapon(const sf::Image& img, int x, int y, const std::string& name) {
    bodies.push_back(std::make_unique<Weapon>(worldId, img, x, y, name));
}

RigidBody* RigidBodySystem::getNearestWeapon(sf::Vector2f pos, float radius) {
    RigidBody* nearest = nullptr;
    float minDist = radius;

    for (auto& rb : bodies) {
        if (!rb->isWeapon || rb->isEquipped || rb->isGlued) continue;
        
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
        if (!rb->isWeapon || rb->isEquipped || rb->isGlued) continue;
        
        b2Vec2 bp = b2Body_GetPosition(rb->bodyId);
        sf::Vector2f wp(bp.x * M2P, bp.y * M2P);
        
        if (std::hypot(wp.x - playerPos.x, wp.y - playerPos.y) < 40.0f) {
            b2Transform xf = b2Body_GetTransform(rb->bodyId);
            float ang = std::atan2(xf.q.s, xf.q.c) * 180.f / PI;
            
            rb->renderPixelated(target, wp + sf::Vector2f( 1,  0), ang, false, sf::Color::Red, false);
            rb->renderPixelated(target, wp + sf::Vector2f(-1,  0), ang, false, sf::Color::Red, false);
            rb->renderPixelated(target, wp + sf::Vector2f( 0,  1), ang, false, sf::Color::Red, false);
            rb->renderPixelated(target, wp + sf::Vector2f( 0, -1), ang, false, sf::Color::Red, false);
        }
    }
}

void RigidBodySystem::renderGluedOutlines(sf::RenderTarget& target, ParticleWorld& world) const {
    sf::VertexArray va(sf::PrimitiveType::Lines);
    
    for (auto& rb : bodies) {
        if (!rb->isGlued) continue;

        int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
        for (const auto& dp : rb->drawnPixels) {
            if (!rb->particles[dp.localIdx].active) continue;
            
            bool isConnection = false;
            for (int d = 0; d < 4; ++d) {
                int nx = dp.wx + dirs[d][0];
                int ny = dp.wy + dirs[d][1];
                
                bool inBody = false;
                int nlx = nx - rb->startX;
                int nly = ny - rb->startY;
                if (nlx >= 0 && nlx < rb->width && nly >= 0 && nly < rb->height) {
                    if (rb->particles[nly * rb->width + nlx].active) inBody = true;
                }
                
                if (!inBody) {
                    BaseComponent* b = world.get<BaseComponent>(nx, ny);
                    if (b && !b->flags.isRigidBodyPart) {
                        Particle* logic = MaterialRegistry[static_cast<int>(b->id)];
                        if (logic && logic->getGroup() == MaterialGroup::ImmovableSolid) {
                            isConnection = true;
                            break;
                        }
                    }
                }
            }
            
            if (isConnection) {
                sf::Color col = sf::Color::Cyan;
                sf::Vector2f tl(dp.wx, dp.wy);
                sf::Vector2f tr(dp.wx + 1.0f, dp.wy);
                sf::Vector2f br(dp.wx + 1.0f, dp.wy + 1.0f);
                sf::Vector2f bl(dp.wx, dp.wy + 1.0f);

                va.append(sf::Vertex{tl, col}); va.append(sf::Vertex{tr, col});
                va.append(sf::Vertex{tr, col}); va.append(sf::Vertex{br, col});
                va.append(sf::Vertex{br, col}); va.append(sf::Vertex{bl, col});
                va.append(sf::Vertex{bl, col}); va.append(sf::Vertex{tl, col});
            }
        }
    }
    
    if (va.getVertexCount() > 0) target.draw(va);
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
    for (auto& rb : bodies) {
        rb->update(dt, world);
    }
    
    for (auto it = bodies.begin(); it != bodies.end(); ) {
        if ((*it)->isDestroyed) {
            if (b2Body_IsValid((*it)->bodyId)) {
                b2DestroyBody((*it)->bodyId);
            }
            it = bodies.erase(it);
        } else {
            ++it;
        }
    }

    std::unordered_set<ChunkCoord, ChunkCoordHash> overlappingChunks;
    
    for (auto& rb : bodies) {
        if (rb->isEquipped || rb->isGlued) continue;

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
    
    // CRITICAL FIX: Fixed Timestep Accumulator
    // This stops objects from "tunneling" or warping into each other at high speeds
    static float physicsAccumulator = 0.0f;
    
    // Cap DT to prevent the "spiral of death" if the game window is dragged/frozen
    if (dt > 0.1f) dt = 0.1f; 
    
    physicsAccumulator += dt;
    
    // 120Hz Simulation Step. High precision for tiny pixel-perfect rigid bodies
    const float FIXED_STEP = 1.0f / 120.0f; 
    
    while (physicsAccumulator >= FIXED_STEP) {
        // 4 Substeps per 1/120th sec ensures hyper-stable physics
        b2World_Step(worldId, FIXED_STEP, 4); 
        physicsAccumulator -= FIXED_STEP;
    }
}

void RigidBodySystem::rasterizeToWorld(ParticleWorld& world) {
    for (auto& rb : bodies) {
        if (rb->isEquipped || rb->isGlued) continue;

        rb->drawnPixels.clear();
        
        b2Transform transform = b2Body_GetTransform(rb->bodyId);
        
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
                
                int lx = static_cast<int>(std::round(rx * M2P + rb->pivot.x));
                int ly = static_cast<int>(std::round(ry * M2P + rb->pivot.y));
                
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
        
        float totalPixels = static_cast<float>(rb->width * rb->height);
        float waterRatio = (totalPixels > 0) ? std::min(static_cast<float>(waterOverlap) / totalPixels, 1.0f) : 0.0f;
        float sandRatio = (totalPixels > 0) ? std::min(static_cast<float>(sandOverlap) / totalPixels, 1.0f) : 0.0f;
        
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
            continue;
        }

        bool needsRebuild = false;
        
        for (auto& dp : rb->drawnPixels) {
            LocalParticle& p = rb->particles[dp.localIdx];
            if (!p.active) continue; 
            
            Chunk* c = world.getChunk(dp.wx, dp.wy);
            if (c) {
                uint32_t idx = world.computeLocalIndex(dp.wx, dp.wy);
                if (c->base[idx].compMask == 0 || c->base[idx].flags.isRigidBodyPart == rb->isGlued) {
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

        if (rb->isGlued && !needsRebuild) {
            bool stillAttached = false;
            int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
            for (auto& dp : rb->drawnPixels) {
                if (!rb->particles[dp.localIdx].active) continue;
                for (int d = 0; d < 4; ++d) {
                    int nx = dp.wx + dirs[d][0];
                    int ny = dp.wy + dirs[d][1];
                    
                    bool inBody = false;
                    int nlx = nx - rb->startX;
                    int nly = ny - rb->startY;
                    if (nlx >= 0 && nlx < rb->width && nly >= 0 && nly < rb->height) {
                        if (rb->particles[nly * rb->width + nlx].active) inBody = true;
                    }
                    
                    if (!inBody) {
                        BaseComponent* b = world.get<BaseComponent>(nx, ny);
                        if (b && !b->flags.isRigidBodyPart) {
                            Particle* logic = MaterialRegistry[static_cast<int>(b->id)];
                            if (logic && logic->getGroup() == MaterialGroup::ImmovableSolid) {
                                stillAttached = true;
                                break;
                            }
                        }
                    }
                }
                if (stillAttached) break;
            }
            if (!stillAttached) needsRebuild = true;
        }
        
        if (needsRebuild) {
            std::vector<std::vector<LocalParticle>> islands = rb->findIslands();
            
            if (islands.empty()) {
                if (!rb->isGlued) orphanedPixels.insert(orphanedPixels.end(), rb->drawnPixels.begin(), rb->drawnPixels.end());
                if (b2Body_IsValid(rb->bodyId)) b2DestroyBody(rb->bodyId);
                it = bodies.erase(it);
                continue;
            } else if (islands.size() == 1 && !rb->isGlued) {
                rb->rebuildFixtures();
            } else {
                if (!rb->isGlued) orphanedPixels.insert(orphanedPixels.end(), rb->drawnPixels.begin(), rb->drawnPixels.end());
                
                for (const auto& island : islands) {
                    bool islandGlued = false;
                    if (rb->isGlued) {
                        int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
                        for (const auto& p : island) {
                            int wx = rb->startX + p.localX;
                            int wy = rb->startY + p.localY;
                            for (int d = 0; d < 4; ++d) {
                                int nx = wx + dirs[d][0];
                                int ny = wy + dirs[d][1];
                                
                                bool inBody = false;
                                int nlx = nx - rb->startX;
                                int nly = ny - rb->startY;
                                if (nlx >= 0 && nlx < rb->width && nly >= 0 && nly < rb->height) {
                                    if (rb->particles[nly * rb->width + nlx].active) inBody = true;
                                }
                                
                                if (!inBody) {
                                    BaseComponent* b = world.get<BaseComponent>(nx, ny);
                                    if (b && !b->flags.isRigidBodyPart) {
                                        Particle* logic = MaterialRegistry[static_cast<int>(b->id)];
                                        if (logic && logic->getGroup() == MaterialGroup::ImmovableSolid) {
                                            islandGlued = true;
                                            break;
                                        }
                                    }
                                }
                            }
                            if (islandGlued) break;
                        }
                    }
                    
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
                    
                    if (islandGlued) {
                        auto newBody = std::make_unique<RigidBody>(worldId, newW, newH, newParts, b2Vec2({0,0}), 0.0f, b2Vec2({0,0}), 0.0f, false, true, rb->startX + minX, rb->startY + minY);
                        for (auto& p : newBody->particles) {
                            if (p.active) {
                                newBody->drawnPixels.push_back({newBody->startX + p.localX, newBody->startY + p.localY, p.localY * newW + p.localX});
                            }
                        }
                        newBodies.push_back(std::move(newBody));
                    } else {
                        if (rb->isGlued) {
                            for (const auto& p : island) {
                                int wx = rb->startX + p.localX;
                                int wy = rb->startY + p.localY;
                                Chunk* c = world.getChunk(wx, wy);
                                if (c) {
                                    uint32_t idx = world.computeLocalIndex(wx, wy);
                                    if (c->base[idx].compMask != 0 && c->base[idx].flags.isRigidBodyPart == false) {
                                        c->base[idx].compMask = 0;
                                        world.updateChunkPixel(c, idx, sf::Color::Transparent);
                                        world.wakeParticle(wx, wy);
                                    }
                                }
                            }
                        }
                        
                        b2Vec2 newPos, newLinVel;
                        float currentAngle, angVel;
                        
                        if (rb->isGlued) {
                            float cx = rb->startX + minX + newW / 2.0f;
                            float cy = rb->startY + minY + newH / 2.0f;
                            newPos = { cx * P2M, cy * P2M };
                            newLinVel = {0, 0};
                            currentAngle = 0;
                            angVel = 0;
                        } else {
                            b2Transform xf = b2Body_GetTransform(rb->bodyId);
                            b2Vec2 linVel = b2Body_GetLinearVelocity(rb->bodyId);
                            angVel = b2Body_GetAngularVelocity(rb->bodyId);
                            currentAngle = std::atan2(xf.q.s, xf.q.c);
                            
                            float dx_m = ((float)minX + newW / 2.0f - rb->width / 2.0f) * P2M;
                            float dy_m = ((float)minY + newH / 2.0f - rb->height / 2.0f) * P2M;
                            
                            float cs = xf.q.c;
                            float sn = xf.q.s;
                            float ox_world = dx_m * cs - dy_m * sn;
                            float oy_world = dx_m * sn + dy_m * cs;
                            
                            newPos = { xf.p.x + ox_world, xf.p.y + oy_world };
                            newLinVel = {
                                linVel.x - angVel * oy_world,
                                linVel.y + angVel * ox_world
                            };
                        }
                        
                        auto newBody = std::make_unique<RigidBody>(worldId, newW, newH, newParts, newPos, currentAngle, newLinVel, angVel, false, false, 0, 0);
                        newBodies.push_back(std::move(newBody));
                    }
                }
                
                if (b2Body_IsValid(rb->bodyId)) b2DestroyBody(rb->bodyId);
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

        // CORRECTED: Create segments for the lines and circles for the vertices
        for (size_t i = 0; i < simplified.size() - 1; ++i) {
            b2Segment segment = { 
                {simplified[i].x * P2M, simplified[i].y * P2M}, 
                {simplified[i+1].x * P2M, simplified[i+1].y * P2M} 
            };
            b2CreateSegmentShape(bodyId, &shapeDef, &segment);
            hasFixtures = true;
        }

        // Add frictionless circle "caps" to each vertex to prevent snagging
        shapeDef.material.friction = 0.0f; // Caps must be frictionless
        for (size_t i = 0; i < simplified.size(); ++i) {
            // No caps on the first and last vertex of an open chain
            if (simplified.front() == simplified.back() || (i > 0 && i < simplified.size() - 1)) {
                 b2Circle circle;
                 circle.center = {simplified[i].x * P2M, simplified[i].y * P2M};
                 circle.radius = 0.01f; // A tiny radius to smooth the corner
                 b2CreateCircleShape(bodyId, &shapeDef, &circle);
            }
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
        b2Vec2 p = {0,0};
        b2Vec2 linVel = {0,0};
        float angVel = 0;
        float angle = 0;
        
        if (!rb->isGlued) {
            b2Transform xf = b2Body_GetTransform(rb->bodyId);
            linVel = b2Body_GetLinearVelocity(rb->bodyId);
            angVel = b2Body_GetAngularVelocity(rb->bodyId);
            angle = std::atan2(xf.q.s, xf.q.c);
            p = xf.p;
        }

        out.write(reinterpret_cast<const char*>(&p), sizeof(b2Vec2));
        out.write(reinterpret_cast<const char*>(&angle), sizeof(float));
        out.write(reinterpret_cast<const char*>(&linVel), sizeof(b2Vec2));
        out.write(reinterpret_cast<const char*>(&angVel), sizeof(float));
        
        out.write(reinterpret_cast<const char*>(&rb->width), sizeof(int));
        out.write(reinterpret_cast<const char*>(&rb->height), sizeof(int));
        
        out.write(reinterpret_cast<const char*>(&rb->isWeapon), sizeof(bool));
        if (rb->isWeapon) {
            Weapon* w = static_cast<Weapon*>(rb.get());
            size_t len = w->weaponName.size();
            out.write(reinterpret_cast<const char*>(&len), sizeof(size_t));
            out.write(w->weaponName.c_str(), len);
        }
        
        out.write(reinterpret_cast<const char*>(&rb->isGlued), sizeof(bool));
        out.write(reinterpret_cast<const char*>(&rb->startX), sizeof(int));
        out.write(reinterpret_cast<const char*>(&rb->startY), sizeof(int));
        
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
        bool isWeapon = false;
        
        in.read(reinterpret_cast<char*>(&pos), sizeof(b2Vec2));
        in.read(reinterpret_cast<char*>(&angle), sizeof(float));
        in.read(reinterpret_cast<char*>(&linVel), sizeof(b2Vec2));
        in.read(reinterpret_cast<char*>(&angVel), sizeof(float));
        in.read(reinterpret_cast<char*>(&w), sizeof(int));
        in.read(reinterpret_cast<char*>(&h), sizeof(int));
        
        if (!in.read(reinterpret_cast<char*>(&isWeapon), sizeof(bool))) {
            isWeapon = false; 
        }
        
        std::string wName = "weapon";
        if (isWeapon) {
            size_t len = 0;
            in.read(reinterpret_cast<char*>(&len), sizeof(size_t));
            wName.resize(len);
            in.read(&wName[0], len);
        }

        bool isGlued = false;
        int sX = 0, sY = 0;
        if (in.read(reinterpret_cast<char*>(&isGlued), sizeof(bool))) {
            in.read(reinterpret_cast<char*>(&sX), sizeof(int));
            in.read(reinterpret_cast<char*>(&sY), sizeof(int));
        }
        
        size_t pCount = 0;
        in.read(reinterpret_cast<char*>(&pCount), sizeof(pCount));
        
        std::vector<LocalParticle> parts(pCount);
        if (pCount > 0) {
            in.read(reinterpret_cast<char*>(parts.data()), pCount * sizeof(LocalParticle));
        }
        
        if (isWeapon) {
            auto newBody = std::make_unique<Weapon>(worldId, w, h, parts, pos, angle, linVel, angVel, wName, isGlued, sX, sY);
            if (isGlued) {
                for (auto& p : newBody->particles) {
                    if (p.active) newBody->drawnPixels.push_back({sX + p.localX, sY + p.localY, p.localY * w + p.localX});
                }
            }
            bodies.push_back(std::move(newBody));
        } else {
            auto newBody = std::make_unique<RigidBody>(worldId, w, h, parts, pos, angle, linVel, angVel, false, isGlued, sX, sY);
            if (isGlued) {
                for (auto& p : newBody->particles) {
                    if (p.active) newBody->drawnPixels.push_back({sX + p.localX, sY + p.localY, p.localY * w + p.localX});
                }
            }
            bodies.push_back(std::move(newBody));
        }
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