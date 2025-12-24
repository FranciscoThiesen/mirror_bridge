// Test: Can we use C++ attributes for field annotations?
//
// Goal: Natural syntax like:
//   struct MyClass {
//       [[exclude]] void* internal_ptr;
//       [[readonly]] int creation_time;
//   };

#include <meta>
#include <iostream>
#include <string_view>

// Define attribute macros using clang::annotate
#define EXCLUDE [[clang::annotate("mirror_bridge::exclude")]]
#define READONLY [[clang::annotate("mirror_bridge::readonly")]]

struct TestClass {
    int public_data;
    EXCLUDE void* internal_ptr;
    READONLY int creation_time;
};

// Helper to get member count
template<typename T>
consteval std::size_t member_count() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()).size();
}

// Helper to get member at index
template<typename T, std::size_t Index>
consteval auto get_member() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())[Index];
}

// Check if P3394 annotations_of is available
template<typename T, std::size_t Index>
consteval bool has_annotations() {
    constexpr auto member = get_member<T, Index>();
    // Try to call annotations_of if it exists
#if __has_feature(reflection_annotations)
    auto annots = std::meta::annotations_of(member);
    return annots.size() > 0;
#else
    return false;  // Feature not available
#endif
}

// Print member info at runtime
template<typename T, std::size_t... Is>
void print_members_impl(std::index_sequence<Is...>) {
    ((std::cout << "  [" << Is << "] "
                << std::meta::identifier_of(get_member<T, Is>())
                << " : " << std::meta::display_string_of(std::meta::type_of(get_member<T, Is>()))
                << "\n"), ...);
}

template<typename T>
void print_members() {
    print_members_impl<T>(std::make_index_sequence<member_count<T>()>{});
}

int main() {
    std::cout << "=== Testing Attribute-Based Annotations ===\n\n";

    std::cout << "TestClass has " << member_count<TestClass>() << " members:\n";
    print_members<TestClass>();

    std::cout << "\nChecking for P3394 annotation support:\n";
#if __has_feature(reflection_annotations)
    std::cout << "  ✓ reflection_annotations feature is available!\n";
#else
    std::cout << "  ✗ reflection_annotations feature not available\n";
    std::cout << "    (P3394 not yet implemented in this compiler)\n";
#endif

    std::cout << "\nAlternative: Using macro + naming convention...\n";

    return 0;
}
