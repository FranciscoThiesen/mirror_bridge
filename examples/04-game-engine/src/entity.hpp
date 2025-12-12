#pragma once
#include <string>
#include <vector>
#include <memory>

// Forward declarations
struct Transform;
struct Component;

// 2D/3D position and rotation
struct Transform {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double rotation = 0.0;

    void translate(double dx, double dy, double dz) {
        x += dx;
        y += dy;
        z += dz;
    }

    void rotate(double angle) {
        rotation += angle;
    }

    double distance_from_origin() const {
        return std::sqrt(x*x + y*y + z*z);
    }
};

// Base component for entity-component system
struct Component {
    std::string name;
    bool enabled = true;

    void enable() { enabled = true; }
    void disable() { enabled = false; }
};

// A game entity with transform and components
struct Entity {
    std::string name;
    Transform transform;
    std::vector<std::string> tags;
    bool active = true;

    void set_position(double x, double y, double z) {
        transform.x = x;
        transform.y = y;
        transform.z = z;
    }

    void move(double dx, double dy, double dz) {
        transform.translate(dx, dy, dz);
    }

    void add_tag(std::string tag) {
        tags.push_back(tag);
    }

    bool has_tag(std::string tag) const {
        for (const auto& t : tags) {
            if (t == tag) return true;
        }
        return false;
    }

    int tag_count() const {
        return static_cast<int>(tags.size());
    }

    void activate() { active = true; }
    void deactivate() { active = false; }

    std::string to_string() const {
        return "Entity(" + name + " at " +
               std::to_string(transform.x) + "," +
               std::to_string(transform.y) + "," +
               std::to_string(transform.z) + ")";
    }
};

// A simple scene containing entities
struct Scene {
    std::string name;
    std::vector<std::string> entity_names;

    void add_entity(std::string entity_name) {
        entity_names.push_back(entity_name);
    }

    int entity_count() const {
        return static_cast<int>(entity_names.size());
    }

    std::vector<std::string> get_entities() const {
        return entity_names;
    }
};
