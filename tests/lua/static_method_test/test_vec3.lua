-- Test Vec3 Lua bindings for:
-- 1. Static methods (hot_loop, add_vectors)
-- 2. Methods with const reference parameters (dot)

local vec3 = require("vec3")

print("============================================================")
print("Vec3 Lua Binding Tests")
print("============================================================")
print()

local all_passed = true

-- Test 1: Basic construction
print("Testing basic construction...")
local v = vec3.Vec3()
assert(v.x == 0 and v.y == 0 and v.z == 0, "Default constructor failed")
print("  PASS: Default constructor works")

-- Constructor arguments, in each of Lua's three spellings. Vec3.new(1, 2, 3)
-- used to skip its first argument, find no two-argument constructor, and
-- return (0, 0, 0); wrong arguments used to return (0, 0, 0) as well.
for label, make in pairs({
    ["Vec3(1, 2, 3)"] = function() return vec3.Vec3(1, 2, 3) end,
    ["Vec3:new(1, 2, 3)"] = function() return vec3.Vec3:new(1, 2, 3) end,
    ["Vec3.new(1, 2, 3)"] = function() return vec3.Vec3.new(1, 2, 3) end,
}) do
    local built = make()
    assert(built.x == 1 and built.y == 2 and built.z == 3, label .. " did not use its arguments")
end
print("  PASS: Vec3(...), Vec3:new(...) and Vec3.new(...) build from their arguments")

local refused, err = pcall(vec3.Vec3, 1, 2)
assert(not refused, "Vec3(1, 2) matched no constructor and was accepted anyway")
assert(tostring(err):find("no constructor takes 2 arguments", 1, true), tostring(err))
print("  PASS: Vec3(1, 2) is refused instead of default-constructed")

-- Test 2: Setting values
v.x = 1.0
v.y = 2.0
v.z = 3.0
assert(v.x == 1.0 and v.y == 2.0 and v.z == 3.0, "Setting values failed")
print("  PASS: Setting values works")

-- Test 3: Const reference parameter (dot method)
print()
print("Testing const reference parameter (dot method)...")
local a = vec3.Vec3()
a.x, a.y, a.z = 1.0, 2.0, 3.0
local b = vec3.Vec3()
b.x, b.y, b.z = 4.0, 5.0, 6.0

-- dot product: 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
local result = a:dot(b)
assert(math.abs(result - 32.0) < 0.001, "dot() returned wrong value: " .. result)
print(string.format("  PASS: a:dot(b) = %.1f", result))

-- Test 4: length method
print()
print("Testing length method...")
local v2 = vec3.Vec3()
v2.x, v2.y, v2.z = 3.0, 4.0, 0.0
local len = v2:length()
assert(math.abs(len - 5.0) < 0.001, "length() returned wrong value: " .. len)
print(string.format("  PASS: length() = %.1f", len))

-- Test 5: Static method (hot_loop)
print()
print("Testing static method (hot_loop)...")
local hot_result = vec3.Vec3.hot_loop(1000)
assert(hot_result > 0, "hot_loop() returned non-positive value")
print(string.format("  PASS: Vec3.hot_loop(1000) = %.2f", hot_result))

-- Test 6: Static method with const ref params returning bound type
print()
print("Testing static method with const ref params (add_vectors)...")
local v_a = vec3.Vec3()
v_a.x, v_a.y, v_a.z = 1.0, 2.0, 3.0
local v_b = vec3.Vec3()
v_b.x, v_b.y, v_b.z = 4.0, 5.0, 6.0

local v_sum = vec3.Vec3.add_vectors(v_a, v_b)
assert(math.abs(v_sum.x - 5.0) < 0.001, "add_vectors x wrong: " .. v_sum.x)
assert(math.abs(v_sum.y - 7.0) < 0.001, "add_vectors y wrong: " .. v_sum.y)
assert(math.abs(v_sum.z - 9.0) < 0.001, "add_vectors z wrong: " .. v_sum.z)
print(string.format("  PASS: Vec3.add_vectors(a, b) = (%.1f, %.1f, %.1f)", v_sum.x, v_sum.y, v_sum.z))

-- Test 7: Verify returned object has working methods
print()
print("Testing that returned object has working methods...")
local v_sum_len = v_sum:length()
-- length of (5, 7, 9) = sqrt(25 + 49 + 81) = sqrt(155) ≈ 12.45
assert(v_sum_len > 12.0 and v_sum_len < 13.0, "Returned object length wrong: " .. v_sum_len)
print(string.format("  PASS: Returned Vec3 has working methods, length() = %.2f", v_sum_len))

print()
print("============================================================")
print("ALL TESTS PASSED!")
print("============================================================")
