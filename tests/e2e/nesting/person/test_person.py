#!/usr/bin/env python3
"""
Test nested class support in mirror_bridge.
Tests binding of classes that contain other bindable classes.
"""

import sys
import os

# Add build directory to path (4 levels up from tests/e2e/nesting/person)
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', 'build'))

try:
    import person
except ImportError as e:
    print(f"❌ Failed to import person: {e}")
    print("Make sure to build the bindings first with ../build_bindings.sh")
    sys.exit(1)

def test_person_nested():
    """Test Person class with nested Address class"""
    print("=== Mirror Bridge Nested Class Test ===\n")

    # Test 1: Create Person and check default values
    print("Test 1: Creating Person with default values...")
    p = person.Person()
    print(f"  ✓ Created Person")
    print(f"  name: '{p.name}'")
    print(f"  age: {p.age}")
    print(f"  address: {p.address}")
    assert p.name == "", "Default name should be empty"
    assert p.age == 0, "Default age should be 0"
    # Address is now a bound class, not a dict
    assert hasattr(p.address, 'street'), "Address should have street attribute"
    print()

    # Test 2: Modify simple fields
    print("Test 2: Modifying simple fields...")
    p.name = "Alice"
    p.age = 30
    print(f"  Set name: '{p.name}'")
    print(f"  Set age: {p.age}")
    assert p.name == "Alice", "Name should be updated"
    assert p.age == 30, "Age should be updated"
    print(f"  ✓ Simple fields work correctly")
    print()

    # Test 3: Create and assign new Address object
    # Note: Nested object properties must be modified on the Address object first,
    # then assigned to Person (getter returns a copy)
    print("Test 3: Assigning new Address object...")
    new_addr = person.Address()
    new_addr.street = "123 Main St"
    new_addr.city = "Springfield"
    new_addr.zip_code = 12345
    p.address = new_addr
    print(f"  Set address: street='{p.address.street}', city='{p.address.city}', zip={p.address.zip_code}")
    assert p.address.street == "123 Main St", "Street should be updated"
    assert p.address.city == "Springfield", "City should be updated"
    assert p.address.zip_code == 12345, "Zip code should be updated"
    print(f"  ✓ Nested class works correctly")
    print()

    # Test 4: Replace address with another
    print("Test 4: Replacing address...")
    another_addr = person.Address()
    another_addr.street = "456 Oak Ave"
    another_addr.city = "Portland"
    another_addr.zip_code = 97201
    p.address = another_addr
    print(f"  Updated address: street='{p.address.street}', city='{p.address.city}'")
    assert p.address.street == "456 Oak Ave", "Street should be updated"
    print(f"  ✓ Address can be completely replaced")
    print()

    # Test 5: Multiple instances with different addresses
    print("Test 5: Multiple independent instances...")
    p1 = person.Person()
    p2 = person.Person()
    p1.name = "Bob"
    addr1 = person.Address()
    addr1.city = "NYC"
    p1.address = addr1
    p2.name = "Charlie"
    addr2 = person.Address()
    addr2.city = "LA"
    p2.address = addr2
    print(f"  p1: {p1.name} at {p1.address.city}")
    print(f"  p2: {p2.name} at {p2.address.city}")
    assert p1.address.city != p2.address.city, "Instances should be independent"
    print(f"  ✓ Multiple instances work independently")
    print()

    print("========================================")
    print("✓ All nested class tests passed!")
    print("========================================")

if __name__ == "__main__":
    test_person_nested()
