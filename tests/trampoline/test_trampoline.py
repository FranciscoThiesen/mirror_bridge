"""Python subclasses override C++ virtual methods, and the override is
called when C++ code invokes the virtual (the 'trampoline' path)."""
import sys
sys.path.append("build")

import trampoline_test

fail = 0
def check(cond, msg):
    global fail
    print(("  PASS: " if cond else "  FAIL: ") + msg)
    if not cond: fail += 1

print("Trampoline: C++ virtual dispatches to Python override")
print()

class Dog(trampoline_test.Animal):
    def Sound(self): return "Woof!"
    def Name(self): return "Rex"

class Silent(trampoline_test.Animal):
    def Sound(self): return "(silence)"
    # Name NOT overridden — should fall through to C++ base

zoo = trampoline_test.Zoo()
dog = Dog()
silent = Silent()

# Critical test: C++ Zoo::describe calls virtual Sound/Name. With trampoline,
# it routes to Python's Dog.Sound / Dog.Name.
result = zoo.describe(dog)
check(result == "Zoo heard: Woof! from Rex",
      f"C++ virtual -> Python override: {result!r}")

# Fallback to C++ base when Python doesn't override (Silent has no Name).
result2 = zoo.describe(silent)
check(result2 == "Zoo heard: (silence) from Animal",
      f"C++ virtual falls back to base when no Python override: {result2!r}")

# Direct Python calls still work (no trampoline needed — Python MRO finds the override).
check(dog.Sound() == "Woof!", "direct Python call: Dog.Sound()")
check(dog.Name() == "Rex", "direct Python call: Dog.Name()")
check(dog.LegCount() == 4, "inherited C++ non-virtual: LegCount()")

print()
if fail == 0:
    print("Trampoline tests passed.")
    sys.exit(0)
else:
    print(f"{fail} test(s) FAILED")
    sys.exit(1)
