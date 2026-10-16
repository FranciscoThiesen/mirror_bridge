#pragma once

#include <string>

#include "python/mirror_bridge_annotations.hpp"

// P3394 annotations as a user writes them. Both supported toolchains
// implement P3394, under different names for the lookup; this header does not
// know which, which is the point.
struct UserProfile {
    int user_id = 0;
    std::string username;

    [[=mirror_bridge::exclude{}]] std::string password_hash;
    [[=mirror_bridge::exclude{}]] int internal_flag = 0;

    [[=mirror_bridge::readonly{}]] std::string created_at;
    [[=mirror_bridge::readonly{}]] int version = 1;

    // The excluded and read-only fields still have to be reachable from C++,
    // or the test could not tell "not bound" from "not there".
    void set_password(const std::string& pw) { password_hash = pw; }
    std::string peek_password() const { return password_hash; }
    void stamp(const std::string& ts) { created_at = ts; }
    void bump_version() { ++version; }
};

// Nothing annotated, so the annotated and plain paths can be compared.
struct PlainProfile {
    int user_id = 0;
    std::string username;
};
