#!/usr/bin/env python3
"""Render Mandelbrot fractal using C++ bindings"""

import sys
sys.path.insert(0, 'build')

import mandelbrot

print("Mandelbrot Fractal Renderer (Python + C++)")
print("=" * 50)

# Create renderer
m = mandelbrot.Mandelbrot(800, 600, 256)
print(f"Resolution: {m.width} x {m.height}")
print(f"Max iterations: {m.max_iter}")

# Classic view
print("\nRendering classic Mandelbrot view...")
ppm = m.to_ppm()
with open("build/mandelbrot_python.ppm", "w") as f:
    f.write(ppm)
print("  Saved: build/mandelbrot_python.ppm")

# Zoomed view - Seahorse Valley
print("\nRendering zoomed view (Seahorse Valley)...")
m2 = mandelbrot.Mandelbrot(800, 600, 512)
m2.set_viewport(-0.75, -0.73, 0.1, 0.115)
ppm2 = m2.to_ppm()
with open("build/mandelbrot_python_zoom.ppm", "w") as f:
    f.write(ppm2)
print("  Saved: build/mandelbrot_python_zoom.ppm")

# ASCII preview
print("\nASCII Preview:")
print("-" * 80)
ascii_art = m.to_ascii(78, 30)
print(ascii_art)
print("-" * 80)

print("\nDone!")
