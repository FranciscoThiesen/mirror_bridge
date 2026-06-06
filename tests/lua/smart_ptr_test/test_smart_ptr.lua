-- Smart pointer support: unique_ptr/shared_ptr members, params, returns,
-- and abstract pointees (nil round-trip only).
local mod = require("smart_ptr_test")

local pass, fail = 0, 0
local function check(name, cond)
    if cond then pass = pass + 1; print("  PASS: " .. name)
    else fail = fail + 1; print("  FAIL: " .. name) end
end

local mgr = mod.ResourceMgr.new()

check("null unique_ptr reads as nil-ish name", mgr:get_unique_name() == "null")
check("null shared_ptr reads as nil-ish name", mgr:get_shared_name() == "null")

-- Table -> smart pointer param (deep-copy semantics)
mgr:set_unique({ name = "alpha", value = 1 })
check("unique_ptr param materialized from table", mgr:get_unique_name() == "alpha")

mgr:set_shared({ name = "beta", value = 2 })
check("shared_ptr param materialized from table", mgr:get_shared_name() == "beta")

-- nil -> reset
mgr:set_shared(nil)
check("nil resets shared_ptr", mgr:get_shared_name() == "null")

-- Smart pointer return: deref'd to a bound value
local d = mgr:make_shared_data("gamma", 3)
check("shared_ptr return yields usable object", d ~= nil and d.name == "gamma" and d.value == 3)

-- Abstract pointee: compiles, defaults to no shape
local holder = mod.ShapeHolder.new()
check("abstract-pointee holder constructs", holder:has_shape() == false)

print("")
if fail == 0 then
    print("All " .. pass .. " Lua smart pointer tests passed!")
else
    print(fail .. " Lua smart pointer tests FAILED")
    os.exit(1)
end
