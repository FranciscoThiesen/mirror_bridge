#pragma once
#include "image.hpp"
#include <vector>
#include <cmath>
#include <stdexcept>
#include <numbers>

namespace imgproc {

// Convolution kernel for image filtering
class Kernel {
private:
    std::vector<double> data_;
    int size_ = 1;  // Kernels are always square and odd-sized

public:
    // Default constructor creates identity kernel (1x1, value 1.0)
    Kernel() : data_({1.0}), size_(1) {}

    Kernel(int size, const std::vector<double>& values)
        : data_(values), size_(size) {
        if (size <= 0 || size % 2 == 0) {
            throw std::invalid_argument("Kernel size must be positive and odd");
        }
        if (values.size() != static_cast<size_t>(size * size)) {
            throw std::invalid_argument("Kernel values size mismatch");
        }
    }

    int size() const { return size_; }
    int radius() const { return size_ / 2; }

    double get(int x, int y) const {
        return data_[y * size_ + x];
    }

    // Normalize kernel so values sum to 1 (for blur kernels)
    Kernel normalized() const {
        double sum = 0.0;
        for (double v : data_) sum += v;

        if (std::abs(sum) < 1e-10) return *this;

        std::vector<double> normalized_data;
        normalized_data.reserve(data_.size());
        for (double v : data_) {
            normalized_data.push_back(v / sum);
        }
        return Kernel(size_, normalized_data);
    }

    // Pre-defined kernels

    // Box blur (simple averaging)
    static Kernel box_blur(int size) {
        if (size <= 0 || size % 2 == 0) {
            throw std::invalid_argument("Box blur size must be positive and odd");
        }
        double value = 1.0 / (size * size);
        std::vector<double> values(size * size, value);
        return Kernel(size, values);
    }

    // Gaussian blur kernel
    static Kernel gaussian(int size, double sigma) {
        if (size <= 0 || size % 2 == 0) {
            throw std::invalid_argument("Gaussian size must be positive and odd");
        }
        if (sigma <= 0) {
            throw std::invalid_argument("Sigma must be positive");
        }

        std::vector<double> values(size * size);
        int radius = size / 2;
        double sum = 0.0;
        double two_sigma_sq = 2.0 * sigma * sigma;

        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                double value = std::exp(-(x * x + y * y) / two_sigma_sq);
                values[(y + radius) * size + (x + radius)] = value;
                sum += value;
            }
        }

        // Normalize
        for (double& v : values) v /= sum;

        return Kernel(size, values);
    }

    // Sharpen kernel
    static Kernel sharpen() {
        return Kernel(3, {
             0, -1,  0,
            -1,  5, -1,
             0, -1,  0
        });
    }

    // Unsharp mask (stronger sharpening)
    static Kernel unsharp_mask() {
        return Kernel(5, {
            -1.0/256, -4.0/256,  -6.0/256, -4.0/256, -1.0/256,
            -4.0/256, -16.0/256, -24.0/256, -16.0/256, -4.0/256,
            -6.0/256, -24.0/256, 476.0/256, -24.0/256, -6.0/256,
            -4.0/256, -16.0/256, -24.0/256, -16.0/256, -4.0/256,
            -1.0/256, -4.0/256,  -6.0/256, -4.0/256, -1.0/256
        });
    }

    // Edge detection (Laplacian)
    static Kernel laplacian() {
        return Kernel(3, {
            0,  1, 0,
            1, -4, 1,
            0,  1, 0
        });
    }

    // Sobel edge detection (horizontal)
    static Kernel sobel_x() {
        return Kernel(3, {
            -1, 0, 1,
            -2, 0, 2,
            -1, 0, 1
        });
    }

    // Sobel edge detection (vertical)
    static Kernel sobel_y() {
        return Kernel(3, {
            -1, -2, -1,
             0,  0,  0,
             1,  2,  1
        });
    }

    // Emboss effect
    static Kernel emboss() {
        return Kernel(3, {
            -2, -1, 0,
            -1,  1, 1,
             0,  1, 2
        });
    }

    // Ridge detection (outline)
    static Kernel ridge() {
        return Kernel(3, {
            -1, -1, -1,
            -1,  8, -1,
            -1, -1, -1
        });
    }
};


