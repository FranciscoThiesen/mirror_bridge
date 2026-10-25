#pragma once

// This header is deliberately unbuildable without the two things only the
// project's own build system knows: a define that selects the configuration,
// and an include directory holding the config header. A generate run that
// reaches a bound Widget has proved the compile database was read and that
// its flags reached the reflection pass, not merely that the file parsed.
#ifndef WIDGET_ENABLED
#error "WIDGET_ENABLED is not defined: the compile database was not read"
#endif

#include "limits_cfg.hpp"

namespace widget {

struct Widget {
    int slots = kMaxSlots;
    double weight = 1.5;

    int capacity() const { return slots * 2; }
};

}  // namespace widget
