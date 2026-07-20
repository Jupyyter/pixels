#include "Particles/Particle.hpp"
#include "ParticleWorld.hpp"
#include "Random.hpp"
#include "Particles/ParticleDef.hpp" 
#include <algorithm>

Particle* MaterialRegistry[256] = { nullptr };

sf::Color Particle::getRandomColor(MaterialID id) {
    auto it = GlobalParticleDefs.find(id);
    if (it != GlobalParticleDefs.end()) {
        return it->second.getRandomColor();
    }
    return sf::Color::Magenta; 
}

void Particle::onSpawn(uint32_t index, int x, int y, ParticleWorld& world) {
    world.add<BaseComponent>(x, y, BaseComponent(
        this->id, 
        getRandomColor(this->id),
        ParticleFlags(false, false, false, false, false, false, false, false, false)
    ));
}

void Particle::die(int x, int y, ParticleWorld& world) {
    world.removeParticle(x, y);
}

void Particle::dieAndReplace(int x, int y, MaterialID newType, ParticleWorld& world) {
    world.removeParticle(x, y);
    world.spawnParticle(newType, x, y);
}

void Particle::checkIfDead(BaseComponent* base, DurabilityComponent* dur, int x, int y, ParticleWorld& world) {
    if (dur && base && dur->health <= 0 && !base->flags.isDead) {
        die(x, y, world);
    }
}

bool Particle::corrode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) {
    if (dur) {
        dur->health -= damage;
        checkIfDead(base, dur, x, y, world);
        return true;
    }
    return false;
}

void Particle::takeEffectsDamage(BaseComponent* base, DurabilityComponent* dur, ThermalComponent* therm, int x, int y, ParticleWorld& world) {
    if (base && base->flags.isIgnited && dur && therm) {
        dur->health -= therm->fireDamage;
    }
    checkIfDead(base, dur, x, y, world);
}

bool Particle::didNotMove(int currentX, int currentY, int originalX, int originalY) {
    return (currentX == originalX && currentY == originalY);
}

bool Particle::shouldApplyHeat(BaseComponent* base) {
    return base && (base->flags.isIgnited || base->flags.heated);
}

void Particle::checkLifeSpan(BaseComponent* base, DurabilityComponent* dur,int x, int y, ParticleWorld& world) {}

bool Particle::receiveHeat(BaseComponent* base, ThermalComponent* therm,  int x, int y,int heat, ParticleWorld& world) {
    if (!base || !therm || base->flags.isIgnited) return false;
    therm->flammabilityResistance -= Random::randInt(0, heat);
    if (therm->flammabilityResistance <= 0) {
        base->flags.isIgnited = true;
        base->flags.didColorChange = true;
    }
    return true;
}

bool Particle::receiveCooling(BaseComponent* base, ThermalComponent* therm, int x, int y, int cooling, ParticleWorld& world) {
    if (base && therm) {
        if (base->flags.isIgnited) {
            therm->flammabilityResistance += cooling;
            if (therm->flammabilityResistance > 0) {
                base->flags.isIgnited = false;
                base->flags.didColorChange = true;
            }
            return true;
        } else if (base->flags.heated) {
            therm->temperature -= cooling;
            return true;
        }
    }
    return false;
}

bool Particle::receiveCharge(BaseComponent* base, int x, int y, ParticleWorld& world) { return false; } // Handled by generics

bool Particle::magmatize(BaseComponent* base, DurabilityComponent* dur, int x, int y, int damage, ParticleWorld& world) {
    if (dur) {
        dur->health -= damage;
        checkIfDead(base, dur, x, y, world);
        return true;
    }
    return false;
}

bool Particle::explode(BaseComponent* base, DurabilityComponent* dur, int x, int y, int strength, ParticleWorld& world) {
    if (!dur) return false;
    if (dur->explosionResistance < strength) {
        die(x, y, world);
        return true;
    }
    return false;
}

bool Particle::infect(int x, int y, ParticleWorld& world) { return false; }

bool Particle::stain(BaseComponent* base, int x, int y, sf::Color newColor, ParticleWorld& world) {
    if (!base || base->flags.isIgnited) return false;
    if (Random::randFloat(0.0f, 1.0f) < 0.2f) {
        world.setParticleColor(x, y, newColor);
        return true;
    }
    return false;
}

bool Particle::cleanColor(BaseComponent* base, int x, int y, ParticleWorld& world) {
    if (!base || !base->flags.discolored) return false;
    if (Random::randFloat(0.0f, 1.0f) < 0.2f) {
        base->flags.didColorChange = true;
        base->flags.discolored = false;
        return true;
    }
    return false;
}

