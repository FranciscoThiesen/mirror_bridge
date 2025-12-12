#pragma once
#include "image.hpp"
#include <array>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace imgproc {

// Image histogram analysis for statistical operations
class Histogram {
private:
    std::array<int, 256> red_{};
    std::array<int, 256> green_{};
    std::array<int, 256> blue_{};
    std::array<int, 256> luminance_{};
    int total_pixels_ = 0;

public:
    Histogram() = default;

    // Build histogram from an image
    explicit Histogram(const Image& img) {
        compute(img);
    }

    void compute(const Image& img) {
        // Reset counts
        red_.fill(0);
        green_.fill(0);
        blue_.fill(0);
        luminance_.fill(0);
        total_pixels_ = img.pixel_count();

        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                Color pixel = img.get_pixel_unchecked(x, y);
                red_[pixel.r]++;
                green_[pixel.g]++;
                blue_[pixel.b]++;

                // Compute luminance bin
                int lum = static_cast<int>(pixel.luminance() * 255.0);
                lum = std::clamp(lum, 0, 255);
                luminance_[lum]++;
            }
        }
    }

    int total_pixels() const { return total_pixels_; }

    // Access histogram data
    int red_count(int bin) const { return red_[bin]; }
    int green_count(int bin) const { return green_[bin]; }
    int blue_count(int bin) const { return blue_[bin]; }
    int luminance_count(int bin) const { return luminance_[bin]; }

    // Get full histogram arrays
    std::vector<int> red_histogram() const {
        return std::vector<int>(red_.begin(), red_.end());
    }

    std::vector<int> green_histogram() const {
        return std::vector<int>(green_.begin(), green_.end());
    }

    std::vector<int> blue_histogram() const {
        return std::vector<int>(blue_.begin(), blue_.end());
    }

    std::vector<int> luminance_histogram() const {
        return std::vector<int>(luminance_.begin(), luminance_.end());
    }

    // Statistical analysis
    double mean_luminance() const {
        if (total_pixels_ == 0) return 0.0;
        double sum = 0.0;
        for (int i = 0; i < 256; ++i) {
            sum += i * luminance_[i];
        }
        return sum / total_pixels_;
    }

    double std_dev_luminance() const {
        if (total_pixels_ == 0) return 0.0;
        double mean = mean_luminance();
        double variance_sum = 0.0;
        for (int i = 0; i < 256; ++i) {
            double diff = i - mean;
            variance_sum += diff * diff * luminance_[i];
        }
        return std::sqrt(variance_sum / total_pixels_);
    }

    // Find the bin with maximum count
    int mode_luminance() const {
        int max_count = 0;
        int mode = 0;
        for (int i = 0; i < 256; ++i) {
            if (luminance_[i] > max_count) {
                max_count = luminance_[i];
                mode = i;
            }
        }
        return mode;
    }

    // Find median luminance value
    int median_luminance() const {
        if (total_pixels_ == 0) return 0;
        int half = total_pixels_ / 2;
        int cumulative = 0;
        for (int i = 0; i < 256; ++i) {
            cumulative += luminance_[i];
            if (cumulative >= half) {
                return i;
            }
        }
        return 255;
    }

    // Dynamic range (difference between max and min populated bins)
    int dynamic_range() const {
        int min_bin = -1, max_bin = -1;
        for (int i = 0; i < 256; ++i) {
            if (luminance_[i] > 0) {
                if (min_bin < 0) min_bin = i;
                max_bin = i;
            }
        }
        if (min_bin < 0) return 0;
        return max_bin - min_bin;
    }

    // Percentage of pixels in shadows (bottom 25% of luminance)
    double shadow_percentage() const {
        if (total_pixels_ == 0) return 0.0;
        int shadow_pixels = 0;
        for (int i = 0; i < 64; ++i) {
            shadow_pixels += luminance_[i];
        }
        return 100.0 * shadow_pixels / total_pixels_;
    }

    // Percentage of pixels in highlights (top 25% of luminance)
    double highlight_percentage() const {
        if (total_pixels_ == 0) return 0.0;
        int highlight_pixels = 0;
        for (int i = 192; i < 256; ++i) {
            highlight_pixels += luminance_[i];
        }
        return 100.0 * highlight_pixels / total_pixels_;
    }

    // Percentage of pixels in midtones
    double midtone_percentage() const {
        return 100.0 - shadow_percentage() - highlight_percentage();
    }

    // Compute percentile value (0-100)
    int percentile(double p) const {
        if (total_pixels_ == 0) return 0;
        p = std::clamp(p, 0.0, 100.0);
        int target = static_cast<int>((p / 100.0) * total_pixels_);

        int cumulative = 0;
        for (int i = 0; i < 256; ++i) {
            cumulative += luminance_[i];
            if (cumulative >= target) {
                return i;
            }
        }
        return 255;
    }

    // Check if image is predominantly dark
    bool is_low_key() const {
        return mean_luminance() < 85;  // Below 1/3 of range
    }

    // Check if image is predominantly bright
    bool is_high_key() const {
        return mean_luminance() > 170;  // Above 2/3 of range
    }

    // Check if image has good contrast
    bool has_good_contrast() const {
        return dynamic_range() > 200 && std_dev_luminance() > 50;
    }

    // Generate cumulative distribution function (for histogram equalization)
    std::vector<double> cdf() const {
        std::vector<double> result(256);
        int cumulative = 0;
        for (int i = 0; i < 256; ++i) {
            cumulative += luminance_[i];
            result[i] = static_cast<double>(cumulative) / total_pixels_;
        }
        return result;
    }
};


