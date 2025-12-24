// Benchmark: Zero-Copy Buffer Views vs Element-by-Element Copy
//
// This demonstrates the performance difference between:
// 1. Element-by-element copy (naive approach, ~O(n) with per-element overhead)
// 2. Bulk memcpy (optimized copy)
// 3. Zero-copy pointer sharing (optimal - O(1))
//
// Run with: clang++ -std=c++20 -O3 benchmark_buffer_standalone.cpp -o benchmark_buffer && ./benchmark_buffer

#include <vector>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <numeric>

// Simulated Python list (to measure element-by-element overhead)
struct SimulatedPyList {
    std::vector<double> data;

    void append(double value) {
        data.push_back(value);
    }

    void reserve(size_t n) {
        data.reserve(n);
    }
};

// Simulated buffer view (zero-copy - just holds a pointer)
struct SimulatedBufferView {
    const uint8_t* data;
    size_t size;
};

// Timing helper
template<typename Func>
double time_ms(Func&& f, int iterations = 100) {
    // Warmup
    for (int i = 0; i < 3; ++i) f();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        f();
    }
    auto end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration<double, std::milli>(end - start).count() / iterations;
}

int main() {
    std::cout << "=== Zero-Copy Buffer View Performance Benchmark ===\n\n";

    // Test different buffer sizes
    std::vector<size_t> sizes = {
        1000,       // 1 KB
        10000,      // 10 KB
        100000,     // 100 KB
        1000000,    // 1 MB
        10000000,   // 10 MB
        100000000   // 100 MB
    };

    std::cout << std::setw(12) << "Size"
              << std::setw(18) << "Element-by-Elem"
              << std::setw(18) << "Bulk memcpy"
              << std::setw(18) << "Zero-Copy"
              << std::setw(15) << "Copy Speedup"
              << std::setw(15) << "Zero Speedup"
              << "\n";
    std::cout << std::string(96, '-') << "\n";

    for (size_t size : sizes) {
        // Source data
        std::vector<uint8_t> source(size);
        std::iota(source.begin(), source.end(), 0);  // Fill with 0,1,2,...

        double element_time = 0;
        double memcpy_time = 0;
        double zerocopy_time = 0;

        // Adjust iterations based on size
        int iterations = (size <= 100000) ? 100 : (size <= 1000000) ? 20 : 5;

        // Method 1: Element-by-element copy (simulates naive Python list building)
        element_time = time_ms([&]() {
            SimulatedPyList list;
            list.reserve(size);
            for (size_t i = 0; i < size; ++i) {
                list.append(source[i]);  // Per-element overhead
            }
            // Prevent optimization
            volatile auto x = list.data.back();
            (void)x;
        }, iterations);

        // Method 2: Bulk memcpy (optimized copy)
        memcpy_time = time_ms([&]() {
            std::vector<uint8_t> dest(size);
            std::memcpy(dest.data(), source.data(), size);
            // Prevent optimization
            volatile auto x = dest.back();
            (void)x;
        }, iterations);

        // Method 3: Zero-copy (just create a view)
        zerocopy_time = time_ms([&]() {
            SimulatedBufferView view{source.data(), size};
            // Simulate accessing the data (would be through buffer protocol)
            volatile auto x = view.data[0];
            (void)x;
        }, iterations);

        // Format size for display
        std::string size_str;
        if (size >= 1000000) {
            size_str = std::to_string(size / 1000000) + " MB";
        } else if (size >= 1000) {
            size_str = std::to_string(size / 1000) + " KB";
        } else {
            size_str = std::to_string(size) + " B";
        }

        std::cout << std::setw(12) << size_str
                  << std::setw(15) << std::fixed << std::setprecision(4) << element_time << " ms"
                  << std::setw(15) << std::fixed << std::setprecision(4) << memcpy_time << " ms"
                  << std::setw(15) << std::fixed << std::setprecision(6) << zerocopy_time << " ms"
                  << std::setw(12) << std::fixed << std::setprecision(1) << (element_time / memcpy_time) << "x"
                  << std::setw(12) << std::fixed << std::setprecision(0) << (element_time / zerocopy_time) << "x"
                  << "\n";
    }

    std::cout << "\n";
    std::cout << "Key Insights:\n";
    std::cout << "- Element-by-element: Creates Python objects for each element (slow)\n";
    std::cout << "- Bulk memcpy: Single memory copy, fast but still O(n)\n";
    std::cout << "- Zero-copy: Returns pointer view via buffer protocol, O(1) constant time\n\n";

    std::cout << "For a 10 MB buffer in a real-time application (60 fps = 16.6ms budget):\n";
    std::cout << "- Element-by-element would consume entire frame budget\n";
    std::cout << "- memcpy adds latency proportional to size\n";
    std::cout << "- Zero-copy is essentially free (< 0.001ms)\n\n";

    std::cout << "=== Python Buffer Protocol Integration ===\n\n";
    std::cout << "In Mirror Bridge, users get zero-copy by returning const& from methods:\n\n";
    std::cout << "    struct ImageData {\n";
    std::cout << "        std::vector<uint8_t> pixels;\n";
    std::cout << "\n";
    std::cout << "        // Returns copy - uses optimized memcpy\n";
    std::cout << "        std::vector<uint8_t> get_pixels_copy() const { return pixels; }\n";
    std::cout << "\n";
    std::cout << "        // Returns reference - can use zero-copy buffer protocol\n";
    std::cout << "        const std::vector<uint8_t>& get_pixels_view() const { return pixels; }\n";
    std::cout << "    };\n\n";

    std::cout << "Python usage:\n";
    std::cout << "    img = ImageData(1920, 1080)  # 6MB buffer\n";
    std::cout << "    arr = np.array(img.get_pixels_view(), copy=False)  # Zero-copy!\n";
    std::cout << "    # arr shares memory with C++ - no copy made\n";

    return 0;
}
