#pragma once
#include <string>
#include <utility>
#include <vector>

// Parameters declared `T&&` on a free function. cxxopts has these (its
// `type_is_container<bool>` machinery takes a `bool&&`), and before
// forward_arg was used on this path they did not merely fail to bind: the
// storage slot is an lvalue, so the call did not compile and took the whole
// module with it.
namespace rv {

inline bool consume_bool(bool&& flag) { return flag; }

inline std::string consume_string(std::string&& text) {
    std::string taken = std::move(text);
    taken += "!";
    return taken;
}

inline std::size_t consume_vector(std::vector<int>&& values) {
    std::vector<int> taken = std::move(values);
    return taken.size();
}

// An rvalue-reference parameter alongside the ordinary kinds, because the
// fold expression expands them together and each one has its own branch in
// forward_arg.
inline std::string mixed(int by_value, const std::string& by_const_ref,
                         double&& by_rvalue_ref) {
    return by_const_ref + ":" + std::to_string(by_value) +
           ":" + std::to_string(static_cast<int>(by_rvalue_ref));
}

struct Counter {
    int total = 0;
    void add(int&& amount) { total += amount; }
};

}  // namespace rv