// Histogram-based image operations
class HistogramOps {
public:
    // Histogram equalization for contrast enhancement
    static Image equalize(const Image& src) {
        Histogram hist(src);
        auto cdf = hist.cdf();

        // Find minimum non-zero CDF value
        double cdf_min = 1.0;
        for (double v : cdf) {
            if (v > 0 && v < cdf_min) {
                cdf_min = v;
            }
        }

        // Build lookup table
        std::array<uint8_t, 256> lut;
        for (int i = 0; i < 256; ++i) {
            double normalized = (cdf[i] - cdf_min) / (1.0 - cdf_min);
            lut[i] = static_cast<uint8_t>(std::clamp(normalized * 255.0, 0.0, 255.0));
        }

        // Apply to image (using luminance-preserving approach)
        Image result(src.width(), src.height());
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                Color pixel = src.get_pixel_unchecked(x, y);

                // Convert to HSV, equalize V, convert back
                ColorHSV hsv = ColorHSV::from_rgb(pixel);
                int v_bin = static_cast<int>(hsv.v * 255.0);
                v_bin = std::clamp(v_bin, 0, 255);
                hsv.v = lut[v_bin] / 255.0;

                result.set_pixel_unchecked(x, y, hsv.to_rgb());
            }
        }

        return result;
    }

    // Stretch histogram to use full range (auto-levels)
    static Image auto_levels(const Image& src) {
        Histogram hist(src);

        // Find actual range (ignore outliers: 0.5% on each end)
        int low = hist.percentile(0.5);
        int high = hist.percentile(99.5);

        if (high <= low) {
            return src.clone();  // No adjustment needed
        }

        double range = high - low;

        // Build lookup table
        std::array<uint8_t, 256> lut;
        for (int i = 0; i < 256; ++i) {
            double normalized = (i - low) / range;
            lut[i] = static_cast<uint8_t>(std::clamp(normalized * 255.0, 0.0, 255.0));
        }

        // Apply to each channel
        Image result(src.width(), src.height());
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                Color pixel = src.get_pixel_unchecked(x, y);
                result.set_pixel_unchecked(x, y, Color{
                    lut[pixel.r],
                    lut[pixel.g],
                    lut[pixel.b],
                    pixel.a
                });
            }
        }

        return result;
    }

    // Adaptive histogram equalization (CLAHE-like, simplified)
    // Divides image into tiles and equalizes each separately
    static Image adaptive_equalize(const Image& src, int tile_size) {
        if (tile_size <= 0) {
            throw std::invalid_argument("Tile size must be positive");
        }

        Image result(src.width(), src.height());

        int tiles_x = (src.width() + tile_size - 1) / tile_size;
        int tiles_y = (src.height() + tile_size - 1) / tile_size;

        for (int ty = 0; ty < tiles_y; ++ty) {
            for (int tx = 0; tx < tiles_x; ++tx) {
                int start_x = tx * tile_size;
                int start_y = ty * tile_size;
                int end_x = std::min(start_x + tile_size, src.width());
                int end_y = std::min(start_y + tile_size, src.height());

                // Build local histogram
                std::array<int, 256> local_hist{};
                int local_count = 0;

                for (int y = start_y; y < end_y; ++y) {
                    for (int x = start_x; x < end_x; ++x) {
                        Color pixel = src.get_pixel_unchecked(x, y);
                        int lum = static_cast<int>(pixel.luminance() * 255.0);
                        local_hist[std::clamp(lum, 0, 255)]++;
                        local_count++;
                    }
                }

                // Build local CDF
                std::array<double, 256> local_cdf;
                int cumulative = 0;
                for (int i = 0; i < 256; ++i) {
                    cumulative += local_hist[i];
                    local_cdf[i] = static_cast<double>(cumulative) / local_count;
                }

                // Apply to tile
                for (int y = start_y; y < end_y; ++y) {
                    for (int x = start_x; x < end_x; ++x) {
                        Color pixel = src.get_pixel_unchecked(x, y);
                        ColorHSV hsv = ColorHSV::from_rgb(pixel);

                        int v_bin = static_cast<int>(hsv.v * 255.0);
                        v_bin = std::clamp(v_bin, 0, 255);
                        hsv.v = local_cdf[v_bin];

                        result.set_pixel_unchecked(x, y, hsv.to_rgb());
                    }
                }
            }
        }

        return result;
    }

    // Binary threshold
    static Image threshold(const Image& src, int threshold_value) {
        threshold_value = std::clamp(threshold_value, 0, 255);

        Image result(src.width(), src.height());
        for (int y = 0; y < src.height(); ++y) {
            for (int x = 0; x < src.width(); ++x) {
                Color pixel = src.get_pixel_unchecked(x, y);
                int lum = static_cast<int>(pixel.luminance() * 255.0);

                uint8_t value = (lum >= threshold_value) ? 255 : 0;
                result.set_pixel_unchecked(x, y, Color{value, value, value, pixel.a});
            }
        }

        return result;
    }

    // Otsu's method for automatic threshold selection
    static int otsu_threshold(const Histogram& hist) {
        int total = hist.total_pixels();
        if (total == 0) return 128;

        auto lum = hist.luminance_histogram();

        double sum_total = 0;
        for (int i = 0; i < 256; ++i) {
            sum_total += i * lum[i];
        }

        double sum_background = 0;
        int weight_background = 0;

        double max_variance = 0;
        int best_threshold = 0;

        for (int t = 0; t < 256; ++t) {
            weight_background += lum[t];
            if (weight_background == 0) continue;

            int weight_foreground = total - weight_background;
            if (weight_foreground == 0) break;

            sum_background += t * lum[t];

            double mean_background = sum_background / weight_background;
            double mean_foreground = (sum_total - sum_background) / weight_foreground;

            // Between-class variance
            double variance = static_cast<double>(weight_background) * weight_foreground *
                             (mean_background - mean_foreground) * (mean_background - mean_foreground);

            if (variance > max_variance) {
                max_variance = variance;
                best_threshold = t;
            }
        }

        return best_threshold;
    }
};

} // namespace imgproc
