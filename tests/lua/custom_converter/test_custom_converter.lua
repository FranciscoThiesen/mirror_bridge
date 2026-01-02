-- Test for custom type converter extension point (Lua)
-- Verifies that user-defined converters work correctly for types
-- not natively supported by mirror_bridge

local pixel = require("pixel")

print("=== Custom Type Converter Test (Lua) ===\n")

-- Test 1: Create Pixel with default Color3
print("Test 1: Default construction...")
local p = pixel.Pixel()
assert(p.x == 0 and p.y == 0)
local color = p:get_color()
assert(color[1] == 0 and color[2] == 0 and color[3] == 0,
    string.format("Expected {0,0,0}, got {%d,%d,%d}", color[1], color[2], color[3]))
print(string.format("  Default color: {%d, %d, %d}", color[1], color[2], color[3]))

-- Test 2: Get Color3 as Lua table
print("\nTest 2: Color3 returned as table...")
p.x = 10
p.y = 20
color = p:get_color()
assert(type(color) == "table", "Expected table, got " .. type(color))
-- Check both indexed and named access
assert(color[1] == color.r, "Indexed and named access should match for r")
assert(color[2] == color.g, "Indexed and named access should match for g")
assert(color[3] == color.b, "Indexed and named access should match for b")
print(string.format("  Color via index: {%d, %d, %d}", color[1], color[2], color[3]))
print(string.format("  Color via name:  {r=%d, g=%d, b=%d}", color.r, color.g, color.b))

-- Test 3: Set Color3 from indexed table
print("\nTest 3: Set color from indexed table...")
p:set_color({255, 128, 64})
color = p:get_color()
assert(color[1] == 255 and color[2] == 128 and color[3] == 64,
    string.format("Expected {255,128,64}, got {%d,%d,%d}", color[1], color[2], color[3]))
print(string.format("  Set from {255, 128, 64}: {%d, %d, %d}", color[1], color[2], color[3]))

-- Test 4: Set Color3 from named table
print("\nTest 4: Set color from named table...")
p:set_color({r = 100, g = 150, b = 200})
color = p:get_color()
assert(color.r == 100 and color.g == 150 and color.b == 200,
    string.format("Expected {r=100,g=150,b=200}, got {r=%d,g=%d,b=%d}", color.r, color.g, color.b))
print(string.format("  Set from {r=100, g=150, b=200}: {%d, %d, %d}", color[1], color[2], color[3]))

-- Test 5: Method returning Color3 (inverted_color)
print("\nTest 5: Method returning custom type...")
p:set_color({100, 50, 25})
local inverted = p:inverted_color()
assert(inverted[1] == 155 and inverted[2] == 205 and inverted[3] == 230,
    string.format("Expected {155,205,230}, got {%d,%d,%d}", inverted[1], inverted[2], inverted[3]))
print(string.format("  Original: {100, 50, 25}, Inverted: {%d, %d, %d}",
    inverted[1], inverted[2], inverted[3]))

-- Test 6: Color3 as property (direct field access)
print("\nTest 6: Color3 as direct property...")
p.color = {10, 20, 30}
local direct_color = p.color
assert(direct_color[1] == 10 and direct_color[2] == 20 and direct_color[3] == 30,
    string.format("Expected {10,20,30}, got {%d,%d,%d}", direct_color[1], direct_color[2], direct_color[3]))
print(string.format("  Direct property access: {%d, %d, %d}",
    direct_color[1], direct_color[2], direct_color[3]))

-- Test 7: Round-trip consistency
print("\nTest 7: Round-trip consistency...")
local test_colors = {
    {0, 0, 0},
    {255, 255, 255},
    {128, 64, 32},
    {1, 2, 3},
}
for _, expected in ipairs(test_colors) do
    p:set_color(expected)
    local actual = p:get_color()
    assert(actual[1] == expected[1] and actual[2] == expected[2] and actual[3] == expected[3],
        string.format("Round-trip failed: {%d,%d,%d} -> {%d,%d,%d}",
            expected[1], expected[2], expected[3], actual[1], actual[2], actual[3]))
end
print(string.format("  All %d round-trip tests passed", #test_colors))

print("\n" .. string.rep("=", 40))
print("All custom type converter tests passed!")
print(string.rep("=", 40))
