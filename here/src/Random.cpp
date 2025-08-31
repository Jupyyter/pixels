#include "Random.hpp"

namespace SandSim {
    std::random_device Random::rd{};
    std::mt19937 Random::gen{};
    bool Random::initialized = false;
}