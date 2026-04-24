#include "ParticleWorld.hpp"
#include "Particles/Particle.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <iostream>
#include "RigidBody.hpp"
#include "Particles/Explosion.hpp"

const char MAGIC_HEADER_V2[4] = {'S', 'N', 'D', '2'};

namespace {
    inline void moveSameChunkFast(Chunk* c, uint32_t oldIdx, uint32_t newIdx, uint8_t mask) {
        if (mask & COMP_KINEMATICS) c->kinematics[newIdx] = c->kinematics[oldIdx];
        if (mask & COMP_DURABILITY) c->durability[newIdx] = c->durability[oldIdx];
        if (mask & COMP_THERMAL)    c->thermal[newIdx]    = c->thermal[oldIdx];
        if (mask & COMP_FLUID)      c->fluid[newIdx]      = c->fluid[oldIdx];
    }

    inline void swapSameChunkFast(Chunk* c, uint32_t i1, uint32_t i2, uint8_t combinedMask) {
        if (combinedMask & COMP_KINEMATICS) std::swap(c->kinematics[i1], c->kinematics[i2]);
        if (combinedMask & COMP_DURABILITY) std::swap(c->durability[i1], c->durability[i2]);
        if (combinedMask & COMP_THERMAL)    std::swap(c->thermal[i1],    c->thermal[i2]);
        if (combinedMask & COMP_FLUID)      std::swap(c->fluid[i1],      c->fluid[i2]);
    }

    inline void moveOptionalCrossChunk(Chunk* oldC, uint32_t oldIdx, Chunk* newC, uint32_t newIdx, uint32_t mask) {
        if (mask & COMP_KINEMATICS) {
            if (!newC->kinematics) newC->kinematics = std::make_unique<KinematicsComponent[]>(CHUNK_AREA);
            newC->kinematics[newIdx] = oldC->kinematics[oldIdx];
        }
        if (mask & COMP_DURABILITY) {
            if (!newC->durability) newC->durability = std::make_unique<DurabilityComponent[]>(CHUNK_AREA);
            newC->durability[newIdx] = oldC->durability[oldIdx];
        }
        if (mask & COMP_THERMAL) {
            if (!newC->thermal) newC->thermal = std::make_unique<ThermalComponent[]>(CHUNK_AREA);
            newC->thermal[newIdx] = oldC->thermal[oldIdx];
        }
        if (mask & COMP_FLUID) {
            if (!newC->fluid) newC->fluid = std::make_unique<FluidComponent[]>(CHUNK_AREA);
            newC->fluid[newIdx] = oldC->fluid[oldIdx];
        }
    }

    inline void swapOptionalCrossChunk(Chunk* c1, uint32_t i1, Chunk* c2, uint32_t i2, uint8_t m1, uint8_t m2) {
        uint8_t combinedMask = m1 | m2;
        if (combinedMask & COMP_KINEMATICS) {
            if (!c1->kinematics) c1->kinematics = std::make_unique<KinematicsComponent[]>(CHUNK_AREA);
            if (!c2->kinematics) c2->kinematics = std::make_unique<KinematicsComponent[]>(CHUNK_AREA);
            std::swap(c1->kinematics[i1], c2->kinematics[i2]);
        }
        if (combinedMask & COMP_DURABILITY) {
            if (!c1->durability) c1->durability = std::make_unique<DurabilityComponent[]>(CHUNK_AREA);
            if (!c2->durability) c2->durability = std::make_unique<DurabilityComponent[]>(CHUNK_AREA);
            std::swap(c1->durability[i1], c2->durability[i2]);
        }
        if (combinedMask & COMP_THERMAL) {
            if (!c1->thermal) c1->thermal = std::make_unique<ThermalComponent[]>(CHUNK_AREA);
            if (!c2->thermal) c2->thermal = std::make_unique<ThermalComponent[]>(CHUNK_AREA);
            std::swap(c1->thermal[i1], c2->thermal[i2]);
        }
        if (combinedMask & COMP_FLUID) {
            if (!c1->fluid) c1->fluid = std::make_unique<FluidComponent[]>(CHUNK_AREA);
            if (!c2->fluid) c2->fluid = std::make_unique<FluidComponent[]>(CHUNK_AREA);
            std::swap(c1->fluid[i1], c2->fluid[i2]);
        }
    }
}

