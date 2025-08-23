# UUID Lua API

The `UUID` API provides Lua bindings for working with **universally unique identifiers (UUIDs)**.  
UUIDs are immutable 128-bit values represented as strings in the standard format:

```
"550e8400-e29b-41d4-a716-446655440000"
```

---

## Overview

In Lua, UUIDs are represented as **proxy tables** with the metatable `UUIDProxyMeta`.  
They behave like immutable objects with properties and methods accessible via dot notation.

**Important Notes:**
- **Completely immutable** - no properties can be modified from Lua
- **Version 4 UUIDs** - generated using cryptographically strong random data
- **Standard format** - follows RFC 4122 format: 8-4-4-4-12 hexadecimal digits
- **Global constructor** - accessed via the global `UUID` table
- **Memory ownership** - Lua-created UUIDs are owned by Lua, C-created UUIDs are owned by C
- **Unique generation** - each call to `UUID.new()` generates a new random UUID

```lua
-- Create a new UUID
local id = UUID.new()

-- Access its string value
print(id.value)   --> "550e8400-e29b-41d4-a716-446655440000"

-- Reset (generate a new UUID for the same object)
id:reset()
print(id.value)
```

---

## Global UUID Table

The `UUID` table is a global table created by the engine during initialization. It provides a constructor for creating new UUID objects.

---

## Global UUID Table Methods

### `UUID.new()`
Creates a new UUID with a randomly generated **version 4 UUID** string.  

**Returns:** `UUID` object  

**Notes:**
- **Generates cryptographically strong random data** using `arc4random_buf()`
- **Sets version 4 bits** (0100 in nibble 4) as per RFC 4122
- **Sets variant bits** (10xx in nibble 13) for standard UUID format
- **Lua-owned** - the returned UUID object is owned by Lua and will be garbage collected
- **Unique** - each call generates a different UUID with extremely low collision probability

**Example:**
```lua
local id = UUID.new()
print("Generated UUID:", id.value)  --> "550e8400-e29b-41d4-a716-446655440000"

-- Each call generates a different UUID
local id2 = UUID.new()
print("Second UUID:", id2.value)   --> "a1b2c3d4-e5f6-7890-abcd-ef1234567890"

-- Verify they're different
print("UUIDs are different:", id.value ~= id2.value)  --> true
```

---

## UUID Object Properties

Each `UUID` object has the following **read-only** properties:

- `value` → the UUID string in standard format (string)  
- `string` → alias for `value` (string)  

**Notes:**
- **Both properties are read-only** - attempting to modify them will raise a Lua error
- **Standard format** - follows RFC 4122: 8-4-4-4-12 hexadecimal digits with hyphens
- **Fixed length** - always exactly 36 characters (32 hex digits + 4 hyphens)
- **Case insensitive** - hexadecimal digits are lowercase but comparison is case-insensitive
- **Immutable** - the UUID value cannot be changed once created (except via reset method)

**Example:**
```lua
local id = UUID.new()
print(id.value)   --> "550e8400-e29b-41d4-a716-446655440000"
print(id.string)  --> "550e8400-e29b-41d4-a716-446655440000"

-- Both properties return the same value
print("Properties match:", id.value == id.string)  --> true

-- Verify format
local uuid_str = id.value
print("Length:", #uuid_str)  --> 36
print("Format valid:", uuid_str:match("^%x%x%x%x%x%x%x%x%-%x%x%x%x%-%x%x%x%x%-%x%x%x%x%-%x%x%x%x%x%x%x%x%x%x%x%x$") ~= nil)  --> true
```

---

## UUID Object Methods

### `uuid:reset()`
Generates a new random UUID for the same object.  

**Returns:** nothing (modifies the UUID in-place)

**Notes:**
- **Modifies the UUID in-place** - changes the `value` property to a new random UUID
- **Same object reference** - the Lua object reference remains the same, only the value changes
- **Cryptographically secure** - uses the same strong random generation as `UUID.new()`
- **Version 4 compliant** - maintains RFC 4122 version 4 format
- **Useful for reusing objects** - avoids creating new UUID objects when you need a new value

**Example:**
```lua
local id = UUID.new()
print("Original UUID:", id.value)  --> "550e8400-e29b-41d4-a716-446655440000"

-- Store reference to verify it's the same object
local id_ref = id
print("Same object:", id == id_ref)  --> true

-- Reset to new UUID
id:reset()
print("New UUID:", id.value)        --> "a1b2c3d4-e5f6-7890-abcd-ef1234567890"

-- Verify it's still the same object
print("Still same object:", id == id_ref)  --> true
print("Value changed:", id.value ~= "550e8400-e29b-41d4-a716-446655440000")  --> true
```

---

## Restrictions

- **UUIDs are completely immutable** from Lua - no properties can be modified:

