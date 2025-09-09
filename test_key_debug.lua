-- Test script to debug KEY table behavior
local engine = require("engine")
local input = engine.create_input_state()

print("Testing KEY table behavior...")

-- Test 1: Access KEY table
print("1. Accessing InputState.KEY.A:", input.KEY.A)

-- Test 2: Try to modify KEY table
print("2. Attempting to modify InputState.KEY.A...")
local success, error_msg = pcall(function()
    input.KEY.A = 999
end)
print("   Modification result:", success, error_msg)

-- Test 3: Check if modification worked
print("3. Checking InputState.KEY.A after modification:", input.KEY.A)

-- Test 4: Check metatable
print("4. Checking metatable...")
local mt = getmetatable(input.KEY)
if mt then
    print("   Metatable exists")
    print("   __newindex:", mt.__newindex)
else
    print("   No metatable found")
end