ParticleWorld::~ParticleWorld() = default;

ParticleWorld::ParticleWorld(unsigned int w, unsigned int h, const std::string &worldFile)
    : viewWidth(w), viewHeight(h), frameCounter(0), cameraPos({0, 0})
{
    std::fill(std::begin(cacheCx), std::end(cacheCx), -999999);
    std::fill(std::begin(cacheCy), std::end(cacheCy), -999999);

    rigidBodySystem = std::make_unique<RigidBodySystem>();
    pixelBuffer.resize(viewWidth * viewHeight * 4);
    if (!worldFile.empty() && std::filesystem::exists(worldFile)) {
        loadWorld(worldFile);
    }
}

void ParticleWorld::clear() { 
    chunks.clear(); 
    std::fill(std::begin(cacheChunk), std::end(cacheChunk), nullptr);
    std::fill(std::begin(cacheCx), std::end(cacheCx), -999999);
    std::fill(std::begin(cacheCy), std::end(cacheCy), -999999);
}

Chunk* ParticleWorld::getChunk(int x, int y) const {
    int cx = x >> 6;
    int cy = y >> 6;

    int cacheIdx = (cx ^ (cy * 73856093)) & 63; 
    if (cacheCx[cacheIdx] == cx && cacheCy[cacheIdx] == cy) {
        return cacheChunk[cacheIdx];
    }

    auto it = chunks.find({cx, cy});
    if (it != chunks.end()) {
        cacheCx[cacheIdx] = cx;
        cacheCy[cacheIdx] = cy;
        cacheChunk[cacheIdx] = it->second.get();
        return it->second.get();
    }
    return nullptr;
}

Chunk* ParticleWorld::getOrCreateChunk(int x, int y) {
    int cx = x >> 6;
    int cy = y >> 6;

    int cacheIdx = (cx ^ (cy * 73856093)) & 63; 
    if (cacheCx[cacheIdx] == cx && cacheCy[cacheIdx] == cy && cacheChunk[cacheIdx]) {
        return cacheChunk[cacheIdx];
    }

    ChunkCoord coord{cx, cy};
    auto it = chunks.find(coord);
    if (it == chunks.end()) {
        chunks[coord] = std::make_unique<Chunk>();
        
        cacheCx[cacheIdx] = cx;
        cacheCy[cacheIdx] = cy;
        cacheChunk[cacheIdx] = chunks[coord].get();
        return chunks[coord].get();
    }
    
    cacheCx[cacheIdx] = cx;
    cacheCy[cacheIdx] = cy;
    cacheChunk[cacheIdx] = it->second.get();
    return it->second.get();
}

void ParticleWorld::updateChunkPixel(Chunk *c, uint32_t localIdx, sf::Color color) {
    if (!c) return;
    std::memcpy(&c->pixelData[localIdx * 4], &color, sizeof(sf::Color));
    c->visualDirty = true;
}

bool ParticleWorld::inBounds(int x, int y) {
    return true; 
}

void ParticleWorld::spawnParticle(MaterialID id, int x, int y) {
    Chunk *c = getOrCreateChunk(x, y);
    uint32_t idx = computeLocalIndex(x, y);

    removeParticleInternal(c, idx); 
    
    Particle *pLogic = MaterialRegistry[static_cast<int>(id)];
    if (pLogic) {
        BaseComponent base(id, sf::Color::Transparent, ParticleFlags());
        add<BaseComponent>(x, y, base); 

        pLogic->onSpawn(idx, x, y, *this);
        
        BaseComponent* finalBase = &c->base[idx];
        updateChunkPixel(c, idx, finalBase->color);
    }
    wakeParticle(x, y);
}