// Apply convolution to an image
class Convolution {
public:
    // Apply a kernel to an image, producing a new image
    static Image apply(const Image& src, const Kernel& kernel) {
        Image result(src.width(), src.height());
        int radius = kernel.radius();

        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                double sum_r = 0, sum_g = 0, sum_b = 0;

                for (int ky = -radius; ky <= radius; ++ky) {
                    for (int kx = -radius; kx <= radius; ++kx) {
                        Color pixel = src.get_pixel_clamped(x + kx, y + ky);
                        double weight = kernel.get(kx + radius, ky + radius);

                        sum_r += pixel.r * weight;
                        sum_g += pixel.g * weight;
                        sum_b += pixel.b * weight;
                    }
                }

                // Clamp results to valid range
                result.set_pixel_unchecked(x, y, Color{
                    static_cast<uint8_t>(std::clamp(sum_r, 0.0, 255.0)),
                    static_cast<uint8_t>(std::clamp(sum_g, 0.0, 255.0)),
                    static_cast<uint8_t>(std::clamp(sum_b, 0.0, 255.0)),
                    src.get_pixel_unchecked(x, y).a  // Preserve alpha
                });
            }
        }

        return result;
    }

    // Sobel edge detection (combines X and Y gradients)
    static Image sobel_edges(const Image& src) {
        Image gray = src.to_grayscale();
        Image grad_x = apply(gray, Kernel::sobel_x());
        Image grad_y = apply(gray, Kernel::sobel_y());

        Image result(src.width(), src.height());

        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                Color px = grad_x.get_pixel_unchecked(x, y);
                Color py = grad_y.get_pixel_unchecked(x, y);

                // Gradient magnitude: sqrt(Gx^2 + Gy^2)
                double gx = px.r;
                double gy = py.r;
                double magnitude = std::sqrt(gx * gx + gy * gy);

                uint8_t edge = static_cast<uint8_t>(std::clamp(magnitude, 0.0, 255.0));
                result.set_pixel_unchecked(x, y, Color{edge, edge, edge, 255});
            }
        }

        return result;
    }

    // Box blur (optimized using integral image for large kernels)
    static Image box_blur(const Image& src, int radius) {
        if (radius <= 0) {
            throw std::invalid_argument("Blur radius must be positive");
        }

        // For small radii, use standard convolution
        if (radius <= 2) {
            return apply(src, Kernel::box_blur(radius * 2 + 1));
        }

        // For larger radii, use separable filter (horizontal then vertical pass)
        Image temp(src.width(), src.height());
        Image result(src.width(), src.height());
        int kernel_size = radius * 2 + 1;
        double divisor = kernel_size;

        // Horizontal pass
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                double sum_r = 0, sum_g = 0, sum_b = 0;

                for (int kx = -radius; kx <= radius; ++kx) {
                    Color pixel = src.get_pixel_clamped(x + kx, y);
                    sum_r += pixel.r;
                    sum_g += pixel.g;
                    sum_b += pixel.b;
                }

                temp.set_pixel_unchecked(x, y, Color{
                    static_cast<uint8_t>(sum_r / divisor),
                    static_cast<uint8_t>(sum_g / divisor),
                    static_cast<uint8_t>(sum_b / divisor),
                    src.get_pixel_unchecked(x, y).a
                });
            }
        }

        // Vertical pass
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                double sum_r = 0, sum_g = 0, sum_b = 0;

                for (int ky = -radius; ky <= radius; ++ky) {
                    Color pixel = temp.get_pixel_clamped(x, y + ky);
                    sum_r += pixel.r;
                    sum_g += pixel.g;
                    sum_b += pixel.b;
                }

                result.set_pixel_unchecked(x, y, Color{
                    static_cast<uint8_t>(sum_r / divisor),
                    static_cast<uint8_t>(sum_g / divisor),
                    static_cast<uint8_t>(sum_b / divisor),
                    temp.get_pixel_unchecked(x, y).a
                });
            }
        }

        return result;
    }

    // Gaussian blur (separable implementation)
    static Image gaussian_blur(const Image& src, double sigma) {
        if (sigma <= 0) {
            throw std::invalid_argument("Sigma must be positive");
        }

        // Kernel size based on sigma (covers ~99.7% of distribution)
        int radius = static_cast<int>(std::ceil(sigma * 3));
        if (radius < 1) radius = 1;

        // Generate 1D Gaussian kernel
        std::vector<double> kernel_1d(radius * 2 + 1);
        double sum = 0.0;
        double two_sigma_sq = 2.0 * sigma * sigma;

        for (int i = -radius; i <= radius; ++i) {
            double value = std::exp(-(i * i) / two_sigma_sq);
            kernel_1d[i + radius] = value;
            sum += value;
        }
        for (double& v : kernel_1d) v /= sum;

        // Separable filter: horizontal then vertical
        Image temp(src.width(), src.height());
        Image result(src.width(), src.height());

        // Horizontal pass
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                double sum_r = 0, sum_g = 0, sum_b = 0;

                for (int kx = -radius; kx <= radius; ++kx) {
                    Color pixel = src.get_pixel_clamped(x + kx, y);
                    double weight = kernel_1d[kx + radius];
                    sum_r += pixel.r * weight;
                    sum_g += pixel.g * weight;
                    sum_b += pixel.b * weight;
                }

                temp.set_pixel_unchecked(x, y, Color{
                    static_cast<uint8_t>(std::clamp(sum_r, 0.0, 255.0)),
                    static_cast<uint8_t>(std::clamp(sum_g, 0.0, 255.0)),
                    static_cast<uint8_t>(std::clamp(sum_b, 0.0, 255.0)),
                    src.get_pixel_unchecked(x, y).a
                });
            }
        }

        // Vertical pass
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                double sum_r = 0, sum_g = 0, sum_b = 0;

                for (int ky = -radius; ky <= radius; ++ky) {
                    Color pixel = temp.get_pixel_clamped(x, y + ky);
                    double weight = kernel_1d[ky + radius];
                    sum_r += pixel.r * weight;
                    sum_g += pixel.g * weight;
                    sum_b += pixel.b * weight;
                }

                result.set_pixel_unchecked(x, y, Color{
                    static_cast<uint8_t>(std::clamp(sum_r, 0.0, 255.0)),
                    static_cast<uint8_t>(std::clamp(sum_g, 0.0, 255.0)),
                    static_cast<uint8_t>(std::clamp(sum_b, 0.0, 255.0)),
                    temp.get_pixel_unchecked(x, y).a
                });
            }
        }

        return result;
    }
};

} // namespace imgproc
