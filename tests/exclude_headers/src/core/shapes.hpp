#pragma once
namespace xh {
struct Point { double x = 1.0; double y = 2.0; double sum() const { return x + y; } };
struct Circle { double r = 3.0; };
}  // namespace xh
