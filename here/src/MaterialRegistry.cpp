#include "Constants.hpp"
#include "particles/Particle.hpp"
#include "particles/MovableSolid.hpp"
#include "particles/Gas.hpp"
#include "particles/Liquid.hpp"
#include "particles/ImmovableSolid.hpp"
// Include the special classes
#include "particles/ExplosiveContainer.hpp" 
#include "particles/Explosion.hpp" 

// --- 1. Create a Helper Template ---
// This acts as a bridge. By default, it calls the standard Particle methods.
template <typename T>
struct ParticleTraits {
    static MaterialGroup getGroup() { 
        return T::getStaticGroup(); 
    }
    static std::unique_ptr<Particle> create() { 
        return std::make_unique<T>(); 
    }
};

// --- 2. Specialize for 'Explosion' ---
// The compiler will use THIS version for Explosion, ignoring the default one.
// This prevents the "no member getStaticGroup" error.
template <>
struct ParticleTraits<Explosion> {
    static MaterialGroup getGroup() { return MaterialGroup::Special; }
    static std::unique_ptr<Particle> create() { return nullptr; } // Cannot create Explosion as a particle
};

// --- 3. Specialize for 'ExplosiveContainer' ---
// Needed because ExplosiveContainer has no default constructor (it needs velocity args).
template <>
struct ParticleTraits<ExplosiveContainer> {
    static MaterialGroup getGroup() { return MaterialGroup::Special; }
    static std::unique_ptr<Particle> create() { return nullptr; } // Don't spawn projectiles via brush
};

// --- 4. Define the Macro using the Traits ---
#define MAP_PARTICLE(ClassName, ...) \
    MaterialProps{ \
        MaterialID::ClassName, \
        #ClassName, \
        __VA_ARGS__, \
        ParticleTraits<ClassName>::getGroup(), \
        []() { return ParticleTraits<ClassName>::create(); } \
    }

// --- 5. Handle Non-Class entries (like EmptyParticle) ---
#define X_EXPAND(name, ...) \
    (std::string(#name) == "EmptyParticle") ? \
    MaterialProps{ MaterialID::name, "Empty", __VA_ARGS__, MaterialGroup::ImmovableSolid, [](){ return nullptr; } } : \
    MAP_PARTICLE(name, __VA_ARGS__),

// --- 6. Initialize the Vector ---
const std::vector<MaterialProps> ALL_MATERIALS = {
    PARTICLE_DATA(X_EXPAND)
};