#pragma once
// Stands in for the part of a third-party library you do not want bound --
// pmp-library's OpenGL viewer is the real case. An ordinary bindable class on
// purpose: the point is that --exclude keeps it out, not that it is awkward.
// There is no way to annotate this header when you do not own it.
namespace xh {
struct Viewer {
    int handle = 0;
    int doubled() const { return handle * 2; }
};
}  // namespace xh
