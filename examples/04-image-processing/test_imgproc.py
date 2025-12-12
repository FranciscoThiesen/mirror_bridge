"""
Image Processing Library - Integration Tests

Demonstrates the imgproc C++ library from Python, showcasing:
- Color manipulation and blending
- Image buffer operations and transformations
- Convolution filters (blur, sharpen, edge detection)
- Histogram analysis and equalization
"""

import imgproc


def test_color_operations():
    """Test color manipulation and conversion"""
    print("=== Color Operations ===")

    # Create colors
    red = imgproc.Color()
    red.r, red.g, red.b, red.a = 255, 0, 0, 255

    blue = imgproc.Color()
    blue.r, blue.g, blue.b, blue.a = 0, 0, 255, 255

    # Test luminance (red should be darker than pure green)
    print(f"Red luminance: {red.luminance():.3f}")
    print(f"Blue luminance: {blue.luminance():.3f}")

    # Test grayscale conversion
    gray = red.to_grayscale()
    print(f"Red to grayscale: RGB({gray.r}, {gray.g}, {gray.b})")

    # Test color interpolation
    purple = red.lerp(blue, 0.5)
    print(f"Red lerp Blue (50%): RGB({purple.r}, {purple.g}, {purple.b})")

    # Test brightness adjustment
    bright = red.adjust_brightness(0.5)
    print(f"Red +50% brightness: RGB({bright.r}, {bright.g}, {bright.b})")

    # Test hex conversion
    print(f"Blue as hex: {blue.to_hex()}")


def test_color_hsv():
    """Test HSV color space conversion"""
    print("\n=== HSV Color Space ===")

    # Create an RGB color
    orange = imgproc.Color()
    orange.r, orange.g, orange.b = 255, 128, 0

    # Convert to HSV
    hsv = imgproc.ColorHSV.from_rgb(orange)
    print(f"Orange RGB(255,128,0) -> HSV({hsv.h:.1f}, {hsv.s:.2f}, {hsv.v:.2f})")

    # Rotate hue by 180 degrees (complement color)
    hsv.rotate_hue(180)
    complement = hsv.to_rgb()
    print(f"Hue rotated 180°: RGB({complement.r}, {complement.g}, {complement.b})")


def test_image_basics():
    """Test basic image operations"""
    print("\n=== Image Basics ===")

    # Create a 100x100 image with red fill
    red = imgproc.Color()
    red.r, red.g, red.b = 255, 0, 0

    img = imgproc.Image(100, 100, red)
    print(f"Created image: {img.width()}x{img.height()}, {img.pixel_count()} pixels")

    # Set a pixel
    green = imgproc.Color()
    green.r, green.g, green.b = 0, 255, 0

    img.set_pixel(50, 50, green)
    retrieved = img.get_pixel(50, 50)
    print(f"Set/get pixel at (50,50): RGB({retrieved.r}, {retrieved.g}, {retrieved.b})")

    # Test average color (should be almost entirely red)
    avg = img.average_color()
    print(f"Average color: RGB({avg.r}, {avg.g}, {avg.b})")


def test_image_transforms():
    """Test image transformations"""
    print("\n=== Image Transforms ===")

    # Create a gradient image for testing
    img = imgproc.Image(200, 100)
    for y in range(100):
        for x in range(200):
            color = imgproc.Color()
            color.r = int(x * 255 / 200)  # Red gradient left to right
            color.g = int(y * 255 / 100)  # Green gradient top to bottom
            color.b = 128
            img.set_pixel(x, y, color)

    # Test resize
    small = img.resize_nearest(50, 25)
    print(f"Resize nearest 200x100 -> {small.width()}x{small.height()}")

    bilinear = img.resize_bilinear(50, 25)
    print(f"Resize bilinear 200x100 -> {bilinear.width()}x{bilinear.height()}")

    # Test crop
    cropped = img.crop(50, 25, 100, 50)
    print(f"Crop region: {cropped.width()}x{cropped.height()}")

    # Test flip
    flipped_h = img.flip_horizontal()
    corner = flipped_h.get_pixel(0, 0)
    print(f"After horizontal flip, (0,0) should be right side: R={corner.r}")

    # Test rotation
    rotated = img.rotate_90_cw()
    print(f"Rotate 90° CW: {img.width()}x{img.height()} -> {rotated.width()}x{rotated.height()}")

    # Test grayscale
    gray = img.to_grayscale()
    gray_pixel = gray.get_pixel(100, 50)
    print(f"Grayscale center pixel: RGB({gray_pixel.r}, {gray_pixel.g}, {gray_pixel.b})")


