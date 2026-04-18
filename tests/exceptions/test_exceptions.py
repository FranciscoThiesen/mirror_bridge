"""C++ std:: exceptions map to the most specific Python exception type."""
import sys
sys.path.insert(0, "build")
import exceptions_test as mod

fail = 0
def check(cond, msg):
    global fail
    print(("  PASS: " if cond else "  FAIL: ") + msg)
    if not cond: fail += 1

c = mod.Container()
c.set_size(3)

try:
    c.get(5)
    check(False, "out_of_range should raise IndexError")
except IndexError:
    check(True, "std::out_of_range → IndexError")

try:
    c.set_size(-1)
    check(False, "invalid_argument should raise ValueError")
except ValueError:
    check(True, "std::invalid_argument → ValueError")

try:
    c.check_positive(0)
    check(False, "domain_error should raise ValueError")
except ValueError:
    check(True, "std::domain_error → ValueError")

try:
    c.io_failure()
    check(False, "runtime_error should raise RuntimeError")
except RuntimeError as e:
    check("simulated IO failure" in str(e), "std::runtime_error → RuntimeError with message")

print()
if fail == 0:
    print("Exception mapping tests passed.")
    sys.exit(0)
else:
    print(f"{fail} FAILED")
    sys.exit(1)
