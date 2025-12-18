#include "javascript/mirror_bridge_javascript.hpp"
#include "mandelbrot.hpp"

MIRROR_BRIDGE_JS_MODULE(mandelbrot,
    mirror_bridge::javascript::bind_class<Mandelbrot>(env, m, "Mandelbrot");
)
