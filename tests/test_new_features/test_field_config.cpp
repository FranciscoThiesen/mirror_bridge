// Test: Non-Intrusive Field Configuration
//
// This approach lets you configure binding behavior WITHOUT modifying
// your struct definitions. No wrapper types, no strings to typo.
//
// Compile: clang++ -std=c++20 -I. tests/test_new_features/test_field_config.cpp

#include "python/mirror_bridge_field_config.hpp"
#include <iostream>
#include <cassert>
#include <string>

// ============================================================================
// Your struct stays COMPLETELY UNCHANGED - natural C++ types
// ============================================================================

struct UserProfile {
    int user_id;
    std::string username;
    std::string email;
    std::string password_hash;  // Want to exclude
    int internal_id;            // Want to exclude
    std::string created_at;     // Want readonly
    int version;                // Want readonly
};

struct GameEntity {
    float x, y, z;
    std::string name;
    int health;
    void* render_handle;        // Want to exclude
    int internal_flags;         // Want to exclude
    int entity_id;              // Want readonly
};

// ============================================================================
// Configure AFTER the struct - type-safe, no strings!
// ============================================================================

// Single-field macros
MIRROR_EXCLUDE(UserProfile, password_hash);
// Note: For multiple fields, we'd need MIRROR_EXCLUDE_FIELDS

// For GameEntity, let's test the offset-based approach directly
template<>
struct mirror_bridge::excluded_fields<GameEntity> {
    static constexpr bool has_config = true;
    static constexpr std::size_t excluded_offsets[] = {
        offsetof(GameEntity, render_handle),
        offsetof(GameEntity, internal_flags)
    };
    static constexpr std::size_t count = 2;

    static constexpr bool is_excluded_offset(std::size_t off) {
        for (std::size_t i = 0; i < count; ++i)
            if (excluded_offsets[i] == off) return true;
        return false;
    }
};

template<>
struct mirror_bridge::readonly_fields<GameEntity> {
    static constexpr bool has_config = true;
    static constexpr std::size_t readonly_offsets[] = {
        offsetof(GameEntity, entity_id)
    };
    static constexpr std::size_t count = 1;

    static constexpr bool is_readonly_offset(std::size_t off) {
        for (std::size_t i = 0; i < count; ++i)
            if (readonly_offsets[i] == off) return true;
        return false;
    }
};

// ============================================================================
// Tests
// ============================================================================

void test_userprofile_config() {
    std::cout << "Testing UserProfile configuration...\n";

    using Config = mirror_bridge::excluded_fields<UserProfile>;

    // password_hash should be excluded
    assert(Config::is_excluded_offset(offsetof(UserProfile, password_hash)));

    // Other fields should NOT be excluded
    assert(!Config::is_excluded_offset(offsetof(UserProfile, user_id)));
    assert(!Config::is_excluded_offset(offsetof(UserProfile, username)));

    std::cout << "  ✓ UserProfile exclusion works\n";
}

void test_gameentity_config() {
    std::cout << "Testing GameEntity configuration...\n";

    using ExConfig = mirror_bridge::excluded_fields<GameEntity>;
    using RoConfig = mirror_bridge::readonly_fields<GameEntity>;

    // render_handle and internal_flags should be excluded
    assert(ExConfig::is_excluded_offset(offsetof(GameEntity, render_handle)));
    assert(ExConfig::is_excluded_offset(offsetof(GameEntity, internal_flags)));

    // Other fields should NOT be excluded
    assert(!ExConfig::is_excluded_offset(offsetof(GameEntity, x)));
    assert(!ExConfig::is_excluded_offset(offsetof(GameEntity, name)));
    assert(!ExConfig::is_excluded_offset(offsetof(GameEntity, entity_id)));

    // entity_id should be readonly
    assert(RoConfig::is_readonly_offset(offsetof(GameEntity, entity_id)));

    // Other fields should NOT be readonly
    assert(!RoConfig::is_readonly_offset(offsetof(GameEntity, x)));
    assert(!RoConfig::is_readonly_offset(offsetof(GameEntity, health)));

    std::cout << "  ✓ GameEntity exclusion works\n";
    std::cout << "  ✓ GameEntity readonly works\n";
}

void test_no_config() {
    std::cout << "Testing struct with no configuration...\n";

    struct PlainStruct {
        int a, b, c;
    };

    using ExConfig = mirror_bridge::excluded_fields<PlainStruct>;
    using RoConfig = mirror_bridge::readonly_fields<PlainStruct>;

    // Should have no exclusions
    assert(!ExConfig::has_config);
    assert(!RoConfig::has_config);

    std::cout << "  ✓ Unconfigured structs work correctly\n";
}

void print_syntax_comparison() {
    std::cout << "\n=== Syntax Comparison ===\n\n";

    std::cout << "APPROACH 1 - Wrapper types (intrusive, changes types):\n";
    std::cout << "  struct MyClass {\n";
    std::cout << "      exclude<void*> internal_ptr;  // Type is now exclude<void*>\n";
    std::cout << "      readonly<int> creation_time;  // Type is now readonly<int>\n";
    std::cout << "  };\n\n";

    std::cout << "APPROACH 2 - Field configuration (non-intrusive!):\n";
    std::cout << "  struct MyClass {\n";
    std::cout << "      void* internal_ptr;   // Type stays void*\n";
    std::cout << "      int creation_time;    // Type stays int\n";
    std::cout << "  };\n";
    std::cout << "  \n";
    std::cout << "  // Configure separately - struct is untouched!\n";
    std::cout << "  MIRROR_EXCLUDE(MyClass, internal_ptr);\n";
    std::cout << "  MIRROR_READONLY(MyClass, creation_time);\n\n";

    std::cout << "Benefits of Approach 2:\n";
    std::cout << "  ✓ Struct definition stays natural and unchanged\n";
    std::cout << "  ✓ No wrapper types affecting serialization/ABI\n";
    std::cout << "  ✓ Type-safe: compiler catches typos in field names\n";
    std::cout << "  ✓ Works with third-party structs you can't modify\n";
    std::cout << "  ✓ Configuration is separate from data definition\n";
}

int main() {
    std::cout << "=== Non-Intrusive Field Configuration Tests ===\n\n";

    test_userprofile_config();
    test_gameentity_config();
    test_no_config();

    print_syntax_comparison();

    std::cout << "\n=== All Tests Passed! ===\n";
    return 0;
}