void ParticleWorld::removeParticle(int x, int y) {
    Chunk *c = getChunk(x, y);
    if (c) {
        removeParticleInternal(c, computeLocalIndex(x, y));
        wakeParticle(x, y);
    }
}

void ParticleWorld::removeParticleInternal(Chunk *chunk, uint32_t localIndex) {
    if (!chunk || chunk->base[localIndex].compMask == 0) return;
    
    chunk->base[localIndex].compMask = 0; 
    chunk->base[localIndex].id = (MaterialID)0; 
    
    updateChunkPixel(chunk, localIndex, sf::Color::Transparent);
}

void ParticleWorld::moveParticle(int oldX, int oldY, int newX, int newY) {
    if (oldX == newX && oldY == newY) return;

    int cx1 = oldX >> 6;
    int cy1 = oldY >> 6;
    int cx2 = newX >> 6;
    int cy2 = newY >> 6;

    uint32_t oldIdx = computeLocalIndex(oldX, oldY);
    uint32_t newIdx = computeLocalIndex(newX, newY);

    Chunk* oldC = getChunk(oldX, oldY);
    if (!oldC) return;
    
    uint8_t mask = oldC->base[oldIdx].compMask;
    if (mask == 0) return;

    if (cx1 == cx2 && cy1 == cy2) {
        oldC->base[newIdx] = oldC->base[oldIdx];
        oldC->base[oldIdx].compMask = 0;

        if (mask > 1) moveSameChunkFast(oldC, oldIdx, newIdx, mask);
        
        updateChunkPixel(oldC, newIdx, oldC->base[newIdx].color);
        updateChunkPixel(oldC, oldIdx, sf::Color::Transparent);
        
        wakeParticle(newX, newY);
        wakeParticle(oldX, oldY - 1); 
        wakeParticle(oldX - 1, oldY); 
        wakeParticle(oldX + 1, oldY); 
        return;
    }

    Chunk* newC = getOrCreateChunk(newX, newY);
    newC->base[newIdx] = oldC->base[oldIdx];
    oldC->base[oldIdx].compMask = 0;

    if (mask > 1) moveOptionalCrossChunk(oldC, oldIdx, newC, newIdx, mask);

    updateChunkPixel(newC, newIdx, newC->base[newIdx].color);
    updateChunkPixel(oldC, oldIdx, sf::Color::Transparent);
    
    wakeParticle(newX, newY);
    wakeParticle(oldX, oldY - 1);
    wakeParticle(oldX - 1, oldY);
    wakeParticle(oldX + 1, oldY);
}

void ParticleWorld::swapParticles(int x1, int y1, int x2, int y2) {
    int cx1 = x1 >> 6;
    int cy1 = y1 >> 6;
    int cx2 = x2 >> 6;
    int cy2 = y2 >> 6;

    uint32_t i1 = computeLocalIndex(x1, y1);
    uint32_t i2 = computeLocalIndex(x2, y2);

    Chunk* c1 = getChunk(x1, y1);
    
    if (c1 && cx1 == cx2 && cy1 == cy2) {
        uint8_t m1 = c1->base[i1].compMask;
        uint8_t m2 = c1->base[i2].compMask;
        uint8_t combined = m1 | m2;
        if (combined == 0) return;

        std::swap(c1->base[i1], c1->base[i2]);
        if (combined > 1) swapSameChunkFast(c1, i1, i2, combined);

        updateChunkPixel(c1, i1, c1->base[i1].color);
        updateChunkPixel(c1, i2, c1->base[i2].color);
        
        wakeParticle(x1, y1); wakeParticle(x1, y1 - 1);
        wakeParticle(x2, y2); wakeParticle(x2, y2 - 1);
        return;
    }

    Chunk* c2 = getChunk(x2, y2);
    if (!c1 && !c2) return;

    BaseComponent* b1 = (c1 && c1->base[i1].compMask) ? &c1->base[i1] : nullptr;
    BaseComponent* b2 = (c2 && c2->base[i2].compMask) ? &c2->base[i2] : nullptr;

    if (!b1 && !b2) return;
    if (b1 && !b2) { moveParticle(x1, y1, x2, y2); return; }
    if (!b1 && b2) { moveParticle(x2, y2, x1, y1); return; }

    uint8_t m1 = b1->compMask;
    uint8_t m2 = b2->compMask;
    uint8_t combined = m1 | m2;

    std::swap(*b1, *b2);
    if (combined > 1) swapOptionalCrossChunk(c1, i1, c2, i2, m1, m2);

    updateChunkPixel(c1, i1, b1->color);
    updateChunkPixel(c2, i2, b2->color);
    
    wakeParticle(x1, y1); wakeParticle(x1, y1 - 1);
    wakeParticle(x2, y2); wakeParticle(x2, y2 - 1);
}

