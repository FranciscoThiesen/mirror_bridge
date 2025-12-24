// Test: Field Annotations for Binding Customization
//
// This demonstrates the improved annotation syntax where fields are
// annotated directly - no separate tuple of strings needed!
//
// Compile: clang++ -std=c++20 -I. tests/test_new_features/test_annotations.cpp -o test_annotations

#include "python/mirror_bridge_annotations.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cassert>

using namespace mirror_bridge;

// ============================================================================
// Example: User Profile with Annotated Fields
// ============================================================================
//
// BEFORE (old cumbersome way):
//   struct UserProfile {
//       static constexpr auto _mirror_bridge_exclude = std::make_tuple(
//           "password_hash", "internal_id"  // Error-prone strings!
//       );
//       std::string password_hash;
//       int internal_id;
//   };
//
// AFTER (new clean way):
//   struct UserProfile {
//       exclude<std::string> password_hash;  // Annotated directly!
//       exclude<int> internal_id;            // No strings needed!
//   };

struct UserProfile {
    // Public fields - normal binding
    int user_id = 0;
    std::string username = "guest";
    std::string email = "";

    // Excluded fields - won't appear in Python
    exclude<std::string> password_hash{""};
    exclude<int> internal_id{0};

    // Readonly fields - Python can read but not write
    readonly<std::string> created_at{"2024-01-01"};
    readonly<int> version{1};

    // Methods work normally
    bool verify_password(const std::string& input) const {
        return input == password_hash;  // Implicit conversion works!
    }

    void set_internal_id(int id) {
        internal_id = id;  // Assignment works in C++
    }
};

// ============================================================================
// Example: Game Entity with Internal State
// ============================================================================

struct GameEntity {
    // Public game state
    float x = 0.0f;
    float y = 0.0f;
    std::string name = "Entity";
    int health = 100;

    // Internal bookkeeping - excluded from scripting
    exclude<void*> render_handle{nullptr};
    exclude<int> internal_flags{0};
    exclude<std::vector<int>> cached_neighbors{};

    // Readonly game stats
    readonly<int> entity_id{0};
    readonly<int> spawn_tick{0};
};

// ============================================================================
// Example: Image with GPU Handle
// ============================================================================

struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;

    // GPU handle should never be exposed to Python
    exclude<void*> gpu_texture{nullptr};
    exclude<int> gpu_buffer_id{-1};

    // Metadata that shouldn't change
    readonly<std::string> format{"RGBA"};
    readonly<size_t> original_size{0};
};

// ============================================================================
// Compile-Time Tests
// ============================================================================

// Test detection of exclude<T>
static_assert(is_excluded_v<exclude<int>>, "exclude<int> should be detected");
static_assert(is_excluded_v<exclude<void*>>, "exclude<void*> should be detected");
static_assert(is_excluded_v<exclude<std::string>>, "exclude<std::string> should be detected");
static_assert(!is_excluded_v<int>, "int should not be detected as excluded");
static_assert(!is_excluded_v<readonly<int>>, "readonly<int> is not excluded");

// Test detection of readonly<T>
static_assert(is_readonly_field_v<readonly<int>>, "readonly<int> should be detected");
static_assert(is_readonly_field_v<readonly<std::string>>, "readonly<std::string> should be detected");
static_assert(!is_readonly_field_v<int>, "int should not be detected as readonly");
static_assert(!is_readonly_field_v<exclude<int>>, "exclude<int> is not readonly");

// Test unwrapping
static_assert(std::is_same_v<unwrap_annotation_t<exclude<int>>, int>,
    "unwrap exclude<int> should give int");
static_assert(std::is_same_v<unwrap_annotation_t<readonly<std::string>>, std::string>,
    "unwrap readonly<std::string> should give std::string");
static_assert(std::is_same_v<unwrap_annotation_t<int>, int>,
    "unwrap int should give int (no-op)");

// Test is_annotated_v
static_assert(is_annotated_v<exclude<int>>, "exclude<int> is annotated");
static_assert(is_annotated_v<readonly<int>>, "readonly<int> is annotated");
static_assert(!is_annotated_v<int>, "int is not annotated");

// ============================================================================
// Runtime Tests - Verify Transparent Behavior
// ============================================================================

