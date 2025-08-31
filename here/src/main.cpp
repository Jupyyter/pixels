#include <iostream>
#include <stdexcept>
#include "SandSim.hpp"

int main() {
    try {
        SandSim::SandSimApp app;
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return -1;
    }
    
    return 0;
}