-- Class-typed values are identity-checked at the Lua boundary.
--
-- Before the check the only test was lua_isuserdata, so any userdata
-- satisfied any class-typed parameter: c.at(label, 2.0) returned
-- 4.0513382958982e-322, a denormal read out of the std::string's bytes.
-- These tests pin both halves of the fix: a wrong class raises, and the
-- derived-to-base conversions that are supposed to work still do, now
-- including a base that is not at offset zero.

local ts = require("type_safety")

local passed = 0

local function ok(message)
    passed = passed + 1
    print("  PASS: " .. message)
end

local function rejects(fn, message)
    local succeeded, err = pcall(fn)
    assert(not succeeded, message .. " was accepted")
    return tostring(err)
end

-- The dangerous case: a real wrapper, so every null check passes, but the
-- wrong class.
local curve, label = ts.Curve.new(), ts.Label.new()

assert(curve:at(2.0) == 0.1, "correct method call broke")
assert(label:size() == #label.tag, "correct method call broke")
ok("the correct type still converts")

local err = rejects(function() return curve.at(label, 2.0) end,
                    "Curve.at called on a Label")
assert(err:find("Curve"), "error does not name the expected class: " .. err)
assert(err:find("Label"), "error does not name the actual class: " .. err)
ok("self of the wrong class raises, naming both classes")

for _, value in ipairs({ {"number", 42}, {"string", "text"}, {"boolean", true} }) do
    rejects(function() return curve.at(value[2], 2.0) end,
            "Curve.at called on a " .. value[1])
end
ok("self of a non-wrapper type raises")

local portfolio = ts.Portfolio.new()
assert(portfolio:book(ts.Curve.new()) == 0.05, "correct argument broke")

err = rejects(function() return portfolio:book(label) end,
              "Portfolio.book(Label)")
assert(err:find("Curve"), "error does not name the expected class: " .. err)
assert(err:find("Label"), "error does not name the actual class: " .. err)
ok("an unrelated bound class is rejected as an argument, not reinterpreted")

rejects(function() return portfolio:label_size(curve) end, "Portfolio.label_size(Curve)")
ok("the rejection runs in both directions")

-- Metamethods can be pulled off a metatable and called on anything.
local metatable = getmetatable(curve)
rejects(function() return metatable.__index(label, "rate") end, "__index on a Label")
rejects(function() return metatable.__newindex(label, "rate", 1.0) end, "__newindex on a Label")
ok("a hijacked metamethod raises instead of reading foreign bytes")

-- Base is at offset 0, so this worked before the check. Regression guard.
assert(portfolio:take_base(ts.Derived.new()) == 7, "derived-to-base conversion broke")
ok("derived-to-base still converts (offset 0)")

-- Second is NOT at offset 0: before the check this returned First's bytes.
assert(portfolio:take_second(ts.Both.new()) == 22222.0,
       "conversion to a non-first base landed on the wrong subobject")
ok("conversion to a non-first base lands on the right subobject")

-- The downcast direction: a Base is not a Derived.
rejects(function() return portfolio:take_second(ts.First.new()) end,
        "Portfolio.take_second(First)")
ok("an unrelated base is still refused")

-- Field assignment goes through the same conversion.
rejects(function() portfolio.held = label end, "Portfolio.held = Label")
portfolio.held = ts.Curve.new()
assert(portfolio.held.rate == 0.05, "class-typed field assignment broke")
ok("class-typed field assignment is checked")

-- A table is still accepted where a class is expected: that is how nested
-- structs are written from Lua, and the check must not close it.
portfolio.held = { rate = 0.25 }
assert(portfolio.held.rate == 0.25, "table-to-struct assignment broke")
ok("a table still converts to a bound class")


-- ------------------------------------------------------------------------
-- Forged identity. A userdata's metatable looks like the natural identity
-- and is not one: Lua lets a script put any metatable on a table with plain
-- setmetatable, and on any value at all with debug.setmetatable. Checking
-- the attached metatable therefore accepted a value with no wrapper behind
-- it and read a C++ object out of it anyway.
--
-- The identity is a word written inside the wrapper, which Lua has no API to
-- write, so each of these must be refused rather than crashing or answering.

local forged_table = setmetatable({}, metatable)

local function refuses_forgery(label, fn)
    local succeeded, err = pcall(fn)
    assert(not succeeded, label .. " was accepted")
    return tostring(err)
end

refuses_forgery("Curve.at on a table wearing Curve's metatable",
                function() return curve.at(forged_table, 2.0) end)
refuses_forgery("Curve.at on it with no arguments",
                function() return curve.at(forged_table) end)
refuses_forgery("__index on it",
                function() return metatable.__index(forged_table, "rate") end)
refuses_forgery("__newindex on it",
                function() return metatable.__newindex(forged_table, "rate", 1.0) end)
refuses_forgery("it as a class-typed argument",
                function() return portfolio:book(forged_table) end)
ok("a table wearing the class metatable is refused, not dereferenced")

-- A full userdata from somewhere else entirely. io handles are the standard
-- library's own, so this needs no helper module: before the fix the first
-- word of a luaL_Stream was read as the C++ object's address.
local handle = io.open("/dev/null", "w")

refuses_forgery("a foreign userdata as a receiver",
                function() return curve.at(handle, 2.0) end)
refuses_forgery("a foreign userdata as an argument",
                function() return portfolio:book(handle) end)
ok("a full userdata from another library is refused")

debug.setmetatable(handle, metatable)
local message = refuses_forgery("a foreign userdata wearing the metatable",
                                function() return curve.at(handle, 2.0) end)
assert(message:find("got userdata"),
       "a forged value described itself as the class it imitated: " .. message)
refuses_forgery("writing through a foreign userdata",
                function() return metatable.__newindex(handle, "rate", 1.5) end)
-- __gc reached by hand with a foreign pointer would be a free of it.
assert(pcall(metatable.__gc, handle), "__gc raised instead of declining")
ok("debug.setmetatable does not get a read, a write or a free")

-- Our own wrapper wearing another class's metatable: the bytes still say
-- Label, so the error must still name Label.
local disguised = ts.Label.new()
debug.setmetatable(disguised, metatable)
message = refuses_forgery("a Label wearing Curve's metatable",
                          function() return curve.at(disguised, 2.0) end)
assert(message:find("Label"), "error does not name the actual class: " .. message)
ok("a disguised wrapper is refused and still named correctly")

-- __gc is reachable by hand on a genuine wrapper. It must not leave the
-- object to be freed a second time by the collector.
local throwaway = ts.Curve.new()
assert(pcall(metatable.__gc, throwaway), "__gc raised on its own class")
assert(pcall(metatable.__gc, throwaway), "a second __gc raised")
assert(not pcall(function() return throwaway:at(2.0) end),
       "a destroyed object still answered")
collectgarbage("collect")
collectgarbage("collect")
ok("__gc by hand is idempotent and the collector does not double free")

print()
print(("All %d Lua type safety tests passed"):format(passed))