```lua
-- ❌ Invalid - will cause Lua error
id.value = "1234"
-- Error: "UUID objects are immutable - cannot set property 'value'"

id.string = "abcd"
-- Error: "UUID objects are immutable - cannot set property 'string'"

-- ❌ Invalid - cannot add new properties
id.custom_property = "value"
-- Error: "UUID objects are immutable - cannot set property 'custom_property'"
```

- **Only the `reset()` method can change the UUID value** - this is the intended way to modify UUIDs

---

## UUID Object Methods

**Note:** The UUID API currently only provides the `reset()` method beyond property access.

---

## Metamethods

- `tostring(uuid)` → returns a string representation:  
  `"UUID: 0x... (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)"`  

- Garbage collection (`__gc`) → if Lua owns the UUID, memory is freed automatically.

**Notes:**
- The `tostring` metamethod provides a human-readable representation for debugging
- The `__gc` metamethod ensures proper cleanup when Lua garbage collection occurs
- Memory ownership is determined by the `__is_lua_owned` flag in the proxy table
- Lua-created UUIDs (via `UUID.new()`) are owned by Lua and will be freed by `__gc`
- C-created UUIDs are owned by C and will not be freed by `__gc`
- The string format includes memory address and the UUID value

---

## Complete Example

```lua
-- Create multiple UUIDs
local id1 = UUID.new()
local id2 = UUID.new()
local id3 = UUID.new()

print("UUID 1:", id1)  --> UUID: 0x... (550e8400-e29b-41d4-a716-446655440000)
print("UUID 2:", id2)  --> UUID: 0x... (a1b2c3d4-e5f6-7890-abcd-ef1234567890)
print("UUID 3:", id3)  --> UUID: 0x... (f0e1d2c3-b4a5-6789-0123-456789abcdef)

-- Verify all UUIDs are unique
local uuids = {id1.value, id2.value, id3.value}
local unique_count = 0
for i = 1, #uuids do
    local is_unique = true
    for j = 1, i-1 do
        if uuids[i] == uuids[j] then
            is_unique = false
            break
        end
    end
    if is_unique then unique_count = unique_count + 1 end
end
print("Unique UUIDs:", unique_count, "out of", #uuids)  --> 3 out of 3

-- Use UUIDs as table keys
local data_table = {}
data_table[id1.value] = "Data for UUID 1"
data_table[id2.value] = "Data for UUID 2"
data_table[id3.value] = "Data for UUID 3"

-- Access data using UUID values
print("Data 1:", data_table[id1.value])  --> "Data for UUID 1"
print("Data 2:", data_table[id2.value])  --> "Data for UUID 2"
print("Data 3:", data_table[id3.value])  --> "Data for UUID 3"

-- Reset a UUID and verify the change
print("Before reset:", id1.value)
local old_value = id1.value
id1:reset()
print("After reset:", id1.value)
print("Value changed:", id1.value ~= old_value)  --> true

-- The old key no longer exists in the table
print("Old key exists:", data_table[old_value] ~= nil)  --> false
print("New key exists:", data_table[id1.value] ~= nil)  --> false

-- Add the new UUID to the table
data_table[id1.value] = "Updated data for UUID 1"
print("New data:", data_table[id1.value])  --> "Updated data for UUID 1"

-- UUID format validation function
local function is_valid_uuid(uuid_str)
    if type(uuid_str) ~= "string" then return false end
    if #uuid_str ~= 36 then return false end
    
    -- Check format: 8-4-4-4-12 hex digits with hyphens
    local pattern = "^%x%x%x%x%x%x%x%x%-%x%x%x%x%-%x%x%x%x%-%x%x%x%x%-%x%x%x%x%x%x%x%x%x%x%x%x$"
    return uuid_str:match(pattern) ~= nil
end

-- Validate our UUIDs
print("UUID 1 valid:", is_valid_uuid(id1.value))  --> true
print("UUID 2 valid:", is_valid_uuid(id2.value))  --> true
print("UUID 3 valid:", is_valid_uuid(id3.value))  --> true

-- UUID comparison function
local function uuid_equals(uuid1, uuid2)
    return uuid1.value == uuid2.value
end

-- Compare UUIDs
print("UUID 1 == UUID 2:", uuid_equals(id1, id2))  --> false
print("UUID 1 == UUID 1:", uuid_equals(id1, id1))  --> true

-- UUID summary
print("=== UUID Summary ===")
print("UUID 1:", id1.value)
print("UUID 2:", id2.value)
print("UUID 3:", id3.value)
print("All valid:", is_valid_uuid(id1.value) and is_valid_uuid(id2.value) and is_valid_uuid(id3.value))
print("All unique:", id1.value ~= id2.value and id2.value ~= id3.value and id1.value ~= id3.value)
```
