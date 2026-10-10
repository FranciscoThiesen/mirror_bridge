#pragma once
namespace xh {
struct Point { double x = 1.0; double y = 2.0; double sum() const { return x + y; } };
struct Circle { double r = 3.0; };
}  // namespace xh

#ifdef XH_EXTRA_CLASS
namespace xh {
// Only declared when the macro is defined, which is how a real library gates
// a platform or feature section. The reflection pass and the module compile
// have to agree about whether it exists.
struct Gated { int value = 42; };
}  // namespace xh
#endif