bool Particle::applyHeatToNeighborsIfIgnited(BaseComponent* base, ThermalComponent* therm, int x, int y, ParticleWorld& world) {
    if (!shouldApplyHeat(base)) return false;
    int heatAmt = therm ? therm->heatFactor : 10;
    for (int nx = x - 1; nx <= x + 1; nx++) {
        for (int ny = y - 1; ny <= y + 1; ny++) {
            if (nx == x && ny == y) continue;
            if (world.inBounds(nx, ny)) {
                if (auto* nBase = world.get<BaseComponent>(nx, ny)) {
                    Particle* neighborLogic = MaterialRegistry[static_cast<int>(nBase->id)];
                    if (neighborLogic) {
                        auto* nTherm = world.get<ThermalComponent>(nx, ny);
                        neighborLogic->receiveHeat(nBase, nTherm,   x,  y,heatAmt, world);
                    }
                }
            }
        }
    }
    return true;
}

void Particle::spawnSparkIfIgnited(BaseComponent* base, int x, int y, ParticleWorld& world) {
    if (!base || !base->flags.isIgnited) return;
    int upX = x;
    int upY = y - 1;
    if (world.inBounds(upX, upY) && world.isEmpty(upX, upY)) {
        MaterialID elementToSpawn = (Random::randFloat(0.0f, 1.0f) > 0.1f) ? GetMatID("Spark") : GetMatID("Smoke");
        world.spawnParticle(elementToSpawn, upX, upY);
    }
}

void Particle::processAdvancedOrganicAndElectricalTraits(const ParticleDef& def, const ParticleContext& ctx, ParticleWorld& world) {
    auto* base = world.getFast<BaseComponent>(ctx, ctx.x, ctx.y);
    if (!base) return;

    // Electricity Pass
    if (def.generates_charge) base->flags.isCharged = true;
    
    if (base->flags.isCharged) {
        if (!def.generates_charge && Random::randFloat(0,1) > 0.8f) {
            base->flags.isCharged = false;
        }
        for (int ox=-1; ox<=1; ++ox) {
            for (int oy=-1; oy<=1; ++oy) {
                if (ox==0 && oy==0) continue;
                auto* nb = world.getFast<BaseComponent>(ctx, ctx.x+ox, ctx.y+oy);
                if (nb && !nb->flags.isCharged) {
                    Particle* nl = MaterialRegistry[static_cast<int>(nb->id)];
                    if (nl) nl->receiveCharge(nb, ctx.x+ox, ctx.y+oy, world);
                }
            }
        }
    }

    // Organic Growth
    if (def.growth_rate > 0.0f && Random::randFloat(0,1) < def.growth_rate) {
        int dx = Random::randInt(-1, 1);
        int dy = Random::randInt(-1, 1);
        if (dx != 0 || dy != 0) {
            int nx = ctx.x + dx, ny = ctx.y + dy;
            if (world.inBounds(nx, ny) && world.isEmpty(nx, ny)) {
                bool canGrow = true;
                if (def.growth_requires_surface) {
                    canGrow = false;
                    for(int ox=-1; ox<=1; ++ox) {
                        for(int oy=-1; oy<=1; ++oy) {
                            if (ox==0 && oy==0) continue;
                            auto* nnb = world.get<BaseComponent>(nx+ox, ny+oy);
                            if (nnb && nnb->compMask != 0) { canGrow = true; break; }
                        }
                        if (canGrow) break;
                    }
                }
                if (canGrow) world.spawnParticle(this->id, nx, ny);
            }
        }
    }

    // Starvation
    if (!def.food_materials.empty()) {
        bool hasFood = false;
        for(int ox=-1; ox<=1; ++ox) {
            for(int oy=-1; oy<=1; ++oy) {
                if (ox==0 && oy==0) continue;
                auto* nb = world.getFast<BaseComponent>(ctx, ctx.x+ox, ctx.y+oy);
                if (nb && std::find(def.food_materials.begin(), def.food_materials.end(), nb->id) != def.food_materials.end()) {
                    hasFood = true; break;
                }
            }
            if (hasFood) break;
        }
        if (!hasFood) {
            if (auto* dur = world.getFast<DurabilityComponent>(ctx, ctx.x, ctx.y)) {
                dur->health--;
                if (dur->health <= 0) {
                    if (def.transform_on_starve_result != 0) {
                        dieAndReplace(ctx.x, ctx.y, def.transform_on_starve_result, world);
                    } else {
                        die(ctx.x, ctx.y, world);
                    }
                }
            }
        }
    }
}