void ParticleWorld::update(float deltaTime)
{
    if (rigidBodySystem) {
        rigidBodySystem->syncFromWorld(*this);
        rigidBodySystem->clearFromWorld(*this);         
        rigidBodySystem->stepPhysics(deltaTime, *this); 
        rigidBodySystem->rasterizeToWorld(*this);       
    }
// Now that rigid bodies have been rasterized, explosions will blow holes in them!
    for (const auto& exp : pendingExplosions) {
        if (rigidBodySystem) {
            rigidBodySystem->applyBlastImpulse(static_cast<float>(exp.x), static_cast<float>(exp.y), static_cast<float>(exp.radius), static_cast<float>(exp.strength));
        }
        Explosion boom(*this, exp.x, exp.y, exp.radius, exp.strength);
        boom.enact();
    }
    pendingExplosions.clear();
    // ---------------------------------------------
    frameCounter++;
    bool dir = (frameCounter % 2) == 0;

    static std::vector<std::pair<ChunkCoord, Chunk *>> safeUpdateList;
    safeUpdateList.clear();
    safeUpdateList.reserve(chunks.size());

    for (auto &[coord, chunk] : chunks) {
        if (chunk->isActive) safeUpdateList.push_back({coord, chunk.get()});
    }

    for (auto &[coord, chunk] : safeUpdateList) {
        if (chunk->nextMinX > chunk->nextMaxX) {
            chunk->isSleeping = true;
            continue;
        }
        chunk->isSleeping = false;
        chunk->activeMinX = std::max(0, chunk->nextMinX - 1);
        chunk->activeMinY = std::max(0, chunk->nextMinY - 1);
        chunk->activeMaxX = std::min(CHUNK_SIZE - 1, chunk->nextMaxX + 1);
        chunk->activeMaxY = std::min(CHUNK_SIZE - 1, chunk->nextMaxY + 1);
        chunk->nextMinX = CHUNK_SIZE; chunk->nextMinY = CHUNK_SIZE;
        chunk->nextMaxX = -1; chunk->nextMaxY = -1;
    }

    for (auto &[coord, chunk] : safeUpdateList)
    {
        if (!chunk->isActive || chunk->isSleeping) continue;

        int chunkOriginX = coord.x * CHUNK_SIZE;
        int chunkOriginY = coord.y * CHUNK_SIZE;
        sf::FloatRect chunkRect(
            {static_cast<float>(chunkOriginX), static_cast<float>(chunkOriginY)},
            {static_cast<float>(CHUNK_SIZE), static_cast<float>(CHUNK_SIZE)}
        );

        if (!simulationBounds.findIntersection(chunkRect)) continue;
        
        BaseComponent* baseArr = chunk->base;

        auto processParticle = [&](int lx, int ly) {
            uint32_t i = (ly << 6) | lx;

            if (baseArr[i].compMask == 0 || baseArr[i].flags.hasBeenUpdatedThisFrame) return;

            baseArr[i].flags.hasBeenUpdatedThisFrame = true;

            ParticleContext ctx { 
                chunk, 
                i, 
                chunkOriginX + lx, 
                chunkOriginY + ly, 
                baseArr, 
                chunk->kinematics.get(), 
                chunk->fluid.get(), 
                chunk->thermal.get(), 
                chunk->durability.get() 
            };

            if (MaterialRegistry[static_cast<int>(baseArr[i].id)]) {
                MaterialRegistry[static_cast<int>(baseArr[i].id)]->update(ctx, deltaTime, *this);
            }
        };

        if (dir) {
            for (int ly = chunk->activeMaxY; ly >= chunk->activeMinY; --ly) {
                for (int lx = chunk->activeMinX; lx <= chunk->activeMaxX; ++lx) {
                    processParticle(lx, ly);
                }
            }
        } else {
            for (int ly = chunk->activeMaxY; ly >= chunk->activeMinY; --ly) {
                for (int lx = chunk->activeMaxX; lx >= chunk->activeMinX; --lx) {
                    processParticle(lx, ly);
                }
            }
        }
    }

    for (auto &[coord, chunk] : safeUpdateList) {
        if (chunk->isSleeping) continue; 
        for (int i=0; i<CHUNK_AREA; ++i) {
            if (chunk->base[i].compMask)
                chunk->base[i].flags.hasBeenUpdatedThisFrame = false;
        }
    }

    if (rigidBodySystem) {
        rigidBodySystem->syncFromWorld(*this);
    }
}
void ParticleWorld::wakeParticle(int x, int y) {
    Chunk* c = getChunk(x, y);
    if (!c) return;
    
    int lx = x & 63; 
    int ly = y & 63;

    if (lx < c->nextMinX) c->nextMinX = lx;
    if (lx > c->nextMaxX) c->nextMaxX = lx;
    if (ly < c->nextMinY) c->nextMinY = ly;
    if (ly > c->nextMaxY) c->nextMaxY = ly;

    auto wakeNeighbor = [&](int nx, int ny) {
        if (Chunk* nc = getChunk(nx, ny)) {
            int nlx = nx & 63;
            int nly = ny & 63;
            if (nlx < nc->nextMinX) nc->nextMinX = nlx;
            if (nlx > nc->nextMaxX) nc->nextMaxX = nlx;
            if (nly < nc->nextMinY) nc->nextMinY = nly;
            if (nly > nc->nextMaxY) nc->nextMaxY = nly;
        }
    };

    bool left = (lx == 0);
    bool right = (lx == 63);
    bool top = (ly == 0);
    bool bottom = (ly == 63);

    if (left) wakeNeighbor(x - 1, y);
    if (right) wakeNeighbor(x + 1, y);
    if (top) wakeNeighbor(x, y - 1);
    if (bottom) wakeNeighbor(x, y + 1);

    if (left && top) wakeNeighbor(x - 1, y - 1);
    if (right && top) wakeNeighbor(x + 1, y - 1);
    if (left && bottom) wakeNeighbor(x - 1, y + 1);
    if (right && bottom) wakeNeighbor(x + 1, y + 1);
}

