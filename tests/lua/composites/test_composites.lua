-- Composite conversions either deliver the value they were given or refuse
-- it, and a container element may be any type a top-level value may be.
--
-- Before: a non-nil value for a shared_ptr to an abstract class became a
-- null pointer (scene.shape = circle cleared the member; area_of(circle)
-- received nullptr); a std::array accepted any length and kept nothing, so
-- body.pos = {1, 2, 3} read back as uninitialised memory; a std::span
-- arrived empty; every member of Collections but frames failed the whole
-- module to compile, and frames stored garbage.

local m = require("composites")

local passed = 0

local function ok(message)
    passed = passed + 1
    print("  PASS: " .. message)
end

local function rejects(fn)
    local succeeded, err = pcall(fn)
    assert(not succeeded, "the assignment or call was accepted")
    return tostring(err)
end

local function mentions(err, ...)
    for _, needle in ipairs({...}) do
        assert(err:find(needle, 1, true), "expected '" .. needle .. "' in: " .. err)
    end
end

-- A smart pointer to an abstract class: only nil converts
local scene = m.Scene()
scene:set_circle(2)
mentions(rejects(function() scene.shape = 42 end), "shape")
assert(scene:has_shape(), "a refused assignment cleared the member")
mentions(rejects(function() scene.shape = m.Circle(1) end), "shape", "Circle")
assert(scene:has_shape(), "a refused assignment cleared the member")
ok("a value that is not nil is refused, and the member keeps its pointer")

scene.shape = nil
assert(not scene:has_shape(), "nil did not clear the member")
assert(scene:area_of(nil) == -1, "area_of(nil)")
ok("nil still clears it, and passes as a null argument")

mentions(rejects(function() return scene:area_of(m.Circle(1)) end), "area_of", "argument 1")
ok("an argument that cannot become the smart pointer is refused, not passed as null")

