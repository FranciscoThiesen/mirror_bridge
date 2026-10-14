#pragma once

#include <string>
#include <vector>

// A library that is not header-only: `step` is declared here and defined in
// engine.cpp, which nothing links unless the caller says so. Open3D is the
// real case -- the module built, and then raised
//
//   ImportError: open3d_compat.so: undefined symbol:
//   _ZTIN6open3d8geometry10PointCloudE
//
// the first time anyone imported it, long after `generate` printed a green
// banner. A module that cannot be imported is not a module.
namespace engine {

struct Engine {
    int seed = 1;

    int step();                                  // defined in engine.cpp
    int twice() const { return seed * 2; }
};

// Declined at compile time, announced at module init: the import is the only
// place `generate` can hear about it.
void accumulate(const std::vector<double>& xs, double& total);

}  // namespace engine
