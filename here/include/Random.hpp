#pragma once
#include <random>

namespace SandSim {
    class Random {
    private:
        static std::random_device rd;
        static std::mt19937 gen;
        static bool initialized;
        
        static void ensureInitialized() {
            if (!initialized) {
                gen.seed(rd());
                initialized = true;
            }
        }
        
    public:
        static void setSeed(uint32_t seed) {
            gen.seed(seed);
            initialized = true;
        }
        
        static int randInt(int min, int max) {
            ensureInitialized();
            std::uniform_int_distribution<int> dis(min, max);
            return dis(gen);
        }
        
        static float randFloat(float min, float max) {
            ensureInitialized();
            std::uniform_real_distribution<float> dis(min, max);
            return dis(gen);
        }
        
        static bool randBool() {
            return randInt(0, 1) == 1;
        }
        
        static bool chance(int oneInN) {
            return randInt(0, oneInN - 1) == 0;
        }
    };
}