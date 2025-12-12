#pragma once
#include <string>

// A simple greeter class to demonstrate Mirror Bridge basics
struct Greeter {
    std::string name = "World";
    int greeting_count = 0;

    std::string greet() {
        greeting_count++;
        return "Hello, " + name + "!";
    }

    void reset() {
        greeting_count = 0;
    }
};
