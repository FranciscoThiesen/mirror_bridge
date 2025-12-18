#!/usr/bin/env python3
"""Test smart pointer binding example."""

import sys
sys.path.insert(0, 'build')

import resources

def test_shared_ptr_basic():
    """Test basic shared_ptr creation and usage."""
    factory = resources.ResourceFactory()

    # Create shared resources
    r1 = factory.create("resource1")
    r2 = factory.create("resource2")

    assert r1.name == "resource1"
    assert r1.id == 1
    assert r2.id == 2

    assert factory.total_created() == 2

    print("✓ Basic shared_ptr creation works")


def test_resource_pool():
    """Test ResourcePool with shared resources."""
    factory = resources.ResourceFactory()
    pool = resources.ResourcePool()

    # Add resources to pool
    r1 = factory.create("db_connection")
    r2 = factory.create("file_handle")
    r3 = factory.create("cache")

    pool.add(r1)
    pool.add(r2)
    pool.add(r3)

    assert pool.count() == 3
    assert pool.active_count() == 3

    # Get by ID
    found = pool.get(2)
    assert found is not None
    assert found.name == "file_handle"

    # Get by name
    found = pool.get_by_name("cache")
    assert found is not None
    assert found.id == 3

    # Deactivate one
    r2.deactivate()
    assert pool.active_count() == 2

    # Not found returns None
    assert pool.get(999) is None
    assert pool.get_by_name("nonexistent") is None

    print("✓ ResourcePool with shared_ptr works")


def test_shared_ptr_aliasing():
    """Test that shared_ptr correctly shares ownership."""
    factory = resources.ResourceFactory()
    pool1 = resources.ResourcePool()
    pool2 = resources.ResourcePool()

    # Create and add to both pools
    resource = factory.create("shared_resource")
    pool1.add(resource)
    pool2.add(resource)

    # Modify through one reference
    resource.name = "modified"

    # Both pools see the change
    assert pool1.get(1).name == "modified"
    assert pool2.get(1).name == "modified"

    print("✓ Shared_ptr aliasing works correctly")


def test_unique_ptr_editor():
    """Test Editor with unique_ptr document."""
    editor = resources.Editor()

    # Initially no document
    assert not editor.has_document()
    assert editor.get_title() == ""

    # Create new document
    editor.new_document("My Document")
    assert editor.has_document()
    assert editor.get_title() == "My Document"

    # Edit content
    editor.set_content("Hello world!")
    assert editor.get_content() == "Hello world!"
    assert editor.word_count() == 2

    editor.append(" How are you?")
    assert editor.word_count() == 5

    # Close document
    editor.close()
    assert not editor.has_document()

    print("✓ Editor with unique_ptr document works")


def test_unique_ptr_transfer():
    """Test transferring unique_ptr between editors."""
    editor1 = resources.Editor()
    editor2 = resources.Editor()

    # Create document in editor1
    editor1.new_document("Transferable Doc")
    editor1.set_content("Important content")

    # Release from editor1
    doc = editor1.release_document()
    assert not editor1.has_document()

    # Load into editor2
    editor2.load_document(doc)
    assert editor2.has_document()
    assert editor2.get_title() == "Transferable Doc"
    assert editor2.get_content() == "Important content"

    print("✓ Unique_ptr ownership transfer works")


def test_tree_with_weak_ptr():
    """Test tree structure with weak_ptr parent references."""
    root = resources.TreeBuilder.create_node("root")
    child1 = resources.TreeBuilder.create_node("child1")
    child2 = resources.TreeBuilder.create_node("child2")
    grandchild = resources.TreeBuilder.create_node("grandchild")

    resources.TreeBuilder.add_child(root, child1)
    resources.TreeBuilder.add_child(root, child2)
    resources.TreeBuilder.add_child(child1, grandchild)

    assert root.child_count() == 2
    assert child1.child_count() == 1

    # Check parent references (weak_ptr)
    assert not root.has_parent()
    assert child1.has_parent()
    assert child1.get_parent().value == "root"

    # Check depth
    assert root.depth() == 0
    assert child1.depth() == 1
    assert grandchild.depth() == 2

    print("✓ Tree with weak_ptr parents works")


def test_sample_tree():
    """Test pre-built sample tree."""
    root = resources.TreeBuilder.build_sample_tree()

    assert root.value == "root"
    assert root.child_count() == 2

    a = root.get_child(0)
    assert a.value == "A"
    assert a.child_count() == 2

    b = root.get_child(1)
    assert b.value == "B"
    assert b.child_count() == 0

    a1 = a.get_child(0)
    assert a1.value == "A1"
    assert a1.depth() == 2

    print("✓ Sample tree structure works")


def test_handle_manager():
    """Test HandleManager with internal unique_ptr storage."""
    manager = resources.HandleManager()

    # Create handles
    h1 = manager.create_handle()
    h2 = manager.create_handle()
    h3 = manager.create_handle()

    assert manager.handle_count() == 3
    assert manager.valid_count() == 3

    assert h1.id() == 1
    assert h2.id() == 2
    assert h1.is_valid()

    # Invalidate one
    h2.invalidate()
    assert not h2.is_valid()
    assert manager.valid_count() == 2

    # Invalidate all
    manager.invalidate_all()
    assert manager.valid_count() == 0
    assert not h1.is_valid()

    # Clear
    manager.clear()
    assert manager.handle_count() == 0

    print("✓ HandleManager with unique_ptr storage works")


def test_nullptr_handling():
    """Test that nullptr returns None in Python."""
    pool = resources.ResourcePool()

    # Empty pool returns None
    result = pool.get(1)
    assert result is None

    result = pool.get_by_name("nonexistent")
    assert result is None

    # Out of bounds returns None
    node = resources.TreeBuilder.create_node("test")
    assert node.get_child(0) is None
    assert node.get_child(-1) is None

    print("✓ Nullptr correctly maps to None")


if __name__ == "__main__":
    test_shared_ptr_basic()
    test_resource_pool()
    test_shared_ptr_aliasing()
    test_unique_ptr_editor()
    test_unique_ptr_transfer()
    test_tree_with_weak_ptr()
    test_sample_tree()
    test_handle_manager()
    test_nullptr_handling()

    print("\n✓ All smart pointer tests passed!")
