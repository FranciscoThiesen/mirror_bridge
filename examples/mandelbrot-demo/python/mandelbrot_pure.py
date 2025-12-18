"""Pure Python Mandelbrot implementation (no C++ bindings)"""

import math

def escape_time(real, imag, max_iter):
    """Compute escape time for a single point with smooth coloring"""
    zr, zi = 0.0, 0.0
    iter = 0

    while zr * zr + zi * zi <= 4.0 and iter < max_iter:
        temp = zr * zr - zi * zi + real
        zi = 2.0 * zr * zi + imag
        zr = temp
        iter += 1

    if iter == max_iter:
        return float(max_iter)

    # Smooth coloring
    log_zn = math.log(zr * zr + zi * zi) / 2.0
    nu = math.log(log_zn / math.log(2.0)) / math.log(2.0)
    return float(iter) + 1.0 - nu

def iter_to_rgb(iter_val, max_iter):
    """Convert iteration count to RGB color"""
    if iter_val >= max_iter:
        return (0, 0, 0)

    t = iter_val / max_iter
    hue = (t * 5.0 + 0.6) % 1.0
    sat = 0.85
    val = t / 0.02 if t < 0.02 else 1.0

    # HSV to RGB
    hi = int(hue * 6.0) % 6
    f = hue * 6.0 - hi
    p = val * (1.0 - sat)
    q = val * (1.0 - f * sat)
    tv = val * (1.0 - (1.0 - f) * sat)

    if hi == 0:
        rd, gd, bd = val, tv, p
    elif hi == 1:
        rd, gd, bd = q, val, p
    elif hi == 2:
        rd, gd, bd = p, val, tv
    elif hi == 3:
        rd, gd, bd = p, q, val
    elif hi == 4:
        rd, gd, bd = tv, p, val
    else:
        rd, gd, bd = val, p, q

    return (int(rd * 255), int(gd * 255), int(bd * 255))

def render_mandelbrot(width, height, max_iter, x_min, x_max, y_min, y_max):
    """Render Mandelbrot to RGB pixel list - THE HOT LOOP"""
    pixels = []

    x_scale = (x_max - x_min) / width
    y_scale = (y_max - y_min) / height

    for py in range(height):
        imag = y_min + py * y_scale
        for px in range(width):
            real = x_min + px * x_scale
            iter_val = escape_time(real, imag, max_iter)
            r, g, b = iter_to_rgb(iter_val, max_iter)
            pixels.extend([r, g, b])

    return pixels

def to_ppm(width, height, pixels):
    """Convert pixel data to PPM string"""
    ppm = f"P3\n{width} {height}\n255\n"
    for i in range(0, len(pixels), 3):
        ppm += f"{pixels[i]} {pixels[i+1]} {pixels[i+2]}"
        ppm += "\n" if ((i // 3) % width == width - 1) else " "
    return ppm

# For benchmarking
if __name__ == "__main__":
    import time

    width, height = 800, 600
    max_iter = 256
    x_min, x_max = -2.5, 1.0
    y_min, y_max = -1.25, 1.25

    print(f"Pure Python Mandelbrot ({width}x{height}, {max_iter} iterations)")

    start = time.perf_counter()
    pixels = render_mandelbrot(width, height, max_iter, x_min, x_max, y_min, y_max)
    elapsed = time.perf_counter() - start

    print(f"Render time: {elapsed:.3f} seconds")
    print(f"Pixels: {len(pixels) // 3}")
