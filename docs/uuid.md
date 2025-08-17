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

## Global `UUID` Table

### `UUID.new()`
Creates a new UUID with a randomly generated **version 4 UUID** string.  

**Returns:** `UUID` object  

```lua
local id = UUID.new()
print("Generated UUID:", id.value)
```

---

## UUID Object Properties

Each `UUID` object has the following **read-only** properties:

- `value` → the UUID string (string)  
- `string` → alias for `value`  

### Example

```lua
local id = UUID.new()
print(id.value)   --> "550e8400-e29b-41d4-a716-446655440000"
print(id.string)  --> same as above
```

---

## UUID Object Methods

### `uuid:reset()`
Generates a new random UUID for the same object.  

**Returns:** nothing  

```lua
local id = UUID.new()
print("Old:", id.value)
id:reset()
print("New:", id.value)
```

---

## Restrictions

- UUIDs are **immutable** from Lua.  
- Attempting to assign to any property will raise an error:

```lua
-- ❌ Invalid
id.value = "1234"
-- Error: "UUID objects are immutable - cannot set property 'value'"
```

---

## Metamethods

- `tostring(uuid)` → returns a string representation:  
  `"UUID: (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)"`  

- Garbage collection (`__gc`) → if Lua owns the UUID, memory is freed automatically.  

---

## Example Usage

```lua
-- Create a UUID
local id = UUID.new()
print(id)  --> UUID: 0x... (550e8400-e29b-41d4-a716-446655440000)

-- Access value
print("UUID string:", id.value)

-- Reset it
id:reset()
print("New UUID:", id.value)

-- Use as keys in a table
local t = {}
t[id.value] = "some data"
print(t[id.value])
```
