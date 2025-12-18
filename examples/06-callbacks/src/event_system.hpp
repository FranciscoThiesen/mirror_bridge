#pragma once
#include <functional>
#include <string>
#include <vector>

// Example: Callbacks and std::function
// Demonstrates passing Python functions to C++ code

namespace events {

// Simple callback types
using VoidCallback = std::function<void()>;
using IntCallback = std::function<void(int)>;
using StringCallback = std::function<std::string(const std::string&)>;

// Event data structure
struct Event {
    std::string type;
    int value;
    double timestamp;
};

// Button that triggers callbacks
class Button {
public:
    std::string label = "Click Me";

    void set_on_click(VoidCallback callback) {
        on_click_ = callback;
    }

    void click() {
        click_count_++;
        if (on_click_) {
            on_click_();
        }
    }

    int get_click_count() const { return click_count_; }

private:
    VoidCallback on_click_;
    int click_count_ = 0;
};

// Timer that calls a callback repeatedly
class Timer {
public:
    void set_callback(IntCallback callback) {
        callback_ = callback;
    }

    void tick(int times = 1) {
        for (int i = 0; i < times; ++i) {
            tick_count_++;
            if (callback_) {
                callback_(tick_count_);
            }
        }
    }

    int get_tick_count() const { return tick_count_; }

private:
    IntCallback callback_;
    int tick_count_ = 0;
};

// Text processor with transform callback
class TextProcessor {
public:
    void set_transform(StringCallback transform) {
        transform_ = transform;
    }

    std::string process(const std::string& input) {
        if (transform_) {
            return transform_(input);
        }
        return input;
    }

private:
    StringCallback transform_;
};

// Event dispatcher for more complex scenarios
class EventDispatcher {
public:
    using EventHandler = std::function<void(const Event&)>;

    void add_handler(EventHandler handler) {
        handlers_.push_back(handler);
    }

    void dispatch(const std::string& type, int value) {
        Event event{type, value, static_cast<double>(dispatch_count_++)};
        for (auto& handler : handlers_) {
            handler(event);
        }
    }

    void clear_handlers() {
        handlers_.clear();
    }

    int handler_count() const { return static_cast<int>(handlers_.size()); }

private:
    std::vector<EventHandler> handlers_;
    int dispatch_count_ = 0;
};

} // namespace events
