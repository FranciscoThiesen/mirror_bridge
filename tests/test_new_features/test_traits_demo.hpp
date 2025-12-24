#pragma once

// ============================================================================
// Demo: In-Class Markers for Auto-Discovery Customization
// ============================================================================
//
// This demonstrates how users can control binding behavior WITHOUT writing
// any binding code. Just add markers to your class, and bind_class<T>
// respects them automatically through reflection.
//
// ============================================================================

#include <string>
#include <vector>
#include <cstdint>
#include <tuple>

// ============================================================================
// Example 1: Class with IN-CLASS markers (preferred for co-location)
// ============================================================================

struct UserProfile {
    // === BINDING CONFIGURATION (in-class markers) ===
    // These static constexpr tuples tell Mirror Bridge what to exclude/protect
    static constexpr auto _mirror_bridge_exclude = std::make_tuple(
        "password_hash",    // Never expose password hash to Python
        "internal_id"       // Internal database ID, not for scripting
    );

    static constexpr auto _mirror_bridge_readonly = std::make_tuple(
        "created_at",       // Creation time shouldn't be modified
        "user_id"           // User ID is immutable after creation
    );

    // === PUBLIC DATA ===
    int user_id = 0;
    std::string username = "";
    std::string email = "";
    int age = 0;

    // === INTERNAL DATA (will be excluded) ===
    std::string password_hash = "";  // EXCLUDED - security sensitive
    int internal_id = 0;             // EXCLUDED - implementation detail

    // === READONLY DATA ===
    std::string created_at = "2024-01-01";  // READONLY

    // === METHODS ===
    void set_email(const std::string& e) { email = e; }
    std::string get_email() const { return email; }

    bool verify_password(const std::string& input) const {
        // In real code: compare hash
        return input == password_hash;
    }
};

// ============================================================================
// Example 2: Class with EXTERNAL traits (when you can't modify the class)
// ============================================================================

// Imagine this is from a third-party library you can't modify
struct ThirdPartyVector3 {
    float x, y, z;
    void* _reserved;  // Internal pointer we want to hide
    int _flags;       // Internal flags

    float length() const { return std::sqrt(x*x + y*y + z*z); }
};

// ============================================================================
// Example 3: Buffer-heavy class for zero-copy demonstration
// ============================================================================

struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;  // Large buffer - benefits from zero-copy

    // Internal buffer management - exclude from binding
    void* gpu_handle = nullptr;
    static constexpr auto _mirror_bridge_exclude = std::make_tuple("gpu_handle");

    ImageData() = default;
    ImageData(int w, int h) : width(w), height(h), pixels(w * h * 3) {
        // Fill with gradient for testing
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t idx = (y * width + x) * 3;
                pixels[idx + 0] = static_cast<uint8_t>(x * 255 / width);   // R
                pixels[idx + 1] = static_cast<uint8_t>(y * 255 / height);  // G
                pixels[idx + 2] = 128;                                      // B
            }
        }
    }

    // Returns pixels - will use optimized transfer
    std::vector<uint8_t> get_pixels() const { return pixels; }

    // Returns pixel count
    size_t pixel_count() const { return width * height; }
    size_t byte_count() const { return pixels.size(); }
};

#include <cmath>
