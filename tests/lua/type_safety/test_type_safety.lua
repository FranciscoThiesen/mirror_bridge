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

print()
print(("All %d Lua type safety tests passed"):format(passed))
