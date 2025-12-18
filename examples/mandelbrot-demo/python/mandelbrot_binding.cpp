#include "mirror_bridge.hpp"
#include "mandelbrot.hpp"

MIRROR_BRIDGE_MODULE(mandelbrot,
    mirror_bridge::bind_class<Mandelbrot>(m, "Mandelbrot");
)
