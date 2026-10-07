#pragma once

// The reflection walk declines to enter a namespace whose name starts with an
// underscore, because that is where an implementation puts its own, so it
// reads this header and reports nothing at all. A text scan does not care
// about namespaces and has an answer, and a header that built before has to
// keep building rather than becoming "no classes found".
namespace proj {
namespace _hidden {

struct Thing {
    int n = 1;
    int twice() const { return n * 2; }
};

}  // namespace _hidden
}  // namespace proj
