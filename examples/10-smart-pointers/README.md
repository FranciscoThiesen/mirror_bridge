# Example 10: Smart Pointers

**Difficulty**: Intermediate

Demonstrates binding `std::shared_ptr`, `std::unique_ptr`, and `std::weak_ptr`.

## What You'll Learn

- Shared ownership with `shared_ptr`
- Exclusive ownership with `unique_ptr`
- Avoiding cycles with `weak_ptr`
- Returning nullptr as Python None
- Factory patterns creating smart pointers
- Ownership transfer semantics

## Files

```
src/
└── resources.hpp    # Resource, ResourcePool, Editor, TreeNode, HandleManager
test_smart_ptrs.py   # Python tests
```

## Key Concepts

### Shared Ownership (shared_ptr)
```cpp
class ResourceFactory {
public:
    std::shared_ptr<Resource> create(const std::string& name);
};

class ResourcePool {
public:
    void add(std::shared_ptr<Resource> res);
    std::shared_ptr<Resource> get(int id) const;
};
```

```python
factory = resources.ResourceFactory()
pool = resources.ResourcePool()

# Create and share
r = factory.create("connection")
pool.add(r)

# Both refer to same object
r.name = "modified"
assert pool.get(1).name == "modified"
```

### Exclusive Ownership (unique_ptr)
```cpp
class Editor {
private:
    std::unique_ptr<Document> doc;
public:
    void new_document(const std::string& title);
    void load_document(std::unique_ptr<Document> d);
    std::unique_ptr<Document> release_document();
};
```

```python
editor1 = resources.Editor()
editor1.new_document("Draft")

# Transfer ownership
doc = editor1.release_document()
editor2 = resources.Editor()
editor2.load_document(doc)

# editor1 no longer has document
assert not editor1.has_document()
assert editor2.has_document()
```

### Parent References (weak_ptr)
```cpp
class TreeNode {
public:
    std::vector<std::shared_ptr<TreeNode>> children;
    std::weak_ptr<TreeNode> parent;  // Avoids reference cycles

    bool has_parent() const { return !parent.expired(); }
    std::shared_ptr<TreeNode> get_parent() const { return parent.lock(); }
};
```

```python
root = resources.TreeBuilder.create_node("root")
child = resources.TreeBuilder.create_node("child")
resources.TreeBuilder.add_child(root, child)

assert child.has_parent()
assert child.get_parent().value == "root"
```

### nullptr as None
```cpp
std::shared_ptr<Resource> get(int id) const {
    // Returns nullptr if not found
    return nullptr;
}
```

```python
result = pool.get(999)
assert result is None  # nullptr maps to None
```

### Internal Unique Ownership
```cpp
class HandleManager {
private:
    std::vector<std::unique_ptr<Handle>> handles;
public:
    Handle* create_handle();  // Returns raw pointer (manager retains ownership)
    int handle_count() const;
    void clear();  // Destroys all handles
};
```

```python
manager = resources.HandleManager()
h = manager.create_handle()
print(h.id(), h.is_valid())
manager.clear()  # All handles destroyed
```

## Running

```bash
cd examples/10-smart-pointers
../../mirror_bridge_auto src/ --module resources
python3 test_smart_ptrs.py
```

## Ownership Patterns

### Factory + Pool (shared_ptr)
Objects created by factory, stored in pools, accessible from multiple places.

### Editor (unique_ptr)
Single owner at a time, explicit transfer with release/load.

### Tree (shared_ptr + weak_ptr)
Children owned by parent (shared), parent reference is weak to prevent cycles.

### Handle Manager (unique_ptr internally)
Manager owns handles, returns raw pointers for use. Handles invalidated when manager clears.

## Notes

- `shared_ptr` allows multiple Python references to same C++ object
- `unique_ptr` parameters transfer ownership from Python to C++
- `unique_ptr` return values transfer ownership from C++ to Python
- `weak_ptr` prevents reference cycles in tree/graph structures
- `nullptr` always becomes Python `None`
- Raw pointers returned from functions that don't transfer ownership still work
