#!/usr/bin/env python3
"""
Test deep nesting support (3 levels: Employee -> Company -> Address)
"""

import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', 'build'))

try:
    import company
except ImportError as e:
    print(f"❌ Failed to import company: {e}")
    sys.exit(1)

def main():
    print("=== Mirror Bridge Deep Nesting Test ===\n")

    # Test 1: Create Employee with default values
    print("Test 1: Creating Employee with 3-level nesting...")
    emp = company.Employee()
    print(f"  ✓ Created Employee")
    print(f"  name: '{emp.name}'")
    print(f"  id: {emp.id}")
    print(f"  employer (Company): {emp.employer}")
    assert emp.name == ""
    assert emp.id == 0
    # Company and Address are now bound classes
    assert hasattr(emp.employer, 'name'), "Company should have name attribute"
    assert hasattr(emp.employer, 'headquarters'), "Company should have headquarters attribute"
    assert hasattr(emp.employer.headquarters, 'city'), "Address should have city attribute"
    print("  ✓ 3-level nesting structure verified")
    print()

    # Test 2: Set deeply nested values
    # Note: Nested object properties must be modified on the object first,
    # then assigned to parent (getter returns a copy)
    print("Test 2: Setting deeply nested address...")
    emp.name = "Bob"
    emp.id = 12345

    # Build from innermost out
    hq = company.Address()
    hq.street = '100 Tech Blvd'
    hq.city = 'San Francisco'
    hq.zip_code = 94105

    employer = company.Company()
    employer.name = 'TechCorp'
    employer.employee_count = 500
    employer.headquarters = hq

    emp.employer = employer

    print(f"  Set employee: {emp.name}")
    print(f"  Set company: {emp.employer.name}")
    print(f"  Set HQ city: {emp.employer.headquarters.city}")

    # Verify all 3 levels
    assert emp.name == "Bob"
    assert emp.employer.name == "TechCorp"
    assert emp.employer.headquarters.city == "San Francisco"
    print("  ✓ All 3 levels accessible")
    print()

    # Test 3: Create and assign new Company
    print("Test 3: Assigning new Company object...")
    new_hq = company.Address()
    new_hq.street = '200 New St'
    new_hq.city = 'Seattle'
    new_hq.zip_code = 98101

    new_company = company.Company()
    new_company.name = 'NewCorp'
    new_company.employee_count = 1000
    new_company.headquarters = new_hq

    emp.employer = new_company
    assert emp.employer.name == "NewCorp"
    assert emp.employer.headquarters.city == "Seattle"
    print(f"  ✓ Updated to {emp.employer.name} in {emp.employer.headquarters.city}")
    print()

    print("="*40)
    print("✓ All deep nesting tests passed!")
    print("="*40)
    return 0

if __name__ == "__main__":
    sys.exit(main())
