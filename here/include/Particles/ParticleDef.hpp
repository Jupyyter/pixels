#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <SFML/Graphics.hpp>
#include "Constants.hpp"

enum class InteractionAction { None, Replace, Explode, Die, Ignite, Infect, Push, Consume };

struct InteractionDef {
    InteractionAction self_action = InteractionAction::None;
    MaterialID self_result = 0;
    InteractionAction target_action = InteractionAction::None;
    MaterialID target_result = 0;
    int strength = 0;
    bool transform_neighbors = false; 
};

struct ParticleDef {
    std::string name;
    MaterialGroup group = MaterialGroup::MovableSolid;
    std::string logic_class;
    
    std::vector<sf::Color> colors;
    
    bool has_fluid = false;
    int fluid_density = 1;
    int fluid_dispersion = 1;
    
    bool has_durability = false;
    int dur_health = 1;
    int dur_health_max = 1; 
    int dur_expRes = 0;
    
    bool has_thermal = false;
    int therm_temp = 0;
    int therm_flamRes = 0;
    int therm_heat = 0;
    int therm_fireDmg = 0;
    
    float gas_buoyancy = 0.0f;
    float gas_chaos = 0.0f;
    
    int decay_rate = 0;             
    int decay_rate_ignited = 0;     
    
    bool scatter_on_spawn = false;
    bool ignite_on_spawn = false;
    bool heated_on_spawn = false;
    bool flutter_fall = false;
    
    int transform_on_rest_ticks = 0;
    MaterialID transform_on_rest_result = 0;
    
    MaterialID transform_on_health_zero_result = 0;
    MaterialID transform_on_health_zero_ignited_result = 0;
    float transform_on_health_zero_ignited_chance = 1.0f;
    
    MaterialID transform_on_heat_result = 0;
    MaterialID transform_on_min_temp_result = 0;
    int min_temp_threshold = 0;
    bool min_temp_transform_neighbors = false;
    
    bool has_trait_stains = false;
    sf::Color stain_color;
    bool has_trait_corrosive = false;
    int corrosive_damage = 10;   
    int corrosive_self_cost = 1;
    bool has_trait_magmatize = false;
    int magmatize_damage = 10;
    
    bool has_trait_coolant = false;
    bool has_trait_cleans_color = false;
    bool has_trait_boils_on_heat = false;
    bool has_trait_ignites_when_touching_fire = false;
    bool has_trait_burns_objects = false;
    int burns_objects_heat = 10;
    bool has_trait_explosive_on_ignite = false;
    int explosive_radius = 15;
    int explosive_strength = 10;
    float spark_chance = -1.0f; 
    
    bool immune_to_magmatize = false;
    bool immune_to_fire = false;
    bool immune_to_corrosion = false; 

    // --- ELECTRICITY ---
    bool is_conductive = false;
    MaterialID transform_on_charged_result = 0;
    bool generates_charge = false;
    
    // --- ORGANIC GROWTH ---
    float growth_rate = 0.0f; 
    bool growth_requires_surface = false;
    std::vector<MaterialID> food_materials;
    MaterialID transform_on_starve_result = 0;
    
    // --- ADVANCED PHYSICS ---
    float stickiness = 0.0f; 
    float bounciness = 0.0f; 
    float friction = 0.5f;   
    int viscosity = 1;       
    bool anti_gravity = false; 

    // --- ADVANCED THERMODYNAMICS / PRESSURE ---
    MaterialID transform_on_max_temp_result = 0;
    int max_temp_threshold = 1000;
    MaterialID transform_on_crush_result = 0;
    bool smolders = false; 
    
    std::unordered_map<MaterialID, InteractionDef> interactions;
    
    sf::Color getRandomColor() const {
        if (colors.empty()) return sf::Color::White;
        return colors[rand() % colors.size()];
    }
};

extern std::unordered_map<std::string, MaterialID> GlobalMaterialNameToID;
extern std::unordered_map<MaterialID, ParticleDef> GlobalParticleDefs;

inline MaterialID GetMatID(const std::string& name) {
    auto it = GlobalMaterialNameToID.find(name);
    if (it != GlobalMaterialNameToID.end()) return it->second;
    return 0; 
}