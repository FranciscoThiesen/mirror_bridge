#!/usr/bin/env python3
"""Test game engine classes."""

import sys
sys.path.insert(0, 'build')

import game_engine

def test_transform():
    t = game_engine.Transform()

    assert t.x == 0.0 and t.y == 0.0 and t.z == 0.0
    assert t.rotation == 0.0

    t.translate(10.0, 20.0, 30.0)
    assert t.x == 10.0 and t.y == 20.0 and t.z == 30.0

    t.rotate(45.0)
    assert t.rotation == 45.0

    dist = t.distance_from_origin()
    assert dist > 0, f"Expected positive distance, got {dist}"

    print("✓ Transform test passed")

def test_entity_basic():
    e = game_engine.Entity()
    e.name = "Player"

    assert e.name == "Player"
    assert e.active == True

    e.set_position(100.0, 50.0, 0.0)
    assert e.transform.x == 100.0
    assert e.transform.y == 50.0

    e.move(10.0, 5.0, 0.0)
    assert e.transform.x == 110.0
    assert e.transform.y == 55.0

    print("✓ Entity basic test passed")

def test_entity_tags():
    e = game_engine.Entity()
    e.name = "Enemy"

    e.add_tag("hostile")
    e.add_tag("npc")
    e.add_tag("boss")

    assert e.tag_count() == 3
    assert e.has_tag("hostile") == True
    assert e.has_tag("friendly") == False

    print("✓ Entity tags test passed")

def test_nested_object():
    e = game_engine.Entity()
    e.name = "TestEntity"

    # Access nested transform
    e.transform.x = 42.0
    e.transform.y = 24.0

    assert e.transform.x == 42.0
    assert e.transform.y == 24.0

    # Set nested object from dict
    e.transform = {"x": 1.0, "y": 2.0, "z": 3.0, "rotation": 90.0}
    assert e.transform.x == 1.0
    assert e.transform.rotation == 90.0

    print("✓ Nested object test passed")

def test_scene():
    s = game_engine.Scene()
    s.name = "Level1"

    s.add_entity("Player")
    s.add_entity("Enemy1")
    s.add_entity("Enemy2")

    assert s.entity_count() == 3

    entities = s.get_entities()
    assert "Player" in entities
    assert "Enemy1" in entities

    print("✓ Scene test passed")

def test_to_string():
    e = game_engine.Entity()
    e.name = "Hero"
    e.set_position(10.0, 20.0, 30.0)

    s = e.to_string()
    assert "Hero" in s
    assert "10" in s

    print("✓ to_string test passed")

if __name__ == "__main__":
    test_transform()
    test_entity_basic()
    test_entity_tags()
    test_nested_object()
    test_scene()
    test_to_string()
    print("\n✓ All tests passed!")
