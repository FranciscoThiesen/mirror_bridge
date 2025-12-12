# Example 04: Game Engine

A mini game engine demonstrating nested objects and complex class relationships.

## What You'll Learn

- Nested objects (Entity contains Transform)
- Setting nested objects from Python dicts
- Classes with multiple methods
- Working with related classes

## The C++ Code

```cpp
struct Transform {
    double x = 0.0, y = 0.0, z = 0.0;
    double rotation = 0.0;

    void translate(double dx, double dy, double dz);
    void rotate(double angle);
    double distance_from_origin() const;
};

struct Entity {
    std::string name;
    Transform transform;  // Nested object!
    std::vector<std::string> tags;
    bool active = true;

    void set_position(double x, double y, double z);
    void move(double dx, double dy, double dz);
    void add_tag(std::string tag);
    bool has_tag(std::string tag) const;
};

struct Scene {
    std::string name;
    std::vector<std::string> entity_names;

    void add_entity(std::string name);
    int entity_count() const;
};
```

## Build & Run

```bash
# Generate Python bindings
../../tools/mirror_bridge generate src/ --module game_engine --lang python

# Run the test
python3 test_game_engine.py
```

## Usage

```python
import game_engine

# Create an entity
player = game_engine.Entity()
player.name = "Player"
player.set_position(100.0, 50.0, 0.0)

# Access nested transform
print(player.transform.x)  # 100.0
player.transform.rotate(45.0)

# Move the entity
player.move(10.0, 5.0, 0.0)

# Add tags
player.add_tag("player")
player.add_tag("controllable")
print(player.has_tag("player"))  # True

# Set nested object from dict
player.transform = {"x": 0.0, "y": 0.0, "z": 0.0, "rotation": 0.0}

# Create a scene
level = game_engine.Scene()
level.name = "Level1"
level.add_entity("Player")
level.add_entity("Enemy")
print(level.entity_count())  # 2
```

## Nested Object Handling

When accessing nested objects:
- **Reading**: Returns the nested object with all its properties and methods
- **Writing**: Can assign a dict with matching field names

```python
# Reading nested object - returns full object
transform = entity.transform
transform.rotate(90.0)  # Methods work!

# Writing nested object - use dict
entity.transform = {"x": 10.0, "y": 20.0, "z": 0.0, "rotation": 45.0}
```

## Next Steps

- [05-production](../05-production/) - Full project structure
