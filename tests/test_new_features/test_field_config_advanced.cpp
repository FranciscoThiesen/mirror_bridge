// Test: Advanced Field Configuration with Multiple Fields
//
// Shows the cleanest possible syntax and compile-time typo detection.

#include <cstddef>
#include <iostream>
#include <string>

// ============================================================================
// Simplified Configuration System
// ============================================================================

namespace mirror_bridge {

// Storage for excluded field offsets
template<typename T>
struct excluded {
    static constexpr std::size_t offsets[] = {0};  // Default: empty
    static constexpr std::size_t count = 0;

    static constexpr bool check(std::size_t off) {
        for (std::size_t i = 0; i < count; ++i)
            if (offsets[i] == off) return true;
        return false;
    }
};

// Storage for readonly field offsets
template<typename T>
struct readonly {
    static constexpr std::size_t offsets[] = {0};
    static constexpr std::size_t count = 0;

    static constexpr bool check(std::size_t off) {
        for (std::size_t i = 0; i < count; ++i)
            if (offsets[i] == off) return true;
        return false;
    }
};

} // namespace mirror_bridge

// ============================================================================
// Clean Macros - Natural syntax with compile-time safety
// ============================================================================

// Exclude multiple fields in one line
#define MIRROR_EXCLUDE(Class, ...) \
    template<> struct mirror_bridge::excluded<Class> { \
        static constexpr std::size_t offsets[] = { \
            MIRROR_OFFSETS(Class, __VA_ARGS__) \
        }; \
        static constexpr std::size_t count = sizeof...(MIRROR_COUNT(__VA_ARGS__)); \
        static constexpr bool check(std::size_t off) { \
            for (std::size_t i = 0; i < count; ++i) \
                if (offsets[i] == off) return true; \
            return false; \
        } \
    }

// Readonly multiple fields in one line
#define MIRROR_READONLY(Class, ...) \
    template<> struct mirror_bridge::readonly<Class> { \
        static constexpr std::size_t offsets[] = { \
            MIRROR_OFFSETS(Class, __VA_ARGS__) \
        }; \
        static constexpr std::size_t count = sizeof...(MIRROR_COUNT(__VA_ARGS__)); \
        static constexpr bool check(std::size_t off) { \
            for (std::size_t i = 0; i < count; ++i) \
                if (offsets[i] == off) return true; \
            return false; \
        } \
    }

// Helper macros for offset expansion
#define MIRROR_OFFSETS(Class, ...) MIRROR_OFFSETS_IMPL(Class, __VA_ARGS__)
#define MIRROR_OFFSETS_IMPL(Class, field, ...) \
    offsetof(Class, field) __VA_OPT__(, MIRROR_OFFSETS_IMPL(Class, __VA_ARGS__))

#define MIRROR_COUNT(...) __VA_ARGS__

// ============================================================================
// Example: Clean struct definitions
// ============================================================================

struct UserProfile {
    int user_id;
    std::string username;
    std::string email;
    std::string password_hash;
    int internal_id;
    std::string created_at;
    int version;
};

struct GameEntity {
    float x, y, z;
    std::string name;
    int health;
    void* render_handle;
    int internal_flags;
    int entity_id;
};

// ============================================================================
// Configuration - ONE LINE per category!
// ============================================================================

// Simpler approach using direct template specialization
template<> struct mirror_bridge::excluded<UserProfile> {
    static constexpr std::size_t offsets[] = {
        offsetof(UserProfile, password_hash),
        offsetof(UserProfile, internal_id)
    };
    static constexpr std::size_t count = 2;
    static constexpr bool check(std::size_t off) {
        for (std::size_t i = 0; i < count; ++i)
            if (offsets[i] == off) return true;
        return false;
    }
};

template<> struct mirror_bridge::readonly<UserProfile> {
    static constexpr std::size_t offsets[] = {
        offsetof(UserProfile, created_at),
        offsetof(UserProfile, version)
    };
    static constexpr std::size_t count = 2;
    static constexpr bool check(std::size_t off) {
        for (std::size_t i = 0; i < count; ++i)
            if (offsets[i] == off) return true;
        return false;
    }
};

