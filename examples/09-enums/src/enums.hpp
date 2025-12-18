#pragma once
#include <string>
#include <stdexcept>

// Example: Enum Types
// Demonstrates binding scoped and unscoped enums

namespace game {

// Scoped enum (preferred in modern C++)
enum class Color {
    Red,
    Green,
    Blue,
    Yellow,
    Cyan,
    Magenta
};

// Enum with explicit values
enum class Priority {
    Low = 1,
    Normal = 5,
    High = 10,
    Critical = 100
};

// Flags-style enum (powers of 2)
enum class Permission {
    None = 0,
    Read = 1,
    Write = 2,
    Execute = 4,
    All = 7  // Read | Write | Execute
};

// Status enum for state machines
enum class Status {
    Idle,
    Running,
    Paused,
    Completed,
    Error
};

// Direction enum
enum class Direction {
    North = 0,
    East = 90,
    South = 180,
    West = 270
};

// Helper functions for enums
std::string color_name(Color c) {
    switch (c) {
        case Color::Red: return "red";
        case Color::Green: return "green";
        case Color::Blue: return "blue";
        case Color::Yellow: return "yellow";
        case Color::Cyan: return "cyan";
        case Color::Magenta: return "magenta";
    }
    return "unknown";
}

Color color_from_name(const std::string& name) {
    if (name == "red") return Color::Red;
    if (name == "green") return Color::Green;
    if (name == "blue") return Color::Blue;
    if (name == "yellow") return Color::Yellow;
    if (name == "cyan") return Color::Cyan;
    if (name == "magenta") return Color::Magenta;
    throw std::runtime_error("Unknown color: " + name);
}

std::string priority_label(Priority p) {
    switch (p) {
        case Priority::Low: return "LOW";
        case Priority::Normal: return "NORMAL";
        case Priority::High: return "HIGH";
        case Priority::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

bool has_permission(Permission flags, Permission check) {
    return (static_cast<int>(flags) & static_cast<int>(check)) != 0;
}

std::string status_string(Status s) {
    switch (s) {
        case Status::Idle: return "idle";
        case Status::Running: return "running";
        case Status::Paused: return "paused";
        case Status::Completed: return "completed";
        case Status::Error: return "error";
    }
    return "unknown";
}

bool is_terminal_status(Status s) {
    return s == Status::Completed || s == Status::Error;
}

int direction_degrees(Direction d) {
    return static_cast<int>(d);
}

Direction rotate_clockwise(Direction d) {
    switch (d) {
        case Direction::North: return Direction::East;
        case Direction::East: return Direction::South;
        case Direction::South: return Direction::West;
        case Direction::West: return Direction::North;
    }
    return d;
}

Direction rotate_counter_clockwise(Direction d) {
    switch (d) {
        case Direction::North: return Direction::West;
        case Direction::West: return Direction::South;
        case Direction::South: return Direction::East;
        case Direction::East: return Direction::North;
    }
    return d;
}

// Class using enums
class Task {
public:
    std::string name;
    Priority priority = Priority::Normal;
    Status status = Status::Idle;

    void start() {
        if (status == Status::Idle || status == Status::Paused) {
            status = Status::Running;
        }
    }

    void pause() {
        if (status == Status::Running) {
            status = Status::Paused;
        }
    }

    void complete() {
        status = Status::Completed;
    }

    void fail() {
        status = Status::Error;
    }

    bool is_done() const {
        return is_terminal_status(status);
    }

    std::string describe() const {
        return name + " [" + priority_label(priority) + "] - " + status_string(status);
    }
};

// Player with direction
class Player {
public:
    std::string name;
    Direction facing = Direction::North;

    void turn_left() {
        facing = rotate_counter_clockwise(facing);
    }

    void turn_right() {
        facing = rotate_clockwise(facing);
    }

    int heading() const {
        return direction_degrees(facing);
    }
};

} // namespace game
