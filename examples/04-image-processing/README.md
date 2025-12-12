# Example 04: Image Processing Library

A realistic image processing library demonstrating why C++ bindings matter for
compute-intensive Python applications.

## Why This Example?

Image processing is a canonical use case for C++ Python bindings:
- **Performance**: Pixel-level operations on megapixel images need native speed
- **Memory**: Large image buffers benefit from efficient C++ memory management
- **Algorithms**: DSP and convolution math are well-suited to C++ optimization

Libraries like OpenCV, Pillow (via libjpeg/libpng), and scikit-image all use
this pattern.

## Library Structure

```
src/
├── color.hpp      # RGBA/HSV color types, blending, color space conversion
├── image.hpp      # Image buffer class with transforms (resize, crop, flip)
├── kernel.hpp     # Convolution kernels and filter operations
└── histogram.hpp  # Statistical analysis and histogram equalization
```

## Features Demonstrated

### Color Operations (`color.hpp`)
- RGBA and HSV color representations
- Luminance calculation (Rec. 709 coefficients)
- Alpha blending with proper compositing
- Brightness/contrast adjustment
- Linear interpolation between colors

### Image Buffer (`image.hpp`)
- Efficient pixel buffer with bounds checking
- Bilinear and nearest-neighbor resize
- Crop, flip, and 90° rotation
- Image compositing (alpha blending overlay)
- Raw byte export for numpy interop

### Convolution Filters (`kernel.hpp`)
- Standard kernels: box blur, Gaussian, sharpen, emboss
- Edge detection: Sobel, Laplacian, ridge
- Separable filter optimization for Gaussian blur
- Configurable kernel sizes

### Histogram Analysis (`histogram.hpp`)
- Per-channel and luminance histograms
- Statistical measures: mean, std dev, median, mode
- Percentile calculation
- Histogram equalization (contrast enhancement)
- Otsu's method for automatic thresholding
- Adaptive histogram equalization (CLAHE-like)

## Building

```bash
# From the mirror_bridge root directory
mirror_bridge generate examples/04-image-processing/src/ \
    --module imgproc \
    --lang python \
    --output examples/04-image-processing/build/
```

## Usage from Python

```python
import imgproc

# Create an image
img = imgproc.Image(800, 600)

# Set pixels
color = imgproc.Color()
color.r, color.g, color.b = 255, 128, 0
img.set_pixel(100, 100, color)

# Apply Gaussian blur
blurred = imgproc.Convolution.gaussian_blur(img, 2.0)

# Detect edges
edges = imgproc.Convolution.sobel_edges(blurred)

# Analyze histogram
hist = imgproc.Histogram(edges)
print(f"Mean luminance: {hist.mean_luminance()}")
print(f"Dynamic range: {hist.dynamic_range()}")

# Enhance contrast
enhanced = imgproc.HistogramOps.equalize(img)
```

## Real-World Applications

This library structure mirrors production image processing code:

1. **Photo editing**: Brightness, contrast, color grading
2. **Computer vision**: Edge detection as preprocessing for feature detection
3. **Medical imaging**: Histogram equalization for X-ray/MRI enhancement
4. **Document processing**: Otsu thresholding for OCR preprocessing
5. **Game development**: Texture processing and procedural generation

## Performance Considerations

The library demonstrates several optimization patterns:

- **Separable filters**: Gaussian blur uses 2 1D passes instead of 2D convolution
  (O(n×k) instead of O(n×k²) where k is kernel size)
- **Clamped pixel access**: Edge handling without conditional branches in inner loop
- **Unchecked access**: Fast path for trusted coordinate access
- **Lookup tables**: Histogram operations use LUTs for O(1) pixel mapping

## What This Shows About mirror_bridge

- **Nested types**: `Color` used as member of `Image`, `ColorHSV` for conversions
- **Static methods**: `Kernel::gaussian()`, `ColorHSV::from_rgb()`
- **Containers**: `std::vector<uint8_t>` for byte arrays, `std::array` for histograms
- **Method chaining**: Operations return new `Image` objects
- **Exceptions**: Bounds checking throws `std::out_of_range`
