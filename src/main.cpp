#include "app/Application.h"

#include <exception>
#include <iostream>

int main() {
    try {
        Application application;
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }
}
