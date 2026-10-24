local m = require("const_members_lua")

print("=== const and non-assignable members, Lua ===")

local s = m.Sensor()
assert(s.id == 42, "const integer member should read its C++ initialiser")
assert(s.model == "MB-1000", "const string member should read")
print("  ok  const members read")

s.reading = 2.5
assert(s.reading == 2.5, "a non-const member beside a const one still writes")
assert(math.abs(s:scaled(4.0) - 10.0) < 1e-9, "a method sees the written value")
print("  ok  the writable member beside them still works")

local ok, err = pcall(function() s.id = 1 end)
assert(not ok, "writing a const member should raise, not succeed silently")
assert(tostring(err):find("not assignable"), "the error should say why: " .. tostring(err))
assert(s.id == 42, "and the value must be unchanged")
print("  ok  writing a const member is refused by name")

-- Fixed has only Fixed(int), so the setter has no local it can default-construct.
-- Readable, not assignable -- the same situation reached a different way.
local h = m.Holder()
assert(h:anchor_value() == 7, "a member whose type has no default constructor still reads")
h.adjustable = 9.0
assert(h.adjustable == 9.0, "and its neighbours are unaffected")
h:reseat(21)
assert(h:anchor_value() == 21, "and C++ can still change it through a method")
print("  ok  a member with no default constructor binds as readable")

print("PASS: const and non-assignable members (Lua)")
