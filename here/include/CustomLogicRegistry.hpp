#pragma once
#include "Particles/ParticleDef.hpp"
#include <functional>
#include <unordered_map>
#include <string>

class Particle;

// A function pointer type that takes an ID and a Def, and returns a new Particle
using ParticleInstantiator = std::function<Particle*(MaterialID, const ParticleDef&)>;

class CustomLogicRegistry {
public:
    static std::unordered_map<std::string, ParticleInstantiator> factories;
    static void Initialize();
};