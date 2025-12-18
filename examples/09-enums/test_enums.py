#!/usr/bin/env python3
"""Test enum binding example."""

import sys
sys.path.insert(0, 'build')

import game_enums

def test_color_enum():
    """Test Color enum values."""
    # Enum values accessible as class attributes
    assert game_enums.Color.Red is not None
    assert game_enums.Color.Green is not None
    assert game_enums.Color.Blue is not None

    # Enums are distinct
    assert game_enums.Color.Red != game_enums.Color.Blue

    # Same enum values are equal
    c1 = game_enums.Color.Red
    c2 = game_enums.Color.Red
    assert c1 == c2

    print("✓ Color enum works")


def test_color_functions():
    """Test functions using Color enum."""
    # Enum to string
    assert game_enums.color_name(game_enums.Color.Red) == "red"
    assert game_enums.color_name(game_enums.Color.Blue) == "blue"

    # String to enum
    red = game_enums.color_from_name("red")
    assert red == game_enums.Color.Red

    # Invalid color raises
    try:
        game_enums.color_from_name("invalid")
        assert False, "Should have raised"
    except RuntimeError:
        pass

    print("✓ Color functions work")


def test_priority_enum():
    """Test Priority enum with explicit values."""
    assert game_enums.Priority.Low is not None
    assert game_enums.Priority.Normal is not None
    assert game_enums.Priority.High is not None
    assert game_enums.Priority.Critical is not None

    # Labels
    assert game_enums.priority_label(game_enums.Priority.Low) == "LOW"
    assert game_enums.priority_label(game_enums.Priority.Critical) == "CRITICAL"

    print("✓ Priority enum works")


def test_permission_flags():
    """Test Permission flags enum."""
    # Check individual permissions
    assert game_enums.has_permission(game_enums.Permission.Read, game_enums.Permission.Read)
    assert not game_enums.has_permission(game_enums.Permission.Read, game_enums.Permission.Write)

    # All has everything
    assert game_enums.has_permission(game_enums.Permission.All, game_enums.Permission.Read)
    assert game_enums.has_permission(game_enums.Permission.All, game_enums.Permission.Write)
    assert game_enums.has_permission(game_enums.Permission.All, game_enums.Permission.Execute)

    # None has nothing
    assert not game_enums.has_permission(game_enums.Permission.None_, game_enums.Permission.Read)

    print("✓ Permission flags work")


def test_status_enum():
    """Test Status enum for state machines."""
    assert game_enums.status_string(game_enums.Status.Idle) == "idle"
    assert game_enums.status_string(game_enums.Status.Running) == "running"

    # Terminal status check
    assert game_enums.is_terminal_status(game_enums.Status.Completed)
    assert game_enums.is_terminal_status(game_enums.Status.Error)
    assert not game_enums.is_terminal_status(game_enums.Status.Running)

    print("✓ Status enum works")


def test_direction_enum():
    """Test Direction enum with degree values."""
    assert game_enums.direction_degrees(game_enums.Direction.North) == 0
    assert game_enums.direction_degrees(game_enums.Direction.East) == 90
    assert game_enums.direction_degrees(game_enums.Direction.South) == 180
    assert game_enums.direction_degrees(game_enums.Direction.West) == 270

    # Rotation
    d = game_enums.Direction.North
    d = game_enums.rotate_clockwise(d)
    assert d == game_enums.Direction.East

    d = game_enums.rotate_counter_clockwise(d)
    assert d == game_enums.Direction.North

    print("✓ Direction enum works")


def test_task_class():
    """Test Task class using enums."""
    task = game_enums.Task()
    task.name = "Build feature"
    task.priority = game_enums.Priority.High

    assert task.status == game_enums.Status.Idle
    assert not task.is_done()

    task.start()
    assert task.status == game_enums.Status.Running

    task.pause()
    assert task.status == game_enums.Status.Paused

    task.start()
    assert task.status == game_enums.Status.Running

    task.complete()
    assert task.status == game_enums.Status.Completed
    assert task.is_done()

    desc = task.describe()
    assert "Build feature" in desc
    assert "HIGH" in desc
    assert "completed" in desc

    print("✓ Task class with enums works")


def test_player_direction():
    """Test Player class with direction enum."""
    player = game_enums.Player()
    player.name = "Hero"

    assert player.facing == game_enums.Direction.North
    assert player.heading() == 0

    player.turn_right()
    assert player.facing == game_enums.Direction.East
    assert player.heading() == 90

    player.turn_right()
    player.turn_right()
    assert player.facing == game_enums.Direction.West

    player.turn_left()
    assert player.facing == game_enums.Direction.South

    print("✓ Player direction works")


if __name__ == "__main__":
    test_color_enum()
    test_color_functions()
    test_priority_enum()
    test_permission_flags()
    test_status_enum()
    test_direction_enum()
    test_task_class()
    test_player_direction()

    print("\n✓ All enum tests passed!")
