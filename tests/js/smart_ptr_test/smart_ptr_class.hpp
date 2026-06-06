#pragma once
#include <memory>
#include <string>

struct Data {
    std::string name;
    int value = 0;
};

struct ResourceMgr {
    std::unique_ptr<Data> unique_data;
    std::shared_ptr<Data> shared_data;

    void set_unique(std::unique_ptr<Data> ptr) { unique_data = std::move(ptr); }
    void set_shared(std::shared_ptr<Data> ptr) { shared_data = ptr; }
    std::string get_unique_name() const { return unique_data ? unique_data->name : "null"; }
    std::string get_shared_name() const { return shared_data ? shared_data->name : "null"; }
    std::shared_ptr<Data> make_shared_data(std::string name, int value) {
        return std::make_shared<Data>(Data{name, value});
    }
};

// Abstract pointee: only nil <-> reset round-trips; binding this must compile.
struct Shape {
    virtual ~Shape() = default;
    virtual double area() const = 0;
};

struct ShapeHolder {
    std::shared_ptr<Shape> shape;
    bool has_shape() const { return shape != nullptr; }
};
