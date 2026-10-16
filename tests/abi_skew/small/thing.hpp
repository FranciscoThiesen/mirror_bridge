#pragma once
namespace xm {

// One team's revision of xm::Thing. Only data members: an out-of-line method
// would be emitted as a weak symbol in both modules and the dynamic linker
// would pick one, which is a different confusion than the one under test.
struct Thing {
    double a = 1.0;
};

}  // namespace xm