void test_exclude_transparency() {
    std::cout << "Testing exclude<T> transparency...\n";

    // Test implicit conversion
    exclude<int> excluded_int{42};
    int regular_int = excluded_int;  // Implicit conversion
    assert(regular_int == 42);

    // Test assignment
    excluded_int = 100;
    assert(excluded_int.get() == 100);

    // Test with pointers
    int value = 123;
    exclude<int*> excluded_ptr{&value};
    int* regular_ptr = excluded_ptr;  // Implicit conversion
    assert(*regular_ptr == 123);
    assert(*excluded_ptr == 123);  // Dereference works

    // Test with strings
    exclude<std::string> excluded_str{"hello"};
    std::string regular_str = excluded_str;
    assert(regular_str == "hello");

    excluded_str = "world";
    assert(excluded_str.get() == "world");

    std::cout << "  ✓ All exclude<T> transparency tests passed\n";
}

void test_readonly_transparency() {
    std::cout << "Testing readonly<T> transparency...\n";

    // Test implicit conversion
    readonly<int> readonly_int{42};
    int regular_int = readonly_int;
    assert(regular_int == 42);

    // Test assignment (works in C++, would be blocked in Python)
    readonly_int = 100;
    assert(readonly_int.get() == 100);

    // Test with strings
    readonly<std::string> readonly_str{"immutable"};
    std::string regular_str = readonly_str;
    assert(regular_str == "immutable");

    std::cout << "  ✓ All readonly<T> transparency tests passed\n";
}

void test_real_world_usage() {
    std::cout << "Testing real-world usage patterns...\n";

    UserProfile profile;
    profile.user_id = 1;
    profile.username = "alice";
    profile.password_hash = "hashed_secret";  // Works like normal assignment
    profile.internal_id = 12345;

    // Verify password (uses implicit conversion)
    assert(profile.verify_password("hashed_secret"));
    assert(!profile.verify_password("wrong"));

    // Access readonly fields
    std::string created = profile.created_at;  // Implicit conversion
    assert(created == "2024-01-01");

    // Modify readonly in C++ (allowed)
    profile.version = 2;
    assert(profile.version.get() == 2);

    GameEntity entity;
    entity.render_handle = reinterpret_cast<void*>(0xDEADBEEF);
    void* handle = entity.render_handle;  // Get the handle back
    assert(handle == reinterpret_cast<void*>(0xDEADBEEF));

    entity.cached_neighbors.get().push_back(1);
    entity.cached_neighbors.get().push_back(2);
    assert(entity.cached_neighbors.get().size() == 2);

    std::cout << "  ✓ All real-world usage tests passed\n";
}

void print_summary() {
    std::cout << "\n=== Annotation Syntax Comparison ===\n\n";

    std::cout << "OLD (cumbersome, error-prone):\n";
    std::cout << "  struct MyClass {\n";
    std::cout << "      static constexpr auto _mirror_bridge_exclude = std::make_tuple(\n";
    std::cout << "          \"password\", \"internal_ptr\"  // Strings can have typos!\n";
    std::cout << "      );\n";
    std::cout << "      std::string password;\n";
    std::cout << "      void* internal_ptr;\n";
    std::cout << "  };\n\n";

    std::cout << "NEW (clean, type-safe, co-located):\n";
    std::cout << "  struct MyClass {\n";
    std::cout << "      exclude<std::string> password;      // Annotated directly!\n";
    std::cout << "      exclude<void*> internal_ptr;        // No strings needed!\n";
    std::cout << "      readonly<int> creation_time{now()}; // Read-only in Python\n";
    std::cout << "  };\n\n";

    std::cout << "Benefits:\n";
    std::cout << "  ✓ No string duplication (typo-proof)\n";
    std::cout << "  ✓ Annotation is co-located with the field\n";
    std::cout << "  ✓ Compile-time detection (no runtime overhead)\n";
    std::cout << "  ✓ Works transparently in C++ code\n";
    std::cout << "  ✓ Still just: bind_class<MyClass>(m, \"MyClass\");\n";
}

int main() {
    std::cout << "=== Field Annotation Tests ===\n\n";

    std::cout << "Compile-time static_asserts passed!\n\n";

    test_exclude_transparency();
    test_readonly_transparency();
    test_real_world_usage();

    print_summary();

    std::cout << "\n=== All Tests Passed! ===\n";
    return 0;
}
