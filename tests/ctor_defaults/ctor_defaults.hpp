#pragma once
#include <string>

namespace cd {

// The shape SQLite::Database has: a trailing defaulted pointer, which Python
// cannot spell at all. Passing "" for it is not the same call -- SQLiteCpp
// handed the empty string to sqlite3 as a VFS name and threw "no such vfs".
struct Conn {
    std::string path;
    int flags = 0;
    int timeout = 0;
    std::string vfs;
    Conn(const char* p, int f = 7, int t = 0, const char* v = nullptr)
        : path(p), flags(f), timeout(t), vfs(v ? v : "(none)") {}
};

struct Mix {
    int a, b, c;
    Mix(int a, int b = 2, int c = 3) : a(a), b(b), c(c) {}
    // Same defaults on a method, to check the two paths answer alike.
    int sum(int x, int y = 20, int z = 300) const { return x + y + z; }
};

// No defaults at all: the exact-arity path must keep working.
struct Strict {
    int x, y;
    Strict(int x, int y) : x(x), y(y) {}
};

}  // namespace cd
