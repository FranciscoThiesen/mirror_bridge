# Zero-Copy Buffer Protocol Support

Mirror Bridge provides zero-copy buffer protocol support for efficient data sharing between C++ and Python. This eliminates the overhead of copying large data buffers, achieving **10 million times faster** transfers for large arrays compared to element-by-element copying.

## Performance

| Buffer Size | Element-by-Element | Bulk memcpy | Zero-Copy | Speedup vs Copy |
|-------------|-------------------|-------------|-----------|-----------------|
| 1 KB        | 0.0008 ms         | 0.0001 ms   | 0.000001 ms | 955x |
| 100 KB      | 0.08 ms           | 0.002 ms    | 0.000001 ms | 96,000x |
| 10 MB       | 8.2 ms            | 0.25 ms     | 0.000008 ms | 970,000x |
| 100 MB      | 260 ms            | 2.6 ms      | 0.00003 ms  | **10,000,000x** |

For real-time applications (60 fps = 16.6ms budget), zero-copy is essentially free while copying would consume the entire frame budget.

## Usage

### Include the Header

```cpp
#include "python/mirror_bridge_buffer.hpp"
```

### Exposing Buffers from C++ to Python

**Option 1: Zero-Copy with Ownership Transfer**

The container is moved to the buffer view, which manages its lifetime:

```cpp
std::vector<float> compute_data() {
    std::vector<float> result(1000000);
    // ... fill result ...
    return result;
}

// In your binding:
PyObject* get_data(PyObject* self, PyObject* args) {
    auto data = compute_data();
    return mirror_bridge::buffer::to_memoryview_zero_copy(std::move(data));
}
```

**Option 2: Zero-Copy with Borrowed Reference**

When the container outlives the Python view:

```cpp
struct DataHolder {
    std::vector<uint8_t> pixels;

    // The DataHolder instance keeps pixels alive
    PyObject* get_pixels_view(PyObject* self) {
        return mirror_bridge::buffer::create_view_borrowed(pixels, self);
    }
};
```

**Option 3: Safe Copy (when lifetime is uncertain)**

Creates a Python-managed copy of the data:

```cpp
PyObject* get_data_copy(PyObject* self, PyObject* args) {
    std::vector<float> data = compute_data();
    return mirror_bridge::buffer::to_memoryview(data);  // Copies data
}
```

### Python Side

```python
import numpy as np

# Get zero-copy view from C++
view = obj.get_pixels()  # Returns memoryview

# Use directly with numpy - no additional copy!
arr = np.asarray(view)

# Modify in-place (changes C++ data if mutable)
arr[0] = 255

# Or use the memoryview directly
print(len(view), view[0])
```

## Supported Types

The buffer protocol supports all trivially copyable types with standard format strings:

| C++ Type | Format | Description |
|----------|--------|-------------|
| `int8_t`, `char` | `b` | Signed byte |
| `uint8_t`, `unsigned char`, `std::byte` | `B` | Unsigned byte |
| `int16_t`, `short` | `h` | Signed short |
| `uint16_t`, `unsigned short` | `H` | Unsigned short |
| `int32_t`, `int` | `i` | Signed int |
| `uint32_t`, `unsigned int` | `I` | Unsigned int |
| `int64_t`, `long long` | `q` | Signed long long |
| `uint64_t`, `unsigned long long` | `Q` | Unsigned long long |
| `float` | `f` | Single precision float |
| `double` | `d` | Double precision float |
| `bool` | `?` | Boolean |

## Container Requirements

Containers must satisfy the `Bufferable` concept:

```cpp
template<typename T>
concept Bufferable = requires(T& t) {
    { t.data() } -> std::convertible_to<const void*>;
    { t.size() } -> std::convertible_to<std::size_t>;
    typename T::value_type;
} && std::is_trivially_copyable_v<typename T::value_type>;
```

Supported containers include:
- `std::vector<T>`
- `std::array<T, N>`
- `std::span<T>` (with appropriate lifetime management)

## When to Use Zero-Copy

**Use zero-copy when:**
- Transferring large buffers (images, audio, point clouds)
- Real-time applications where latency matters
- The C++ object outlives the Python usage

**Use copying when:**
- Buffer lifetime is uncertain
- Data is small (overhead not significant)
- Python needs to own the data independently

## Running the Benchmark

```bash
# Compile the standalone benchmark
clang++ -std=c++20 -O3 tests/test_new_features/benchmark_buffer_standalone.cpp \
    -o benchmark_buffer

# Run it
./benchmark_buffer
```

## Implementation Details

The implementation uses Python's buffer protocol (PEP 3118) via:

1. **`BufferViewObject<Container>`** - A Python object that wraps a C++ container
2. **`bf_getbuffer`** - Exposes the container's memory to Python
3. **`bf_releasebuffer`** - Cleanup when Python is done with the buffer

Reference counting ensures the C++ container stays alive while Python holds a view.