void ParticleWorld::setParticleColor(int x, int y, const sf::Color& newColor) {
    Chunk* c = getChunk(x, y);
    if (!c) return;

    uint32_t localIdx = computeLocalIndex(x, y);
    if (c->base[localIdx].compMask == 0) return;
    
    c->base[localIdx].color = newColor;
    updateChunkPixel(c, localIdx, newColor);
    wakeParticle(x, y);
}

void ParticleWorld::updateParticleColor(uint32_t localIndex, int x, int y, Chunk* c) {
    if (!c) c = getChunk(x, y);
    if (!c) return;
    
    BaseComponent& base = c->base[localIndex];
    if (base.compMask == 0) return;

    if (base.flags.isIgnited) {
        if (Random::randInt(0, 100) < 20) {
            sf::Color newColor = base.color; 
            int roll = Random::randInt(0, 100);
            if (roll < 10) newColor = sf::Color(255, 255, 150);
            else if (roll < 60) newColor = sf::Color(255, Random::randInt(120, 180), 20);
            else newColor = sf::Color(Random::randInt(180, 220), 40, 10);
            
            setParticleColor(x, y, newColor);
        }
    }
}

void ParticleWorld::setCameraPos(int x, int y) {
    cameraPos = {x, y};
}

void ParticleWorld::renderToBuffer() {
    std::fill(pixelBuffer.begin(), pixelBuffer.end(), 0); 

    int startCX = cameraPos.x >> 6;
    int startCY = cameraPos.y >> 6;
    int endCX = startCX + (viewWidth / CHUNK_SIZE) + 1;
    int endCY = startCY + (viewHeight / CHUNK_SIZE) + 1;

    uint32_t* dest32 = reinterpret_cast<uint32_t*>(pixelBuffer.data());
    int viewW = viewWidth;
    int viewH = viewHeight;

    for (int cy = startCY; cy <= endCY; ++cy) {
        for (int cx = startCX; cx <= endCX; ++cx) {
            auto it = chunks.find({cx, cy});
            if (it == chunks.end()) continue;

            Chunk* chunk = it->second.get();
            int chunkWorldX = cx * CHUNK_SIZE;
            int chunkWorldY = cy * CHUNK_SIZE;
            const uint32_t* src32 = reinterpret_cast<const uint32_t*>(chunk->pixelData.data());

            for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                int screenY = chunkWorldY + ly - cameraPos.y;
                if (screenY < 0 || screenY >= viewH) continue;

                for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                    int screenX = chunkWorldX + lx - cameraPos.x;
                    if (screenX < 0 || screenX >= viewW) continue;

                    uint32_t pixel = src32[ly * CHUNK_SIZE + lx];
                    if (pixel != 0) { 
                        dest32[screenY * viewW + screenX] = pixel;
                    }
                }
            }
        }
    }
}

