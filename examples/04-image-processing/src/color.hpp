#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <string>

namespace imgproc {

// RGBA color with 8-bit channels
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    // Luminance using Rec. 709 coefficients (HDTV standard)
    double luminance() const {
        return 0.2126 * (r / 255.0) + 0.7152 * (g / 255.0) + 0.0722 * (b / 255.0);
    }

    // Convert to grayscale (preserving alpha)
    Color to_grayscale() const {
        uint8_t gray = static_cast<uint8_t>(luminance() * 255.0);
        return Color{gray, gray, gray, a};
    }

    // Linear interpolation between two colors
    Color lerp(Color other, double t) const {
        t = std::clamp(t, 0.0, 1.0);
        return Color{
            static_cast<uint8_t>(r + (other.r - r) * t),
            static_cast<uint8_t>(g + (other.g - g) * t),
            static_cast<uint8_t>(b + (other.b - b) * t),
            static_cast<uint8_t>(a + (other.a - a) * t)
        };
    }

    // Blend this color over a background (alpha compositing)
    Color blend_over(Color background) const {
        double src_alpha = a / 255.0;
        double dst_alpha = background.a / 255.0;
        double out_alpha = src_alpha + dst_alpha * (1.0 - src_alpha);

        if (out_alpha < 1e-6) {
            return Color{0, 0, 0, 0};
        }

        auto blend_channel = [&](uint8_t src, uint8_t dst) -> uint8_t {
            double result = (src * src_alpha + dst * dst_alpha * (1.0 - src_alpha)) / out_alpha;
            return static_cast<uint8_t>(std::clamp(result, 0.0, 255.0));
        };

        return Color{
            blend_channel(r, background.r),
            blend_channel(g, background.g),
            blend_channel(b, background.b),
            static_cast<uint8_t>(out_alpha * 255.0)
        };
    }

    // Adjust brightness (-1.0 to 1.0)
    Color adjust_brightness(double factor) const {
        factor = std::clamp(factor, -1.0, 1.0);
        int adjustment = static_cast<int>(factor * 255);
        return Color{
            static_cast<uint8_t>(std::clamp(r + adjustment, 0, 255)),
            static_cast<uint8_t>(std::clamp(g + adjustment, 0, 255)),
            static_cast<uint8_t>(std::clamp(b + adjustment, 0, 255)),
            a
        };
    }

    // Adjust contrast (0.0 = gray, 1.0 = unchanged, 2.0 = doubled)
    Color adjust_contrast(double factor) const {
        auto adjust = [factor](uint8_t channel) -> uint8_t {
            double normalized = (channel / 255.0 - 0.5) * factor + 0.5;
            return static_cast<uint8_t>(std::clamp(normalized * 255.0, 0.0, 255.0));
        };
        return Color{adjust(r), adjust(g), adjust(b), a};
    }

    // Invert colors
    Color invert() const {
        return Color{
            static_cast<uint8_t>(255 - r),
            static_cast<uint8_t>(255 - g),
            static_cast<uint8_t>(255 - b),
            a
        };
    }

    // Pack to 32-bit integer (RGBA order)
    uint32_t to_uint32() const {
        return (static_cast<uint32_t>(r) << 24) |
               (static_cast<uint32_t>(g) << 16) |
               (static_cast<uint32_t>(b) << 8) |
               static_cast<uint32_t>(a);
    }

    // Euclidean distance in RGB space (ignoring alpha)
    double distance(Color other) const {
        int dr = static_cast<int>(r) - static_cast<int>(other.r);
        int dg = static_cast<int>(g) - static_cast<int>(other.g);
        int db = static_cast<int>(b) - static_cast<int>(other.b);
        return std::sqrt(dr * dr + dg * dg + db * db);
    }

    std::string to_hex() const {
        char buf[10];
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", r, g, b, a);
        return std::string(buf);
    }
};

// HSV color for color manipulation workflows
struct ColorHSV {
    double h = 0.0;  // Hue: 0-360 degrees
    double s = 0.0;  // Saturation: 0-1
    double v = 0.0;  // Value: 0-1
    double a = 1.0;  // Alpha: 0-1

    // Convert from RGB
    static ColorHSV from_rgb(Color rgb) {
        double r = rgb.r / 255.0;
        double g = rgb.g / 255.0;
        double b = rgb.b / 255.0;

        double max_val = std::max({r, g, b});
        double min_val = std::min({r, g, b});
        double delta = max_val - min_val;

        ColorHSV hsv;
        hsv.v = max_val;
        hsv.a = rgb.a / 255.0;

        if (delta < 1e-6) {
            hsv.h = 0.0;
            hsv.s = 0.0;
        } else {
            hsv.s = delta / max_val;

            if (max_val == r) {
                hsv.h = 60.0 * std::fmod((g - b) / delta, 6.0);
            } else if (max_val == g) {
                hsv.h = 60.0 * ((b - r) / delta + 2.0);
            } else {
                hsv.h = 60.0 * ((r - g) / delta + 4.0);
            }

            if (hsv.h < 0) hsv.h += 360.0;
        }

        return hsv;
    }

    // Convert to RGB
    Color to_rgb() const {
        double c = v * s;
        double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
        double m = v - c;

        double r_prime, g_prime, b_prime;

        if (h < 60) {
            r_prime = c; g_prime = x; b_prime = 0;
        } else if (h < 120) {
            r_prime = x; g_prime = c; b_prime = 0;
        } else if (h < 180) {
            r_prime = 0; g_prime = c; b_prime = x;
        } else if (h < 240) {
            r_prime = 0; g_prime = x; b_prime = c;
        } else if (h < 300) {
            r_prime = x; g_prime = 0; b_prime = c;
        } else {
            r_prime = c; g_prime = 0; b_prime = x;
        }

        return Color{
            static_cast<uint8_t>((r_prime + m) * 255.0),
            static_cast<uint8_t>((g_prime + m) * 255.0),
            static_cast<uint8_t>((b_prime + m) * 255.0),
            static_cast<uint8_t>(a * 255.0)
        };
    }

    // Rotate hue by degrees
    void rotate_hue(double degrees) {
        h = std::fmod(h + degrees, 360.0);
        if (h < 0) h += 360.0;
    }
};

} // namespace imgproc
