// Test: P3394 Annotation Type Checking
//
// Uses annotation_of_type<T>() to check for specific annotation types

#include <meta>
#include <iostream>
#include <string>

// ============================================================================
// Annotation Types
// ============================================================================

namespace mirror_bridge {
    struct exclude {
        constexpr bool operator==(const exclude&) const = default;
    };
    struct readonly {
        constexpr bool operator==(const readonly&) const = default;
    };
}

// Import for clean syntax
using mirror_bridge::exclude;
using mirror_bridge::readonly;

// ============================================================================
// Example Struct
// ============================================================================

struct UserProfile {
    int user_id;
    std::string username;

    [[=exclude{}]] std::string password_hash;
    [[=exclude{}]] int internal_id;

    [[=readonly{}]] std::string created_at;
    [[=readonly{}]] int version;
};

// ============================================================================
// Reflection Helpers
// ============================================================================

template<typename T>
consteval std::size_t member_count() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()).size();
}

template<typename T, std::size_t Index>
consteval auto get_member() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())[Index];
}

template<typename T, std::size_t Index>
consteval const char* get_member_name() {
    return std::meta::identifier_of(get_member<T, Index>()).data();
}

// ============================================================================
// Annotation Type Checking - Using annotation_of_type<T>
// ============================================================================

template<typename T, std::size_t Index>
consteval bool is_excluded() {
    constexpr auto member = get_member<T, Index>();
    // annotation_of_type<T>(member) returns optional<T> if annotation exists
    return std::meta::annotation_of_type<mirror_bridge::exclude>(member).has_value();
}

template<typename T, std::size_t Index>
consteval bool is_readonly() {
    constexpr auto member = get_member<T, Index>();
    // annotation_of_type<T>(member) returns optional<T> if annotation exists
    return std::meta::annotation_of_type<mirror_bridge::readonly>(member).has_value();
}

// ============================================================================
// Print Info
// ============================================================================

template<typename T, std::size_t... Is>
void print_members_impl(std::index_sequence<Is...>) {
    ((std::cout << "  " << get_member_name<T, Is>()
                << (is_excluded<T, Is>() ? " [EXCLUDE]" : "")
                << (is_readonly<T, Is>() ? " [READONLY]" : "")
                << "\n"), ...);
}

template<typename T>
void print_binding_info(const char* name) {
    std::cout << name << ":\n";
    print_members_impl<T>(std::make_index_sequence<member_count<T>()>{});
}

// Count visible members
template<typename T, std::size_t... Is>
consteval std::size_t count_visible_impl(std::index_sequence<Is...>) {
    return ((is_excluded<T, Is>() ? 0 : 1) + ...);
}

template<typename T>
consteval std::size_t count_visible() {
    return count_visible_impl<T>(std::make_index_sequence<member_count<T>()>{});
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== P3394 Annotation Type Checking ===\n\n";

    print_binding_info<UserProfile>("UserProfile");

    std::cout << "\n  Total members: " << member_count<UserProfile>() << "\n";
    std::cout << "  Visible (not excluded): " << count_visible<UserProfile>() << "\n";

    std::cout << "\n=== Syntax ===\n\n";
    std::cout << "using mirror_bridge::exclude;\n";
    std::cout << "using mirror_bridge::readonly;\n\n";
    std::cout << "struct UserProfile {\n";
    std::cout << "    int user_id;                           // Bound normally\n";
    std::cout << "    [[=exclude{}]] std::string password_hash;  // Excluded\n";
    std::cout << "    [[=readonly{}]] std::string created_at;    // Read-only\n";
    std::cout << "};\n";

    return 0;
}