void ParticleWorld::updatePixelColor(int x, int y, const sf::Color &color) {}

void ParticleWorld::updateCameraBounds(float centerX, float centerY, float viewWidth, float viewHeight) {
    renderBounds = sf::FloatRect(
        {centerX - viewWidth / 2.0f, centerY - viewHeight / 2.0f},
        {viewWidth, viewHeight}
    );
    float margin = CHUNK_SIZE * 2.0f;
    simulationBounds = sf::FloatRect(
        {renderBounds.position.x - margin, renderBounds.position.y - margin}, 
        {renderBounds.size.x + (margin * 2.0f), renderBounds.size.y + (margin * 2.0f)} 
    );
}

void ParticleWorld::triggerExplosion(int x, int y, int radius, int strength) {
    pendingExplosions.push_back({x, y, radius, strength});
}
void ParticleWorld::addParticleCircle(int centerX, int centerY, float radius, MaterialID materialType) {
    int r = (int)std::ceil(radius);
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                spawnParticle(materialType, centerX + dx, centerY + dy);
            }
        }
    }
}

void ParticleWorld::addParticleSquare(int centerX, int centerY, float radius, MaterialID materialType) {
    int r = (int)std::ceil(radius);
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            spawnParticle(materialType, centerX + dx, centerY + dy);
        }
    }
}

void ParticleWorld::eraseCircle(int centerX, int centerY, float radius) {
    int r = (int)std::ceil(radius);
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                removeParticle(centerX + dx, centerY + dy);
            }
        }
    }
}

void ParticleWorld::eraseSquare(int centerX, int centerY, float radius) {
    int r = (int)std::ceil(radius);
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            removeParticle(centerX + dx, centerY + dy);
        }
    }
}

