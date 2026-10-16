// Test: P3394 Annotation Type Checking
//
// Checks for specific annotation types on specific members.
//
// The lookup goes through mirror_bridge::annotations::has_annotation rather
// than straight to the standard function, because the two implementations
// spell it differently -- std::meta::annotation_of_type<T>(info) in libc++,
// std::meta::annotations_of_with_type(info, info) in libstdc++ -- and a test
// written against one spelling is a test that can only run on one of the two
// compilers this project supports. That is how annotations came to be silently
// ignored on GCC for as long as they were.

#include "../../python/mirror_bridge_annotations.hpp"

#include <iostream>
#include <string>

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
    return mirror_bridge::annotations::has_annotation<mirror_bridge::exclude>(
        get_member<T, Index>());
}

template<typename T, std::size_t Index>
consteval bool is_readonly() {
    return mirror_bridge::annotations::has_annotation<mirror_bridge::readonly>(
        get_member<T, Index>());
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

    static_assert(count_visible<UserProfile>() == 4,
                  "two of the six members are excluded");
    static_assert(is_excluded<UserProfile, 2>() && is_excluded<UserProfile, 3>());
    static_assert(is_readonly<UserProfile, 4>() && is_readonly<UserProfile, 5>());
    static_assert(!is_excluded<UserProfile, 0>() && !is_readonly<UserProfile, 0>());

    return 0;
}
