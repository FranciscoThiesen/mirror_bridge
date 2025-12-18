-- Test Vec2 Lua binding - Multi-language demo

local vec2 = require("vec2")

print("╔══════════════════════════════════════════╗")
print("║          Vec2 · Lua Binding Test         ║")
print("╚══════════════════════════════════════════╝")
print()

-- Create vectors
local a = vec2.Vec2(3.0, 4.0)
local b = vec2.Vec2(1.0, 0.0)

print(string.format("  a = Vec2(%.1f, %.1f)", a.x, a.y))
print(string.format("  b = Vec2(%.1f, %.1f)", b.x, b.y))
print()

-- Test methods
print("  ┌─────────────────────────────────────┐")
print(string.format("  │ a:length()      = %-18s │", tostring(a:length())))
print(string.format("  │ a:dot(b)        = %-18s │", tostring(a:dot(b))))

local n = a:normalized()
print(string.format("  │ a:normalized()  = (%.2f, %.2f)          │", n.x, n.y))

local scaled = a:scale(2.0)
print(string.format("  │ a:scale(2.0)    = (%.1f, %.1f)           │", scaled.x, scaled.y))

local added = a:add(b)
print(string.format("  │ a:add(b)        = (%.1f, %.1f)            │", added.x, added.y))

local dist = a:distance_to(b)
print(string.format("  │ a:distance_to(b)= %-18.4f │", dist))
print("  └─────────────────────────────────────┘")
print()

-- Verify correctness
assert(math.abs(a:length() - 5.0) < 0.0001, "length() failed")
assert(math.abs(a:dot(b) - 3.0) < 0.0001, "dot() failed")
assert(math.abs(n.x - 0.6) < 0.0001, "normalized() x failed")
assert(math.abs(n.y - 0.8) < 0.0001, "normalized() y failed")

print("  ✓ All assertions passed!")
print()
