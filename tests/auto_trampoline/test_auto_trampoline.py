"""Auto-trampoline: vtable-swap driven by reflection lets Python subclasses
override C++ virtuals without any hand-written trampoline class."""
import gc
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

# An owned object handed back from C++ has to be released the way it was
# allocated. When the payload is small enough to live inside the Python
# object it was placement-constructed, and a plain `delete` on it aborts the
# process rather than failing a check -- so the assertion is that the
# interpreter is still here afterwards.
clone = mod.clone_animal(a)
check(clone.Sound() == "generic", f"a returned Animal is usable: {clone.Sound()}")
del clone
gc.collect()
check(True, "releasing a returned Animal does not abort")

# Copying out of an object whose vptr was swapped. The copy is a plain
# Animal, so it must answer the C++ default rather than inherit the override,
# and the registry entry that the swap created belongs to d and not to it.
clone_sub = mod.clone_animal(d)
check(clone_sub.Sound() == "generic",
      f"a copy taken from a subclass is sliced, as C++ slices it: {clone_sub.Sound()}")
del clone_sub
gc.collect()
check(d.Sound() == "Woof!", "and the original still dispatches to Python")

print()
print("A class with no parameterized constructor says so instead of ignoring arguments.")
try:
    mod.Animal("surplus")
    check(False, "Animal(\"surplus\") silently dropped the argument")
except TypeError as exc:
    check("takes no constructor arguments" in str(exc),
          f"Animal(\"surplus\") is refused: {exc}")

print()
print("A class whose constructor takes an argument is built in tp_init, not tp_new.")

r = mod.Robot()
check(r.Sound() == "beep", f"Robot() constructs at all: {r.Sound()!r}")
check(zoo.describe_robot(r) == "beep from unnamed",
      f"and C++ sees the base: {zoo.describe_robot(r)!r}")

r2 = mod.Robot("R2")
check(r2.id() == "R2", f"the constructor argument is kept: {r2.id()!r}")
check(zoo.describe_robot(r2) == "beep from R2",
      f"and reaches C++: {zoo.describe_robot(r2)!r}")


class Talker(mod.Robot):
    """A subclass with its own __init__ calling super().

    This is the shape that goes through the __init__ wrapper descriptor
    PyType_Ready builds in tp_dict, which holds the tp_init pointer it saw
    at that moment. A tp_init written over the slot after PyType_Ready is
    not the one reached here, and the object would be constructed with no
    vtable swap: C++ would quietly call the base while Python calls the
    override, which is a wrong answer rather than a crash.
    """

    def __init__(self, name):
        super().__init__(name)
        self.greeting = "hello " + name

    def Sound(self):
        return self.greeting


t = Talker("ada")
check(t.id() == "ada", f"super().__init__ built the C++ object: {t.id()!r}")
check(t.greeting == "hello ada", "and the subclass's own __init__ body ran")
check(zoo.describe_robot(t) == "hello ada from ada",
      f"C++ dispatches to the override: {zoo.describe_robot(t)!r}")


class Silent(mod.Robot):
    def Sound(self):
        return "..."


s_ = Silent("s1")
check(zoo.describe_robot(s_) == "... from s1",
      f"a subclass without its own __init__ also dispatches: {zoo.describe_robot(s_)!r}")

# Constructing through py_init means the payload lands in the inline slot,
# so the dealloc has to destroy it in place rather than free it as a heap
# pointer. Getting that wrong aborts the process rather than failing a check.
del r, r2, t, s_
gc.collect()
check(True, "releasing an object built in tp_init does not abort")

print()
if fail == 0:
    print("Auto-trampoline tests passed — no hand-written trampoline code.")
    sys.exit(0)
else:
    print(f"{fail} test(s) FAILED")
    sys.exit(1)
