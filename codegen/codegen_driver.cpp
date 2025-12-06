// Codegen driver for integration test
// This file is compiled with the reflection compiler to generate C++17 code

#include "codegen/mirror_bridge_codegen.hpp"
#include "sample_class.hpp"

int main() {
    mirror_bridge::codegen::generate_module("sample_codegen",
        mirror_bridge::codegen::bind<Calculator>("Calculator"),
        mirror_bridge::codegen::bind<Point2D>("Point2D")
    );
    return 0;
}
