#!/usr/bin/env python3
"""
Test cross-file dependencies (Student -> University -> Address)
Each class is in a separate .hpp file
"""

import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', 'build'))

try:
    import student
except ImportError as e:
    print(f"❌ Failed to import student: {e}")
    sys.exit(1)

def main():
    print("=== Mirror Bridge Cross-File Dependencies Test ===\n")

    # Test 1: Create Student with cross-file dependencies
    print("Test 1: Creating Student with cross-file nested types...")
    s = student.Student()
    print(f"  ✓ Created Student")
    print(f"  name: '{s.name}'")
    print(f"  student_id: {s.student_id}")
    print(f"  school (University from university.hpp): {s.school}")
    # University and Address are now bound classes
    assert hasattr(s.school, 'name'), "University should have name attribute"
    assert hasattr(s.school, 'location'), "University should have location attribute"
    assert hasattr(s.school.location, 'city'), "Address should have city attribute"
    print("  ✓ Cross-file dependency chain works: Student -> University -> Address")
    print()

    # Test 2: Set values across file boundaries
    # Note: Nested object properties must be modified on the object first,
    # then assigned to parent (getter returns a copy)
    print("Test 2: Setting nested values from different files...")
    s.name = "Alice"
    s.student_id = 54321

    # Build the nested objects from innermost out
    addr = student.Address()
    addr.street = '77 Massachusetts Ave'
    addr.city = 'Cambridge'
    addr.country = 'USA'
    addr.zip_code = 2139

    uni = student.University()
    uni.name = 'MIT'
    uni.student_count = 11000
    uni.location = addr

    s.school = uni

    print(f"  Student: {s.name} (ID: {s.student_id})")
    print(f"  School: {s.school.name}")
    print(f"  Location: {s.school.location.city}, {s.school.location.country}")

    assert s.name == "Alice"
    assert s.school.name == "MIT"
    assert s.school.location.city == "Cambridge"
    print("  ✓ All cross-file nested values work correctly")
    print()

    # Test 3: Verify all classes are accessible
    print("Test 3: Verify all classes are bound...")
    addr2 = student.Address()
    addr2.city = "Boston"
    uni2 = student.University()
    uni2.name = "Harvard"
    print(f"  ✓ Address class accessible: city={addr2.city}")
    print(f"  ✓ University class accessible: name={uni2.name}")
    print()

    print("="*40)
    print("✓ All cross-file dependency tests passed!")
    print("="*40)
    return 0

if __name__ == "__main__":
    sys.exit(main())