-- std::array: exactly N elements, filled in place
local body = m.Body()
assert(body:sum3({1, 2, 3}) == 6, "sum3 received zeros")
body.pos = {1, 2, 3}
assert(body:pos_sum() == 6, "pos was not filled")
local pos = body.pos
assert(#pos == 3 and pos[1] == 1 and pos[2] == 2 and pos[3] == 3, "pos read back wrong")
ok("a std::array member and parameter receive the elements they were given")

rejects(function() body.pos = {1, 2} end)
rejects(function() body.pos = {1, 2, 3, 4} end)
rejects(function() body.pos = {1, "two", 3} end)
assert(body:pos_sum() == 6, "a refused assignment changed the member")
mentions(rejects(function() return body:sum3({1, 2, 3, 4}) end), "sum3", "argument 1")
ok("the wrong length or a bad element is refused, and the member keeps its value")

-- std::span: cannot be filled from a table, so it is refused
mentions(rejects(function() return body:total({1, 2, 3}) end), "total", "argument 1")
ok("a std::span parameter is refused rather than computed over nothing")

-- Element types the container converters could not see
local c = m.Collections()

c.points = {{x = 1, y = 2}, {x = 3, y = 4}}
assert(c:points_sum() == 10, "vector<Point2> from tables")
local points = c.points
assert(#points == 2 and points[2].x == 3 and points[2].y == 4, "vector<Point2> read back")
c.points = points
assert(c:points_sum() == 10, "vector<Point2> did not round-trip its own wrappers")
local p = m.Point2()
p.x, p.y = 5, 6
c.points = {p}
assert(c:points_sum() == 11, "vector<Point2> from a wrapper")
rejects(function() c.points = {m.Mark()} end)
assert(c:points_sum() == 11, "a refused assignment changed the member")
ok("std::vector of a bound class: from tables, from wrappers, and back; not from another class")

c.ends = {{x = 1, y = 1}, {x = 2, y = 2}}
assert(c:ends_sum() == 6, "array<Point2, 2>")
rejects(function() c.ends = {{x = 1, y = 1}} end)
assert(c:ends_sum() == 6, "a refused assignment changed the member")
ok("std::array of a bound class")

c.maybe = {1, 2}
assert(c:maybe_sum() == 3, "vector<optional<int>>")
c:fill_maybe()
local maybe = c.maybe
assert(maybe[1] == 1 and maybe[2] == nil and maybe[3] == 3, "vector<optional<int>> read back")
ok("std::vector of std::optional")

c.shared = {{x = 1, y = 2}, {x = 3, y = 4}}
assert(c:shared_sum() == 10, "vector<shared_ptr<Point2>>")
assert(c.shared[2].x == 3, "vector<shared_ptr<Point2>> read back")
ok("std::vector of std::shared_ptr")

c.pairs = {{2, 1.5}, {4, 0.25}}
assert(c:pairs_sum() == 4, "vector<pair<int, double>>")
local pairs_back = c.pairs
assert(pairs_back[1][1] == 2 and pairs_back[1][2] == 1.5, "vector<pair> read back")
ok("std::vector of std::pair")

c.flags = {true, false, true}
assert(c:flag_count() == 2, "vector<bool>")
local flags = c.flags
assert(flags[1] == true and flags[2] == false and flags[3] == true, "vector<bool> read back")
ok("std::vector<bool>, whose elements iterate as a proxy under libc++")

c.frames = {{1, 2, 3}, {4, 5, 6}}
assert(c:frames_sum() == 21, "vector<array<double, 3>>")
rejects(function() c.frames = {{1, 2}} end)
assert(c:frames_sum() == 21, "a refused assignment changed the member")
ok("std::vector of std::array checks each element's length")

c:add_cell(1, 2)
local cells = c.cells
assert(#cells == 1 and cells[1].row == 1 and cells[1].col == 2, "vector<Cell> read back")
mentions(rejects(function() c.cells = {} end), "cells")
assert(c:cell_count() == 1, "a refused assignment changed the member")
ok("a vector of a class with no default constructor reads, and refuses assignment")

-- A variant or an optional of a class needs every field
local choices = m.Choices()
choices.pc = {r = 1, g = 2, b = 3}
assert(choices:pc_index() == 1 and choices:pc_red() == 1, "a Color table became a Point2")
choices.pc = {x = 1, y = 2}
assert(choices:pc_index() == 0, "a Point2 table did not select Point2")
ok("std::variant of two classes picks the one whose fields the table has")

rejects(function() choices.maybe_point = {} end)
rejects(function() choices.maybe_point = {x = 5} end)
assert(not choices:has_point(), "a table missing fields became a Point2")
choices.maybe_point = {x = 5, y = 6}
assert(choices:has_point(), "a complete table did not convert")
local point = m.Point2()
choices.maybe_point = point
assert(choices:has_point(), "a wrapper did not convert")
ok("std::optional of a class refuses a table that is missing fields, and takes a wrapper")

local owner = m.Owner()
owner.head = {value = 4, extra = 9}
assert(owner:head_value() == 4 and owner:head_extra() == 9, "optional<Node> from a table")
ok("a movable, non-copyable class inside a composite still converts from a table")

-- Maps inside composites: each of these crashed on its first element
local maps = m.Maps()
maps.layers = {{a = 1, b = 2}, {c = 3}}
assert(maps:layers_total() == 6, "vector<map<string, int>>")
assert(maps.layers[2].c == 3, "vector<map> read back")
maps.items = {{tags = {a = 1}}, {tags = {b = 2, c = 3}}}
assert(maps:items_total() == 6, "vector of a struct holding a map")
maps.single = {tags = {x = 7}}
assert(maps:single_total() == 7, "a struct holding a map, assigned from a table")
ok("a map inside a vector, and inside a struct, converts from a table")

print("")
print("All " .. passed .. " Lua composite tests passed!")
