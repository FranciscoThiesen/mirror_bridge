"""Auto-trampoline: vtable-swap driven by reflection lets Python subclasses
override C++ virtuals without any hand-written trampoline class."""
import sys
sys.path.append("build")

import auto_trampoline_test as mod

fail = 0
def check(cond, msg):
    global fail
    print(("  PASS: " if cond else "  FAIL: ") + msg)
    if not cond: fail += 1

print("Auto-trampoline: Python overrides of C++ virtuals via reflection")
print()

zoo = mod.Zoo()

# Instance with no Python subclass — base defaults reachable through the
# custom vtable's fallback path.
a = mod.Animal()
check(zoo.describe(a) == "generic from Animal",
      f"direct Animal() uses C++ base: {zoo.describe(a)!r}")

# Python subclass overrides ONE virtual; the other should fall back to C++.
class Dog(mod.Animal):
    def Sound(self):
        return "Woof!"

d = Dog()
check(zoo.describe(d) == "Woof! from Animal",
      f"override Sound, fallback Name: {zoo.describe(d)!r}")

# Subclass overrides multiple virtuals.
class Cat(mod.Animal):
    def Sound(self):
        return "Meow"
    def Name(self):
        return "Whiskers"

c = Cat()
check(zoo.describe(c) == "Meow from Whiskers",
      f"override both: {zoo.describe(c)!r}")

# Subclass with no overrides — all virtuals fall through to C++.
class Plain(mod.Animal):
    pass

p = Plain()
check(zoo.describe(p) == "generic from Animal",
      f"no overrides: {zoo.describe(p)!r}")

# Weight is a different virtual; independent per-instance state.
class Heavy(mod.Animal):
    def Weight(self):
        return 200

class Light(mod.Animal):
    def Weight(self):
        return 3

h = Heavy()
l = Light()
check(zoo.total_weight(h, l) == 203,
      f"multiple instances, different overrides: total = {zoo.total_weight(h, l)}")

# Non-virtual method (LegCount) still works — it's not in the vtable,
# and our reflection-based method binding handles it directly.
check(d.LegCount() == 4, f"non-virtual still works: {d.LegCount()}")

# Pure Python-visible method calls still work via Python MRO (these never
# touch the vtable — Python finds them by name lookup).
check(d.Sound() == "Woof!", "direct Python call still works")
check(a.Sound() == "generic", "direct Python call on non-subclass also works")

print()
if fail == 0:
    print("Auto-trampoline tests passed — no hand-written trampoline code.")
    sys.exit(0)
else:
    print(f"{fail} test(s) FAILED")
    sys.exit(1)
