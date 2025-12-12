#pragma once
#include "color.hpp"
#include <vector>
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace imgproc {

// 2D image buffer with RGBA pixels
class Image {
private:
    std::vector<Color> pixels_;
    int width_ = 0;
    int height_ = 0;

public:
    Image() = default;

    Image(int width, int height) : width_(width), height_(height) {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument("Image dimensions must be positive");
        }
        pixels_.resize(static_cast<size_t>(width) * height);
    }

    Image(int width, int height, Color fill) : Image(width, height) {
        std::fill(pixels_.begin(), pixels_.end(), fill);
    }

    int width() const { return width_; }
    int height() const { return height_; }
    int pixel_count() const { return width_ * height_; }
    bool empty() const { return pixels_.empty(); }

    // Bounds checking
    bool in_bounds(int x, int y) const {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }

    // Pixel access with bounds checking
    Color get_pixel(int x, int y) const {
        if (!in_bounds(x, y)) {
            throw std::out_of_range("Pixel coordinates out of bounds");
        }
        return pixels_[y * width_ + x];
    }

    void set_pixel(int x, int y, Color color) {
        if (!in_bounds(x, y)) {
            throw std::out_of_range("Pixel coordinates out of bounds");
        }
        pixels_[y * width_ + x] = color;
    }

    // Unchecked access for performance-critical inner loops
    Color get_pixel_unchecked(int x, int y) const {
        return pixels_[y * width_ + x];
    }

    void set_pixel_unchecked(int x, int y, Color color) {
        pixels_[y * width_ + x] = color;
    }

    // Get pixel with clamped coordinates (for convolution edge handling)
    Color get_pixel_clamped(int x, int y) const {
        x = std::clamp(x, 0, width_ - 1);
        y = std::clamp(y, 0, height_ - 1);
        return pixels_[y * width_ + x];
    }

    // Fill entire image with a color
    void fill(Color color) {
        std::fill(pixels_.begin(), pixels_.end(), color);
    }

    // Create a deep copy
    Image clone() const {
        Image copy(width_, height_);
        copy.pixels_ = pixels_;
        return copy;
    }

    // Extract a rectangular region
    Image crop(int x, int y, int crop_width, int crop_height) const {
        if (x < 0 || y < 0 || x + crop_width > width_ || y + crop_height > height_) {
            throw std::out_of_range("Crop region exceeds image bounds");
        }

        Image result(crop_width, crop_height);
        for (int row = 0; row < crop_height; ++row) {
            for (int col = 0; col < crop_width; ++col) {
                result.set_pixel_unchecked(col, row, get_pixel_unchecked(x + col, y + row));
            }
        }
        return result;
    }

    // Resize using nearest-neighbor interpolation
    Image resize_nearest(int new_width, int new_height) const {
        if (new_width <= 0 || new_height <= 0) {
            throw std::invalid_argument("New dimensions must be positive");
        }

        Image result(new_width, new_height);
        double x_ratio = static_cast<double>(width_) / new_width;
        double y_ratio = static_cast<double>(height_) / new_height;

        for (int y = 0; y < new_height; ++y) {
            for (int x = 0; x < new_width; ++x) {
                int src_x = static_cast<int>(x * x_ratio);
                int src_y = static_cast<int>(y * y_ratio);
                result.set_pixel_unchecked(x, y, get_pixel_unchecked(src_x, src_y));
            }
        }
        return result;
    }

    // Resize using bilinear interpolation
    Image resize_bilinear(int new_width, int new_height) const {
        if (new_width <= 0 || new_height <= 0) {
            throw std::invalid_argument("New dimensions must be positive");
        }

        Image result(new_width, new_height);
        double x_ratio = static_cast<double>(width_ - 1) / new_width;
        double y_ratio = static_cast<double>(height_ - 1) / new_height;

        for (int y = 0; y < new_height; ++y) {
            for (int x = 0; x < new_width; ++x) {
                double src_x = x * x_ratio;
                double src_y = y * y_ratio;

                int x0 = static_cast<int>(src_x);
                int y0 = static_cast<int>(src_y);
                int x1 = std::min(x0 + 1, width_ - 1);
                int y1 = std::min(y0 + 1, height_ - 1);

                double x_frac = src_x - x0;
                double y_frac = src_y - y0;

                // Get the four neighboring pixels
                Color c00 = get_pixel_unchecked(x0, y0);
                Color c10 = get_pixel_unchecked(x1, y0);
                Color c01 = get_pixel_unchecked(x0, y1);
                Color c11 = get_pixel_unchecked(x1, y1);

                // Interpolate horizontally then vertically
                Color top = c00.lerp(c10, x_frac);
                Color bottom = c01.lerp(c11, x_frac);
                result.set_pixel_unchecked(x, y, top.lerp(bottom, y_frac));
            }
        }
        return result;
    }

    // Flip horizontally
    Image flip_horizontal() const {
        Image result(width_, height_);
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                result.set_pixel_unchecked(width_ - 1 - x, y, get_pixel_unchecked(x, y));
            }
        }
        return result;
    }

    // Flip vertically
    Image flip_vertical() const {
        Image result(width_, height_);
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                result.set_pixel_unchecked(x, height_ - 1 - y, get_pixel_unchecked(x, y));
            }
        }
        return result;
    }

    // Rotate 90 degrees clockwise
    Image rotate_90_cw() const {
        Image result(height_, width_);
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                result.set_pixel_unchecked(height_ - 1 - y, x, get_pixel_unchecked(x, y));
            }
        }
        return result;
    }

    // Convert to grayscale
    Image to_grayscale() const {
        Image result(width_, height_);
        for (int i = 0; i < pixel_count(); ++i) {
            result.pixels_[i] = pixels_[i].to_grayscale();
        }
        return result;
    }

    // Adjust brightness for entire image
    Image adjust_brightness(double factor) const {
        Image result(width_, height_);
        for (int i = 0; i < pixel_count(); ++i) {
            result.pixels_[i] = pixels_[i].adjust_brightness(factor);
        }
        return result;
    }

    // Adjust contrast for entire image
    Image adjust_contrast(double factor) const {
        Image result(width_, height_);
        for (int i = 0; i < pixel_count(); ++i) {
            result.pixels_[i] = pixels_[i].adjust_contrast(factor);
        }
        return result;
    }

    // Invert colors
    Image invert() const {
        Image result(width_, height_);
        for (int i = 0; i < pixel_count(); ++i) {
            result.pixels_[i] = pixels_[i].invert();
        }
        return result;
    }

    // Compute average color
    Color average_color() const {
        if (empty()) return Color{};

        uint64_t sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
        for (const auto& pixel : pixels_) {
            sum_r += pixel.r;
            sum_g += pixel.g;
            sum_b += pixel.b;
            sum_a += pixel.a;
        }

        int count = pixel_count();
        return Color{
            static_cast<uint8_t>(sum_r / count),
            static_cast<uint8_t>(sum_g / count),
            static_cast<uint8_t>(sum_b / count),
            static_cast<uint8_t>(sum_a / count)
        };
    }

    // Composite another image on top at given position
    void composite(const Image& overlay, int x, int y) {
        for (int oy = 0; oy < overlay.height_; ++oy) {
            for (int ox = 0; ox < overlay.width_; ++ox) {
                int dest_x = x + ox;
                int dest_y = y + oy;
                if (in_bounds(dest_x, dest_y)) {
                    Color src = overlay.get_pixel_unchecked(ox, oy);
                    Color dst = get_pixel_unchecked(dest_x, dest_y);
                    set_pixel_unchecked(dest_x, dest_y, src.blend_over(dst));
                }
            }
        }
    }

    // Access raw pixel data (for interop)
    const std::vector<Color>& pixels() const { return pixels_; }
    std::vector<Color>& pixels() { return pixels_; }

    // Get flat array of RGB values (for Python numpy interop)
    std::vector<uint8_t> to_rgb_bytes() const {
        std::vector<uint8_t> bytes;
        bytes.reserve(pixels_.size() * 3);
        for (const auto& p : pixels_) {
            bytes.push_back(p.r);
            bytes.push_back(p.g);
            bytes.push_back(p.b);
        }
        return bytes;
    }

    // Get flat array of RGBA values
    std::vector<uint8_t> to_rgba_bytes() const {
        std::vector<uint8_t> bytes;
        bytes.reserve(pixels_.size() * 4);
        for (const auto& p : pixels_) {
            bytes.push_back(p.r);
            bytes.push_back(p.g);
            bytes.push_back(p.b);
            bytes.push_back(p.a);
        }
        return bytes;
    }
};

} // namespace imgproc
