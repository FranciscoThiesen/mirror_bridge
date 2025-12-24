// Test: Non-Intrusive Field Configuration
//
// Shows the clean syntax for configuring field binding behavior
// without modifying your struct definitions.

#include "python/mirror_bridge_config.hpp"
#include <iostream>
#include <string>
#include <utility>

// ============================================================================
// Your structs - COMPLETELY NATURAL, UNCHANGED
// ============================================================================

struct UserProfile {
    int user_id;
    std::string username;
    std::string email;
    std::string password_hash;
    int internal_id;
    std::string created_at;
};

struct GameEntity {
    float x, y, z;
    std::string name;
    void* render_handle;
    int flags;
    int entity_id;
};

// ============================================================================
// Configuration - Simple and clean!
// ============================================================================

// Validate fields exist (compile-time check)
MIRROR_VALIDATE(UserProfile, password_hash);
MIRROR_VALIDATE(UserProfile, internal_id);
MIRROR_VALIDATE(UserProfile, created_at);
MIRROR_VALIDATE(GameEntity, render_handle);
MIRROR_VALIDATE(GameEntity, flags);
MIRROR_VALIDATE(GameEntity, entity_id);

// Configure excluded fields
template<> inline constexpr auto mirror_bridge::exclude<UserProfile> =
    mirror_bridge::fields("password_hash", "internal_id");

template<> inline constexpr auto mirror_bridge::exclude<GameEntity> =
    mirror_bridge::fields("render_handle", "flags");

// Configure readonly fields
template<> inline constexpr auto mirror_bridge::readonly<UserProfile> =
    mirror_bridge::fields("created_at");

template<> inline constexpr auto mirror_bridge::readonly<GameEntity> =
    mirror_bridge::fields("entity_id");

// ============================================================================
// Tests
// ============================================================================

int main() {
    std::cout << "=== Non-Intrusive Field Configuration ===\n\n";

    std::cout << "UserProfile checks:\n";
    std::cout << "  is_excluded(password_hash): "
              << mirror_bridge::is_excluded<UserProfile>("password_hash") << "\n";
    std::cout << "  is_excluded(internal_id):   "
              << mirror_bridge::is_excluded<UserProfile>("internal_id") << "\n";
    std::cout << "  is_excluded(username):      "
              << mirror_bridge::is_excluded<UserProfile>("username") << "\n";
    std::cout << "  is_readonly(created_at):    "
              << mirror_bridge::is_readonly<UserProfile>("created_at") << "\n";

    std::cout << "\nGameEntity checks:\n";
    std::cout << "  is_excluded(render_handle): "
              << mirror_bridge::is_excluded<GameEntity>("render_handle") << "\n";
    std::cout << "  is_excluded(flags):         "
              << mirror_bridge::is_excluded<GameEntity>("flags") << "\n";
    std::cout << "  is_readonly(entity_id):     "
              << mirror_bridge::is_readonly<GameEntity>("entity_id") << "\n";

    std::cout << "\n=== Syntax Summary ===\n\n";

    std::cout << "Your struct stays 100% natural:\n";
    std::cout << "  struct UserProfile {\n";
    std::cout << "      int user_id;\n";
    std::cout << "      std::string password_hash;  // Normal field\n";
    std::cout << "      std::string created_at;     // Normal field\n";
    std::cout << "  };\n\n";

    std::cout << "Configuration (separate, type-safe):\n";
    std::cout << "  // Validate fields exist at compile time\n";
    std::cout << "  MIRROR_VALIDATE(UserProfile, password_hash);\n\n";
    std::cout << "  // Configure excluded/readonly\n";
    std::cout << "  template<> inline constexpr auto mirror_bridge::exclude<UserProfile> =\n";
    std::cout << "      mirror_bridge::fields(\"password_hash\");\n\n";

    std::cout << "Benefits:\n";
    std::cout << "  ✓ Struct definition is 100% natural C++\n";
    std::cout << "  ✓ MIRROR_VALIDATE catches typos at compile time\n";
    std::cout << "  ✓ Works with third-party/legacy structs\n";
    std::cout << "  ✓ Configuration can be in a separate file\n";

    return 0;
}
