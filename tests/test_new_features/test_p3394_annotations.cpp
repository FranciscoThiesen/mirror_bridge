// Test: P3394 Annotations for Reflection
//
// P3394 was accepted into C++26 at the Sofia meeting (June 2025).
// Bloomberg's clang-p2996 supports it with -freflection-latest
//
// This enables CLEAN field annotation syntax:
//   [[=exclude{}]] std::string password_hash;
//   [[=readonly{}]] int creation_time;

#include <meta>
#include <iostream>
#include <string>

// ============================================================================
// Annotation Types
// ============================================================================

struct exclude {};
struct readonly {};

// ============================================================================
// Example Structs with Annotations
// ============================================================================

struct UserProfile {
    int user_id;
    std::string username;
    std::string email;

    [[=exclude{}]] std::string password_hash;
    [[=exclude{}]] int internal_id;

    [[=readonly{}]] std::string created_at;
};

struct GameEntity {
    float x, y, z;
    std::string name;
    int health;

    [[=exclude{}]] void* render_handle;
    [[=exclude{}]] int internal_flags;

    [[=readonly{}]] int entity_id;
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
// Annotation Checking
// ============================================================================

template<typename T, std::size_t Index>
consteval std::size_t annotation_count() {
    return std::meta::annotations_of(get_member<T, Index>()).size();
}

// For now, check if ANY annotation exists
// (Full type checking requires more complex syntax)
template<typename T, std::size_t Index>
consteval bool has_any_annotation() {
    return annotation_count<T, Index>() > 0;
}

// ============================================================================
// Print Info
// ============================================================================

template<typename T, std::size_t... Is>
void print_members_impl(std::index_sequence<Is...>) {
    ((std::cout << "  " << get_member_name<T, Is>()
                << " (annotations: " << annotation_count<T, Is>() << ")"
                << (has_any_annotation<T, Is>() ? " [ANNOTATED]" : "")
                << "\n"), ...);
}

template<typename T>
void print_binding_info(const char* name) {
    std::cout << name << ":\n";
    print_members_impl<T>(std::make_index_sequence<member_count<T>()>{});
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== P3394 Annotations for Reflection ===\n\n";

    print_binding_info<UserProfile>("UserProfile");
    std::cout << "\n";

    print_binding_info<GameEntity>("GameEntity");
    std::cout << "\n";

    std::cout << "=== The Clean Syntax Works! ===\n\n";
    std::cout << "struct UserProfile {\n";
    std::cout << "    int user_id;                                  // No annotation\n";
    std::cout << "    [[=exclude{}]] std::string password_hash;     // 1 annotation\n";
    std::cout << "    [[=readonly{}]] std::string created_at;       // 1 annotation\n";
    std::cout << "};\n\n";

    std::cout << "P3394 is accepted in C++26 and supported by clang-p2996!\n";
    std::cout << "Next step: integrate annotation type checking into bind_class.\n";

    return 0;
}