void ParticleWorld::renderDebugColliders(sf::RenderTarget& target) const {
    if (rigidBodySystem) {
        rigidBodySystem->renderDebug(target);
    }
}

bool ParticleWorld::saveWorld(const std::string &baseFilename) {
    std::string filename = getNextAvailableFilename("worlds/" + baseFilename);
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(MAGIC_HEADER_V2, 4);
    
    size_t chunkCount = chunks.size();
    file.write(reinterpret_cast<const char*>(&chunkCount), sizeof(chunkCount));

    for (const auto &[coord, chunk] : chunks) {
        file.write(reinterpret_cast<const char*>(&coord.x), sizeof(coord.x));
        file.write(reinterpret_cast<const char*>(&coord.y), sizeof(coord.y));

        for (int i = 0; i < CHUNK_AREA; ++i) {
            uint8_t mask = chunk->base[i].compMask;
            file.write(reinterpret_cast<const char*>(&mask), sizeof(mask));

            if (mask != 0) {
                if (mask & COMP_BASE)       file.write(reinterpret_cast<const char*>(&chunk->base[i]),       sizeof(BaseComponent));
                if (mask & COMP_KINEMATICS) file.write(reinterpret_cast<const char*>(&chunk->kinematics[i]), sizeof(KinematicsComponent));
                if (mask & COMP_DURABILITY) file.write(reinterpret_cast<const char*>(&chunk->durability[i]), sizeof(DurabilityComponent));
                if (mask & COMP_THERMAL)    file.write(reinterpret_cast<const char*>(&chunk->thermal[i]),    sizeof(ThermalComponent));
                if (mask & COMP_FLUID)      file.write(reinterpret_cast<const char*>(&chunk->fluid[i]),      sizeof(FluidComponent));
            }
        }
    }

    bool hasRBS = rigidBodySystem != nullptr;
    file.write(reinterpret_cast<const char*>(&hasRBS), sizeof(hasRBS));
    if (hasRBS) {
        rigidBodySystem->save(file);
    }

    return true;
}

bool ParticleWorld::loadWorld(const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    char h[4];
    file.read(h, 4);
    if (std::memcmp(h, MAGIC_HEADER_V2, 4) != 0) {
        std::cerr << "Cannot load world: Outdated or corrupted file format!\n";
        return false;
    }

    clear();
    if (rigidBodySystem) {
        rigidBodySystem->clearAll();
    }

    size_t chunkCount;
    file.read(reinterpret_cast<char*>(&chunkCount), sizeof(chunkCount));

    for (size_t i = 0; i < chunkCount; ++i) {
        int cx, cy;
        file.read(reinterpret_cast<char*>(&cx), sizeof(cx));
        file.read(reinterpret_cast<char*>(&cy), sizeof(cy));

        Chunk *c = getOrCreateChunk(cx * CHUNK_SIZE, cy * CHUNK_SIZE);

        for (int idx = 0; idx < CHUNK_AREA; ++idx) {
            uint8_t mask;
            file.read(reinterpret_cast<char*>(&mask), sizeof(mask));

            if (mask != 0) {
                if (mask & COMP_BASE) {
                    file.read(reinterpret_cast<char*>(&c->base[idx]), sizeof(BaseComponent));
                }
                if (mask & COMP_KINEMATICS) {
                    if (!c->kinematics) c->kinematics = std::make_unique<KinematicsComponent[]>(CHUNK_AREA);
                    file.read(reinterpret_cast<char*>(&c->kinematics[idx]), sizeof(KinematicsComponent));
                }
                if (mask & COMP_DURABILITY) {
                    if (!c->durability) c->durability = std::make_unique<DurabilityComponent[]>(CHUNK_AREA);
                    file.read(reinterpret_cast<char*>(&c->durability[idx]), sizeof(DurabilityComponent));
                }
                if (mask & COMP_THERMAL) {
                    if (!c->thermal) c->thermal = std::make_unique<ThermalComponent[]>(CHUNK_AREA);
                    file.read(reinterpret_cast<char*>(&c->thermal[idx]), sizeof(ThermalComponent));
                }
                if (mask & COMP_FLUID) {
                    if (!c->fluid) c->fluid = std::make_unique<FluidComponent[]>(CHUNK_AREA);
                    file.read(reinterpret_cast<char*>(&c->fluid[idx]), sizeof(FluidComponent));
                }

                if ((mask & COMP_BASE) && c->base[idx].flags.isRigidBodyPart) {
                    c->base[idx].compMask = 0; 
                } else if (mask & COMP_BASE) {
                    updateChunkPixel(c, idx, c->base[idx].color);
                }
            }
        }
        c->isSleeping = false;
        c->visualDirty = true;
    }

    bool hasRBS;
    if (file.read(reinterpret_cast<char*>(&hasRBS), sizeof(hasRBS))) {
        if (hasRBS && rigidBodySystem) {
            rigidBodySystem->load(file);
        }
    }

    return true;
}

