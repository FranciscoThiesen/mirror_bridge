-- A constructor call builds the object it was asked for, or raises an error
-- that says why.
--
-- Before, arguments that no constructor accepted were dropped in favour of
-- the default constructor: Rect(2) was a 0 x 0 rectangle and Point2(3, 4)
-- was (0, 0), with no error. Rect.new(2, 3) read its arguments one slot too
-- far along and built a 0 x 0 rectangle as well. A deleted overload, an
-- abstract class that declares a constructor, and a constructor taking a
-- non-const reference each failed the whole module to compile, so this file
-- could not have been loaded at all.

local m = require("constructors")

local passed = 0

local function ok(message)
    passed = passed + 1
    print("  PASS: " .. message)
end

-- The error a call raised; fails if the call was accepted.
local function rejects(fn, ...)
    local succeeded, err = pcall(fn, ...)
    assert(not succeeded, "the call was accepted")
    return tostring(err)
end

local function mentions(err, ...)
    for _, needle in ipairs({...}) do
        assert(err:find(needle, 1, true), "expected '" .. needle .. "' in: " .. err)
    end
end

-- The three spellings of a call
assert(m.Rect(2, 3):area() == 6, "Rect(2, 3)")
assert(m.Rect:new(2, 3):area() == 6, "Rect:new(2, 3)")
assert(m.Rect.new(2, 3):area() == 6, "Rect.new(2, 3) did not read its first argument")
ok("Rect(...), Rect:new(...) and Rect.new(...) all build from their arguments")

assert(m.Rect():area() == 0 and m.Rect:new():area() == 0 and m.Rect.new():area() == 0,
       "a call with no arguments")
ok("each spelling with no arguments default-constructs")

local new = m.Rect.new
assert(new(m.Rect, 2, 3):area() == 6, "new(Rect, 2, 3)")
ok("handing new the class table explicitly is the colon form")

-- The usual Lua inheritance idiom: a table whose __index reaches the class
local Sub = setmetatable({}, {__index = m.Rect})
local SubSub = setmetatable({}, {__index = Sub})
assert(Sub:new(2, 3):area() == 6 and SubSub:new(2, 3):area() == 6, "Sub:new(2, 3)")
assert(Sub:new():area() == 0, "Sub:new()")
ok("Sub:new(...) on a table inheriting the class table is the colon form too")

assert(m.Segment.new({x = 0, y = 0}, {x = 3, y = 4}):length() == 5,
       "a table first argument was taken for the class table")
assert(m.Segment(m.Point2(), {x = 6, y = 8}):length() == 10, "Segment(Point2(), {...})")
ok("a table as the first argument is an argument, not the class table")

-- Arguments no constructor accepts are refused, never defaulted
mentions(rejects(m.Rect, 2), "Rect", "no constructor takes 1 argument", "constructors take 0 or 2")
mentions(rejects(m.Rect, 1, 2, 3), "Rect", "no constructor takes 3 arguments")
ok("a count no constructor takes is refused, naming the counts that work")

mentions(rejects(m.Rect, {}, {}), "Rect", "argument 1")
mentions(rejects(m.Rect, 2, "tall"), "Rect", "argument 2")
mentions(rejects(m.Cell, 1.5, 2), "Cell", "argument 1")
ok("a value that will not convert is refused, naming the argument")

mentions(rejects(m.Point2, 3, 4), "Point2", "takes no constructor arguments")
local p = m.Point2()
assert(p.x == 0 and p.y == 0, "Point2()")
ok("an aggregate refuses arguments and still default-constructs")

mentions(rejects(m.Cell), "Cell", "no default constructor", "constructors take 2")
local cell = m.Cell(4, 5)
assert(cell.row == 4 and cell.col == 5, "Cell(4, 5)")
ok("a class with no default constructor asks for its arguments")

-- Constructors of one arity are tried in declaration order
local numeric = m.Tagged(2, 3)
assert(numeric.value == 6 and numeric.tag == "numeric", "Tagged(2, 3)")
assert(m.Tagged("a", "b").tag == "ab", "Tagged('a', 'b')")
mentions(rejects(m.Tagged, true, "x"),
         "Tagged", "no constructor taking 2 arguments accepts", "boolean, string")
ok("of two constructors taking two arguments, the one that converts wins")

-- Constructors that cannot be called are not offered
assert(m.Celsius(36.5).degrees == 36.5, "Celsius(36.5)")
assert(m.Celsius(36).degrees == 36, "an integer reached the deleted Celsius(int)")
ok("a deleted overload is not offered")

mentions(rejects(m.Gauge, 5), "Gauge", "abstract")
mentions(rejects(m.Gauge), "Gauge", "abstract")
local gauge = m.FixedGauge(5)
assert(gauge:measure() == 6 and gauge:doubled_tag() == 10, "FixedGauge(5)")
ok("an abstract class with a constructor binds, refuses construction, and its subclass works")

assert(m.Widget(4).level == 4 and m.Widget().level == 0, "Widget(4), Widget()")
mentions(rejects(m.Widget, m.Config()), "Widget", "argument 1")
ok("a constructor taking a non-const reference is not offered; the others still are")

mentions(rejects(m.Line, m.Cell(0, 1), m.Cell(0, 4)), "Line", "takes no constructor arguments")
assert(m.Line().length == 0, "Line()")
ok("a constructor taking a class with no default constructor is not offered")

mentions(rejects(m.View), "View", "cannot be constructed from Lua")
mentions(rejects(m.View, m.Config()), "View", "cannot be constructed from Lua")
ok("a class with nothing callable says so")

assert(m.Big(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17).sum == 153, "Big(1..17)")
ok("a constructor with seventeen parameters")

local ints = m.Mixed(5, 6)
assert(ints.tag == "ints" and ints.a == 5 and ints.b == 6, "Mixed(5, 6) did not reach Mixed(int, int)")
local tagged = m.Mixed("x", {7, 8})
assert(tagged.tag == "x" and tagged.a == 7, "Mixed('x', {7, 8})")
ok("a failed overload attempt leaves the arguments as the caller passed them")

assert(m.Linked().value == 0, "Linked()")
mentions(rejects(m.Linked, 1, nil), "Linked", "takes no constructor arguments")
ok("a constructor taking a raw pointer is not offered")

assert(m.Meas(1 / 3, 1).value == 1 / 3, "the numeric overload received a rewritten argument")
assert(m.Meas("ab", {1, 2, 3}).value == 5, "Meas('ab', {1, 2, 3})")
ok("a failed attempt at a borrowed-string overload leaves a number argument intact")

-- A throwing constructor
mentions(rejects(m.Checked, -1), "non-negative")
assert(m.Checked(3).value == 3, "Checked(3)")
ok("a C++ exception from a constructor becomes a Lua error")

print("")
print("All " .. passed .. " Lua constructor tests passed!")
