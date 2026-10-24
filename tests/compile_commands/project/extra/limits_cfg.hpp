#pragma once

// Reachable only through an -I that no mirror_bridge command line mentions.
// The compile database is the only thing that knows where this lives.
namespace widget {
inline constexpr int kMaxSlots = 7;
}