def test_convolution():
    """Test convolution filters"""
    print("\n=== Convolution Filters ===")

    # Create a test image
    img = imgproc.Image(64, 64)
    white = imgproc.Color()
    white.r, white.g, white.b = 255, 255, 255
    black = imgproc.Color()
    black.r, black.g, black.b = 0, 0, 0

    # Create a checkerboard pattern
    for y in range(64):
        for x in range(64):
            if (x // 8 + y // 8) % 2 == 0:
                img.set_pixel(x, y, white)
            else:
                img.set_pixel(x, y, black)

    # Test different kernels
    box = imgproc.Kernel.box_blur(3)
    print(f"Box blur kernel size: {box.size()}")

    gaussian = imgproc.Kernel.gaussian(5, 1.0)
    print(f"Gaussian kernel size: {gaussian.size()}")

    sharpen = imgproc.Kernel.sharpen()
    print(f"Sharpen kernel size: {sharpen.size()}")

    # Apply convolution
    blurred = imgproc.Convolution.apply(img, gaussian)
    print(f"Applied Gaussian blur to {img.width()}x{img.height()} image")

    # Test box blur optimized implementation
    box_blurred = imgproc.Convolution.box_blur(img, 5)
    print(f"Applied optimized box blur (radius 5)")

    # Test Gaussian blur optimized implementation
    gauss_blurred = imgproc.Convolution.gaussian_blur(img, 2.0)
    print(f"Applied optimized Gaussian blur (sigma 2.0)")

    # Test edge detection
    edges = imgproc.Convolution.sobel_edges(img)
    edge_pixel = edges.get_pixel(32, 32)
    print(f"Sobel edge detection result - center pixel luminance: {edge_pixel.r}")


def test_histogram():
    """Test histogram analysis"""
    print("\n=== Histogram Analysis ===")

    # Create an image with known luminance distribution
    img = imgproc.Image(100, 100)
    for y in range(100):
        for x in range(100):
            color = imgproc.Color()
            # Gradient creates predictable histogram
            gray = int((x + y) * 255 / 200)
            color.r, color.g, color.b = gray, gray, gray
            img.set_pixel(x, y, color)

    # Compute histogram
    hist = imgproc.Histogram(img)

    print(f"Total pixels: {hist.total_pixels()}")
    print(f"Mean luminance: {hist.mean_luminance():.1f}")
    print(f"Std dev: {hist.std_dev_luminance():.1f}")
    print(f"Median: {hist.median_luminance()}")
    print(f"Mode: {hist.mode_luminance()}")
    print(f"Dynamic range: {hist.dynamic_range()}")

    print(f"Shadow %: {hist.shadow_percentage():.1f}")
    print(f"Midtone %: {hist.midtone_percentage():.1f}")
    print(f"Highlight %: {hist.highlight_percentage():.1f}")

    print(f"Is low key: {hist.is_low_key()}")
    print(f"Is high key: {hist.is_high_key()}")
    print(f"Has good contrast: {hist.has_good_contrast()}")

    # Get 25th and 75th percentiles
    print(f"25th percentile: {hist.percentile(25)}")
    print(f"75th percentile: {hist.percentile(75)}")


def test_histogram_operations():
    """Test histogram-based image operations"""
    print("\n=== Histogram Operations ===")

    # Create a low-contrast image
    img = imgproc.Image(100, 100)
    for y in range(100):
        for x in range(100):
            color = imgproc.Color()
            # Compress range to 100-155
            gray = 100 + int((x + y) * 55 / 200)
            color.r, color.g, color.b = gray, gray, gray
            img.set_pixel(x, y, color)

    hist_before = imgproc.Histogram(img)
    print(f"Before: range {hist_before.dynamic_range()}, mean {hist_before.mean_luminance():.1f}")

    # Apply histogram equalization
    equalized = imgproc.HistogramOps.equalize(img)
    hist_after = imgproc.Histogram(equalized)
    print(f"After equalization: range {hist_after.dynamic_range()}, mean {hist_after.mean_luminance():.1f}")

    # Apply auto levels
    auto_leveled = imgproc.HistogramOps.auto_levels(img)
    hist_auto = imgproc.Histogram(auto_leveled)
    print(f"After auto levels: range {hist_auto.dynamic_range()}, mean {hist_auto.mean_luminance():.1f}")

    # Test Otsu's threshold
    threshold = imgproc.HistogramOps.otsu_threshold(hist_before)
    print(f"Otsu threshold: {threshold}")

    # Apply threshold
    binary = imgproc.HistogramOps.threshold(img, threshold)
    binary_pixel = binary.get_pixel(50, 50)
    print(f"Binary image center: {binary_pixel.r}")


def test_integration():
    """Full pipeline test combining multiple operations"""
    print("\n=== Integration Pipeline ===")

    # Create a synthetic "photo" with noise and low contrast
    img = imgproc.Image(128, 128)
    import random
    random.seed(42)

    for y in range(128):
        for x in range(128):
            # Base gradient
            base = int((x + y) * 100 / 256) + 50
            # Add noise
            noise = random.randint(-20, 20)
            value = max(0, min(255, base + noise))

            color = imgproc.Color()
            color.r, color.g, color.b = value, value, value
            img.set_pixel(x, y, color)

    print(f"Created synthetic image: {img.width()}x{img.height()}")

    # Pipeline: denoise -> enhance contrast -> detect edges
    # Step 1: Gaussian blur to reduce noise
    denoised = imgproc.Convolution.gaussian_blur(img, 1.5)
    print("Step 1: Applied Gaussian blur (sigma=1.5)")

    # Step 2: Histogram equalization
    enhanced = imgproc.HistogramOps.equalize(denoised)
    hist = imgproc.Histogram(enhanced)
    print(f"Step 2: Histogram equalization (new range: {hist.dynamic_range()})")

    # Step 3: Edge detection
    edges = imgproc.Convolution.sobel_edges(enhanced)
    print("Step 3: Sobel edge detection")

    # Analyze final result
    edge_hist = imgproc.Histogram(edges)
    print(f"Final edge image stats: mean={edge_hist.mean_luminance():.1f}, std={edge_hist.std_dev_luminance():.1f}")


if __name__ == "__main__":
    test_color_operations()
    test_color_hsv()
    test_image_basics()
    test_image_transforms()
    test_convolution()
    test_histogram()
    test_histogram_operations()
    test_integration()

    print("\n=== All tests completed! ===")
