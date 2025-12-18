#!/usr/bin/env python3
"""Generate zoom animation frames for Mandelbrot"""

import sys
import os
import math

sys.path.insert(0, 'build')
import mandelbrot

# Animation parameters
WIDTH = 640
HEIGHT = 480
MAX_ITER = 256
NUM_FRAMES = 60

# Start: full Mandelbrot view
START_X_MIN, START_X_MAX = -2.5, 1.0
START_Y_MIN, START_Y_MAX = -1.25, 1.25

# End: Seahorse Valley zoom
END_X_MIN, END_X_MAX = -0.7463, -0.7413
END_Y_MIN, END_Y_MAX = 0.1102, 0.1152

def lerp(a, b, t):
    """Linear interpolation"""
    return a + (b - a) * t

def ease_in_out(t):
    """Smooth easing function"""
    return t * t * (3 - 2 * t)

def generate_frames():
    """Generate all animation frames"""
    os.makedirs('build/frames', exist_ok=True)

    print(f"Generating {NUM_FRAMES} frames ({WIDTH}x{HEIGHT})...")
    print()

    m = mandelbrot.Mandelbrot(WIDTH, HEIGHT, MAX_ITER)

    for frame in range(NUM_FRAMES):
        # Calculate interpolation factor with easing
        t = frame / (NUM_FRAMES - 1)
        t_eased = ease_in_out(t)

        # Interpolate viewport (exponential for zoom feel)
        # Use log space for smoother zoom
        x_min = lerp(START_X_MIN, END_X_MIN, t_eased)
        x_max = lerp(START_X_MAX, END_X_MAX, t_eased)
        y_min = lerp(START_Y_MIN, END_Y_MIN, t_eased)
        y_max = lerp(START_Y_MAX, END_Y_MAX, t_eased)

        # Set viewport and render
        m.set_viewport(x_min, x_max, y_min, y_max)

        # Increase iterations as we zoom in for more detail
        zoom_factor = (START_X_MAX - START_X_MIN) / (x_max - x_min)
        m.max_iter = min(512, int(MAX_ITER + math.log2(max(1, zoom_factor)) * 30))

        ppm = m.to_ppm()

        filename = f'build/frames/frame_{frame:03d}.ppm'
        with open(filename, 'w') as f:
            f.write(ppm)

        # Progress indicator
        progress = (frame + 1) / NUM_FRAMES * 100
        bar = '=' * int(progress / 2) + '>' + ' ' * (50 - int(progress / 2))
        zoom_str = f"zoom: {zoom_factor:.1f}x"
        print(f"\r  [{bar}] {progress:5.1f}% ({zoom_str})", end='', flush=True)

    print()
    print()
    print(f"Generated {NUM_FRAMES} frames in build/frames/")
    print()
    print("To convert to GIF (on macOS):")
    print("  cd build/frames")
    print("  for f in *.ppm; do sips -s format png \"$f\" --out \"${f%.ppm}.png\"; done")
    print("  ffmpeg -framerate 15 -i frame_%03d.png -vf 'palettegen' palette.png")
    print("  ffmpeg -framerate 15 -i frame_%03d.png -i palette.png -lavfi 'paletteuse' ../mandelbrot_zoom.gif")

if __name__ == "__main__":
    generate_frames()