bool Particle::executeGenericTraitsAndInteractions(const ParticleDef& def, BaseComponent* myBase, int myX, int myY, BaseComponent* otherBase, int otherX, int otherY, ParticleWorld& world) {
    if (!otherBase) return false;
    Particle* otherLogic = MaterialRegistry[static_cast<int>(otherBase->id)];
    if (!otherLogic) return false;

    if (def.has_trait_cleans_color) otherLogic->cleanColor(otherBase, otherX, otherY, world);
    if (def.has_trait_stains) otherLogic->stain(otherBase, otherX, otherY, def.stain_color, world);
    
    if (def.has_trait_ignites_when_touching_fire) {
        if (otherBase->flags.isIgnited || otherBase->flags.heated) {
            auto* myTherm = world.get<ThermalComponent>(myX, myY);
            receiveHeat(myBase, myTherm, myX, myY, 100, world); 
        }
    }
    
    if (def.has_trait_burns_objects) {
        auto* tTherm = world.get<ThermalComponent>(otherX, otherY);
        otherLogic->receiveHeat(otherBase, tTherm, otherX, otherY, def.burns_objects_heat, world);
        if (otherLogic->getGroup() != MaterialGroup::Gas) { 
            die(myX, myY, world);
            return true;
        }
    }

    if (def.has_trait_coolant || def.has_trait_boils_on_heat) {
        if (otherLogic->shouldApplyHeat(otherBase)) {
            if (def.has_trait_coolant) {
                auto* otherTherm = world.get<ThermalComponent>(otherX, otherY);
                if (otherLogic->receiveCooling(otherBase, otherTherm, otherX, otherY, 5, world)) {
                    if (def.transform_on_heat_result != 0) {
                        dieAndReplace(myX, myY, def.transform_on_heat_result, world);
                        return true;
                    }
                }
            } else if (def.has_trait_boils_on_heat) {
                if (def.transform_on_heat_result != 0) {
                    dieAndReplace(myX, myY, def.transform_on_heat_result, world);
                    return true;
                }
            }
        }
    }
    
    if (def.has_trait_corrosive) {
        auto* otherDur = world.get<DurabilityComponent>(otherX, otherY);
        if (otherDur) {
            bool willDie = (otherDur->health - def.corrosive_damage <= 0);
            if (otherLogic->corrode(otherBase, otherDur, otherX, otherY, def.corrosive_damage, world)) {
                if (willDie) world.spawnParticle(GetMatID("FlammableGas"), otherX, otherY);
                
                auto* myDur = world.get<DurabilityComponent>(myX, myY);
                if (myDur) {
                    myDur->health -= def.corrosive_self_cost;
                    checkIfDead(myBase, myDur, myX, myY, world);
                }
                return true;
            }
        }
    }

    if (def.has_trait_magmatize) {
         auto* otherDur = world.get<DurabilityComponent>(otherX, otherY);
         otherLogic->magmatize(otherBase, otherDur, otherX, otherY, Random::randInt(0, def.magmatize_damage), world);
    }

    auto it = def.interactions.find(otherBase->id);
    if (it != def.interactions.end()) {
        const auto& interaction = it->second;
        
        // Target Actions
        if (interaction.target_action == InteractionAction::Replace) {
            otherLogic->dieAndReplace(otherX, otherY, interaction.target_result, world);
        } else if (interaction.target_action == InteractionAction::Explode) {
            world.triggerExplosion(otherX, otherY, interaction.strength, interaction.strength);
        } else if (interaction.target_action == InteractionAction::Infect) {
            otherLogic->dieAndReplace(otherX, otherY, interaction.target_result != 0 ? interaction.target_result : myBase->id, world);
        } else if (interaction.target_action == InteractionAction::Consume) {
            otherLogic->die(otherX, otherY, world);
            auto* myDur = world.get<DurabilityComponent>(myX, myY);
            if (myDur) myDur->health = std::min(def.dur_health_max, myDur->health + std::max(1, interaction.strength));
        } else if (interaction.target_action == InteractionAction::Push) {
            if (auto* oKin = world.get<KinematicsComponent>(otherX, otherY)) {
                oKin->velocity.x += (otherX - myX) * std::max(10, interaction.strength * 10);
                oKin->velocity.y += (otherY - myY) * std::max(10, interaction.strength * 10);
                oKin->isFreeFalling = true;
                world.wakeParticle(otherX, otherY);
            }
        } else if (interaction.target_action == InteractionAction::Ignite) {
            auto* tTherm = world.get<ThermalComponent>(otherX, otherY);
            otherLogic->receiveHeat(otherBase, tTherm, otherX, otherY, 1000, world);
        }

        // Self Actions
        if (interaction.self_action == InteractionAction::Replace) {
            dieAndReplace(myX, myY, interaction.self_result, world);
            
            if (interaction.transform_neighbors) {
                for (int ox = -1; ox <= 1; ++ox) {
                    for (int oy = -1; oy <= 1; ++oy) {
                        if (ox == 0 && oy == 0) continue;
                        int tx = myX + ox;
                        int ty = myY + oy;
                        if (world.inBounds(tx, ty)) {
                            BaseComponent* nb = world.get<BaseComponent>(tx, ty);
                            if (nb) {
                                Particle* nLogic = MaterialRegistry[static_cast<int>(nb->id)];
                                if (nLogic && nLogic->getGroup() == MaterialGroup::Liquid) {
                                    nLogic->dieAndReplace(tx, ty, interaction.self_result, world);
                                }
                            }
                        }
                    }
                }
            }
            return true;
        } else if (interaction.self_action == InteractionAction::Die) {
            die(myX, myY, world);
            return true;
        }
    }
    return false;
}