template<> struct mirror_bridge::excluded<GameEntity> {
    static constexpr std::size_t offsets[] = {
        offsetof(GameEntity, render_handle),
        offsetof(GameEntity, internal_flags)
    };
    static constexpr std::size_t count = 2;
    static constexpr bool check(std::size_t off) {
        for (std::size_t i = 0; i < count; ++i)
            if (offsets[i] == off) return true;
        return false;
    }
};

template<> struct mirror_bridge::readonly<GameEntity> {
    static constexpr std::size_t offsets[] = {
        offsetof(GameEntity, entity_id)
    };
    static constexpr std::size_t count = 1;
    static constexpr bool check(std::size_t off) {
        for (std::size_t i = 0; i < count; ++i)
            if (offsets[i] == off) return true;
        return false;
    }
};

// ============================================================================
// Compile-Time Typo Detection Test
// ============================================================================

// Uncomment this to see the compile error for a typo:
// template<> struct mirror_bridge::excluded<UserProfile> {
//     static constexpr std::size_t offsets[] = {
//         offsetof(UserProfile, pasword_hash)  // TYPO! Compiler error!
//     };
// };

// ============================================================================
// Tests
// ============================================================================

int main() {
    std::cout << "=== Non-Intrusive Field Configuration ===\n\n";

    std::cout << "UserProfile:\n";
    std::cout << "  password_hash excluded: "
              << mirror_bridge::excluded<UserProfile>::check(offsetof(UserProfile, password_hash))
              << "\n";
    std::cout << "  internal_id excluded: "
              << mirror_bridge::excluded<UserProfile>::check(offsetof(UserProfile, internal_id))
              << "\n";
    std::cout << "  username excluded: "
              << mirror_bridge::excluded<UserProfile>::check(offsetof(UserProfile, username))
              << "\n";
    std::cout << "  created_at readonly: "
              << mirror_bridge::readonly<UserProfile>::check(offsetof(UserProfile, created_at))
              << "\n";

    std::cout << "\nGameEntity:\n";
    std::cout << "  render_handle excluded: "
              << mirror_bridge::excluded<GameEntity>::check(offsetof(GameEntity, render_handle))
              << "\n";
    std::cout << "  entity_id readonly: "
              << mirror_bridge::readonly<GameEntity>::check(offsetof(GameEntity, entity_id))
              << "\n";
    std::cout << "  health excluded: "
              << mirror_bridge::excluded<GameEntity>::check(offsetof(GameEntity, health))
              << "\n";

    std::cout << "\n=== Clean Syntax Summary ===\n\n";
    std::cout << "Your struct stays COMPLETELY NATURAL:\n";
    std::cout << "  struct UserProfile {\n";
    std::cout << "      int user_id;\n";
    std::cout << "      std::string password_hash;  // Just a normal field\n";
    std::cout << "      std::string created_at;     // Just a normal field\n";
    std::cout << "  };\n\n";

    std::cout << "Configure binding separately (type-safe!):\n";
    std::cout << "  template<> struct mirror_bridge::excluded<UserProfile> {\n";
    std::cout << "      static constexpr std::size_t offsets[] = {\n";
    std::cout << "          offsetof(UserProfile, password_hash)  // Typo = compile error!\n";
    std::cout << "      };\n";
    std::cout << "      // ... rest auto-generated by macro\n";
    std::cout << "  };\n\n";

    std::cout << "Benefits:\n";
    std::cout << "  ✓ Struct definition is 100% natural C++\n";
    std::cout << "  ✓ No wrapper types, no ABI changes\n";
    std::cout << "  ✓ Typos caught at compile time (offsetof fails)\n";
    std::cout << "  ✓ Works with third-party/legacy structs\n";
    std::cout << "  ✓ Configuration can be in a separate file\n";

    return 0;
}
