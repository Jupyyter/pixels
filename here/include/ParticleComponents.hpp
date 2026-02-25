#pragma once
#include <SFML/Graphics.hpp>

namespace ParticleComponents {

struct Phys {
  sf::Vector2f velocity{0.0f, 0.0f};
  float xThreshold = 0.0f;
  float yThreshold = 0.0f;
  float frictionFactor = 0.5f;
  float inertialResistance = 0.1f;
  int mass = 100;
  int stoppedMovingCount = 0;
  int stoppedMovingThreshold = 1;
  bool isFreeFalling = true;
};

struct Heat {
  int temperature = 0;
  int flammabilityResistance = 100;
  int resetFlammabilityResistance = 50;
  int heatFactor = 10;
  int coolingFactor = 5;
  int fireDamage = 3;
  bool isIgnited = false;
  bool heated = false;
};

struct Life {
  int health = 500;
  int lifeSpan = 0;
  int explosionResistance = 1;
};

struct Fluid {
  int density = 0;
  int dispersionRate = 0;
  float buoyancy = 0.0f;
  float chaosLevel = 0.0f;
};

} // namespace ParticleComponents
