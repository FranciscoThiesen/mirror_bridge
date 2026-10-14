#include "src/engine.hpp"

namespace engine {

int Engine::step() { return ++seed; }

void accumulate(const std::vector<double>& xs, double& total) {
    total = 0.0;
    for (double x : xs) total += x;
}

}  // namespace engine