std::string ParticleWorld::getNextAvailableFilename(const std::string &baseName) {
    std::filesystem::path path(baseName);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::string f;
    int c = 0;
    do {
        f = baseName + std::to_string(c++) + ".rrr";
    } while (std::filesystem::exists(f));
    return f;
}
void ParticleWorld::addRigidBodyFromSprite(const sf::Image& img, int startX, int startY, MaterialID mat, bool glue) {
    if (rigidBodySystem) {
        rigidBodySystem->addRigidBodyFromSprite(img, startX, startY, mat, glue, *this);
    }
}

void ParticleWorld::addStructureFromSprite(const sf::Image& img, int startX, int startY, MaterialID mat) {
    for (unsigned int y = 0; y < img.getSize().y; ++y) {
        for (unsigned int x = 0; x < img.getSize().x; ++x) {
            sf::Color col = img.getPixel(sf::Vector2u(x, y));
            if (col.a > 0) {
                int wx = startX + static_cast<int>(x);
                int wy = startY + static_cast<int>(y);
                
                spawnParticle(mat, wx, wy);
                setParticleColor(wx, wy, col); 
            }
        }
    }
}

void ParticleWorld::addRigidBody(int cx, int cy, float sz, RigidBodyShape sh, MaterialID mat, bool glue) {
    if (!rigidBodySystem) return;

    int s = static_cast<int>(sz);
    if (s <= 0) return;

    sf::Image img;
    img.resize(sf::Vector2u(static_cast<unsigned int>(s), static_cast<unsigned int>(s)), sf::Color::Transparent);
    
    sf::Color matColor = Particle::getRandomColor(mat); 

    if (sh == RigidBodyShape::Box) {
        for (int y = 0; y < s; ++y) {
            for (int x = 0; x < s; ++x) {
                img.setPixel(sf::Vector2u(static_cast<unsigned int>(x), static_cast<unsigned int>(y)), matColor);
            }
        }
    } else if (sh == RigidBodyShape::Circle) {
        int r = s / 2;
        for (int y = 0; y < s; ++y) {
            for (int x = 0; x < s; ++x) {
                int dx = x - r;
                int dy = y - r;
                if (dx * dx + dy * dy <= r * r) {
                    img.setPixel(sf::Vector2u(static_cast<unsigned int>(x), static_cast<unsigned int>(y)), matColor);
                }
            }
        }
    }
    
    addRigidBodyFromSprite(img, cx, cy, mat, glue);
}

void ParticleWorld::addWeapon(const sf::Image& img, int startX, int startY, const std::string& name) {
    if (rigidBodySystem) {
        rigidBodySystem->addWeapon(img, startX, startY, name);
    }
}

void ParticleWorld::renderWeaponsOutline(sf::RenderTarget& target, sf::Vector2f playerPos) const {
    if (rigidBodySystem) {
        rigidBodySystem->renderWeaponsOutline(target, playerPos);
    }
}