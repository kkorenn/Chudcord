#include "core/app.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        discord::App app;
        if (app.init("config.json")) {
            app.run();
        } else {
            std::cerr << "Failed to initialize application. Check config.json." << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
