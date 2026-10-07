#pragma once

// Nothing here for the class walk to find: the whole surface is a template,
// which the planner handles separately. An empty class list is the correct
// answer, and the CLI has to survive carrying one around.
namespace lib {

template <class T>
struct Box {
    T value{};
    T get() const { return value; }
};

using BoxInt = Box<int>;

}  // namespace lib
