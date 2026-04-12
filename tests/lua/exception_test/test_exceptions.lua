-- Tests for C++ exception handling in Lua bindings
-- C++ exceptions should be caught and converted to Lua errors

package.cpath = package.cpath .. ";./?.so"
local mod = require("exception_test")

local pass_count = 0
local fail_count = 0

local function test(name, fn)
    local ok, err = pcall(fn)
    if ok then
        print("  PASS: " .. name)
        pass_count = pass_count + 1
    else
        print("  FAIL: " .. name .. " - " .. tostring(err))
        fail_count = fail_count + 1
    end
end

print("Testing C++ exception handling in Lua bindings:")

-- Test: successful operations should work normally
test("safe_divide success", function()
    local svc = mod.ThrowingService()
    local result = svc:safe_divide(10.0, 2.0)
    assert(result == 5.0, "Expected 5.0 but got " .. tostring(result))
end)

test("safe_sqrt success", function()
    local svc = mod.ThrowingService()
    local result = svc:safe_sqrt(4.0)
    assert(result == 2.0, "Expected 2.0 but got " .. tostring(result))
end)

-- Test: C++ exceptions should become Lua errors catchable with pcall
test("safe_divide throws on zero", function()
    local svc = mod.ThrowingService()
    local ok, err = pcall(function() svc:safe_divide(10.0, 0.0) end)
    assert(not ok, "Expected error but call succeeded")
    assert(string.find(tostring(err), "division by zero"),
        "Error should mention 'division by zero', got: " .. tostring(err))
end)

test("safe_sqrt throws on negative", function()
    local svc = mod.ThrowingService()
    local ok, err = pcall(function() svc:safe_sqrt(-1.0) end)
    assert(not ok, "Expected error but call succeeded")
    assert(string.find(tostring(err), "negative"),
        "Error should mention 'negative', got: " .. tostring(err))
end)

test("set_positive throws on non-positive", function()
    local svc = mod.ThrowingService()
    local ok, err = pcall(function() svc:set_positive(-5.0) end)
    assert(not ok, "Expected error but call succeeded")
    assert(string.find(tostring(err), "positive"),
        "Error should mention 'positive', got: " .. tostring(err))
end)

-- Test: object state should be preserved after caught exception
test("object state preserved after exception", function()
    local svc = mod.ThrowingService()
    svc:set_positive(42.0)
    assert(svc.value == 42.0, "Value should be 42.0")

    -- This should fail but not corrupt the object
    local ok, _ = pcall(function() svc:set_positive(-1.0) end)
    assert(not ok, "Expected error")

    -- Original value should still be there
    assert(svc.value == 42.0, "Value should still be 42.0 after failed set")
end)

print("")
if fail_count == 0 then
    print("All " .. pass_count .. " Lua exception tests passed!")
else
    print(fail_count .. " of " .. (pass_count + fail_count) .. " tests FAILED")
    os.exit(1)
end
