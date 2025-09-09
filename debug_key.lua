-- Test script to debug KEY table behavior
local input = InputState

print("=== Testing KEY table access ===")
print("InputState.KEY.A =", InputState.KEY.A)

print("=== Testing KEY table modification ===")
local result = pcall(function()
    InputState.KEY.A = 999
end)
print("Assignment result:", result)

print("=== Testing KEY table after modification ===")
print("InputState.KEY.A =", InputState.KEY.A)

print("=== Testing metatable ===")
local key_table = InputState.KEY
print("KEY table metatable:", getmetatable(key_table))
print("KEY table metatable __newindex:", getmetatable(key_table).__newindex)
