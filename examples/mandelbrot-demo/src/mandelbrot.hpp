#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>

struct Mandelbrot {
    int width = 800;
    int height = 600;
    int max_iter = 256;

    // Viewport (default: classic Mandelbrot view)
    double x_min = -2.5;
    double x_max = 1.0;
    double y_min = -1.25;
    double y_max = 1.25;

    Mandelbrot() = default;

    Mandelbrot(int w, int h, int iter = 256)
        : width(w), height(h), max_iter(iter) {}

    // Set viewport for zooming
    void set_viewport(double xmin, double xmax, double ymin, double ymax) {
        x_min = xmin;
        x_max = xmax;
        y_min = ymin;
        y_max = ymax;
    }

    // Compute escape time for a single point (returns smooth value for coloring)
    double escape_time(double real, double imag) const {
        double zr = 0.0, zi = 0.0;
        int iter = 0;

        while (zr * zr + zi * zi <= 4.0 && iter < max_iter) {
            double temp = zr * zr - zi * zi + real;
            zi = 2.0 * zr * zi + imag;
            zr = temp;
            iter++;
        }

        if (iter == max_iter) {
            return static_cast<double>(max_iter);
        }

        // Smooth coloring using continuous escape time
        double log_zn = std::log(zr * zr + zi * zi) / 2.0;
        double nu = std::log(log_zn / std::log(2.0)) / std::log(2.0);
        return static_cast<double>(iter) + 1.0 - nu;
    }

private:
    // Attempt exponential interpolation for smoother color bands
    static double smoothstep(double edge0, double edge1, double x) {
        double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }

    // Convert iteration count to RGB color - dramatic electric/fire palette
    static void iter_to_rgb_impl(double iter, int max_iter, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (iter >= max_iter) {
            r = g = b = 0;  // Black for points in the set
            return;
        }

        // Normalize with logarithmic scaling for more detail in lower iterations
        double t = iter / static_cast<double>(max_iter);

        // Apply slight log curve for better color distribution
        t = std::pow(t, 0.7);

        // Electric plasma palette: black → deep purple → electric blue → cyan → gold → white
        double rd, gd, bd;

        if (t < 0.16) {
            // Black to deep purple
            double s = t / 0.16;
            rd = s * 0.3;
            gd = 0.0;
            bd = s * 0.5;
        } else if (t < 0.42) {
            // Deep purple to electric blue
            double s = (t - 0.16) / 0.26;
            rd = 0.3 - s * 0.3;
            gd = s * 0.4;
            bd = 0.5 + s * 0.5;
        } else if (t < 0.6425) {
            // Electric blue to cyan
            double s = (t - 0.42) / 0.2225;
            rd = 0.0;
            gd = 0.4 + s * 0.6;
            bd = 1.0;
        } else if (t < 0.8575) {
            // Cyan to gold/orange
            double s = (t - 0.6425) / 0.215;
            rd = s;
            gd = 1.0 - s * 0.2;
            bd = 1.0 - s;
        } else {
            // Gold to white
            double s = (t - 0.8575) / 0.1425;
            rd = 1.0;
            gd = 0.8 + s * 0.2;
            bd = s;
        }

        r = static_cast<uint8_t>(std::clamp(rd * 255.0, 0.0, 255.0));
        g = static_cast<uint8_t>(std::clamp(gd * 255.0, 0.0, 255.0));
        b = static_cast<uint8_t>(std::clamp(bd * 255.0, 0.0, 255.0));
    }

public:

    // Render to RGB pixel data (row-major, RGB triplets)
    std::vector<uint8_t> render_rgb() const {
        std::vector<uint8_t> pixels(width * height * 3);

        double x_scale = (x_max - x_min) / width;
        double y_scale = (y_max - y_min) / height;

        for (int py = 0; py < height; py++) {
            double imag = y_min + py * y_scale;
            for (int px = 0; px < width; px++) {
                double real = x_min + px * x_scale;
                double iter = escape_time(real, imag);

                size_t idx = (py * width + px) * 3;
                iter_to_rgb_impl(iter, max_iter, pixels[idx], pixels[idx + 1], pixels[idx + 2]);
            }
        }

        return pixels;
    }

    // Render to PPM format string (can be saved directly as .ppm file)
    std::string to_ppm() const {
        auto pixels = render_rgb();

        std::string ppm = "P3\n" + std::to_string(width) + " " + std::to_string(height) + "\n255\n";

        for (int i = 0; i < width * height; i++) {
            ppm += std::to_string(pixels[i * 3]) + " ";
            ppm += std::to_string(pixels[i * 3 + 1]) + " ";
            ppm += std::to_string(pixels[i * 3 + 2]);
            ppm += (i % width == width - 1) ? "\n" : " ";
        }

        return ppm;
    }

    // Render ASCII art version (for terminal display)
    std::string to_ascii(int term_width = 80, int term_height = 40) const {
        const char* chars = " .:-=+*#%@";
        int num_chars = 10;

        std::string result;

        double x_scale = (x_max - x_min) / term_width;
        double y_scale = (y_max - y_min) / term_height;

        for (int py = 0; py < term_height; py++) {
            double imag = y_min + py * y_scale;
            for (int px = 0; px < term_width; px++) {
                double real = x_min + px * x_scale;
                double iter = escape_time(real, imag);

                int char_idx;
                if (iter >= max_iter) {
                    char_idx = num_chars - 1;  // Darkest for set interior
                } else {
                    char_idx = static_cast<int>((iter / max_iter) * (num_chars - 1));
                }
                result += chars[char_idx];
            }
            result += '\n';
        }

        return result;
    }
};
