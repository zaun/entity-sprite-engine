# Entity Lua API

The `Entity` API provides Lua bindings for working with **game entities**.  
An entity is a container for **components** (rendering, scripts, colliders, etc.), has a **position**, and can store **custom data**.

---

## Overview

In Lua, entities are represented as **proxy tables** that behave like regular Lua objects.  
They behave like objects with properties and methods accessible via dot notation.

**Important Notes:**
- **Engine-managed** - entities created via `Entity.new()` are managed by the engine
- **Automatic engine registration** - new entities are automatically added to the engine's entity list
- **Component management** - the `components` property provides a proxy table with array-like access and methods
- **Custom data storage** - the `data` property is a Lua table for storing arbitrary script data
- **Position is a Point object** - you can modify the existing point in-place or assign another `Point` (its `x`/`y` are copied, the reference is not replaced)
- **Tag system** - entities support a flexible tagging system for categorization and searching
- **Event bus integration** - helper methods for pub/sub style messaging via `publish`, `subscribe`, and `unsubscribe`

```lua
-- Create a new entity
local e = Entity.new()

-- Access properties
print(e.id, e.active, e.draw_order)

-- Modify properties
e.active = true
e.draw_order = 5
e.position.x = 100
e.position.y = 200

-- Work with components
print("Component count:", e.components.count)

-- Work with tags
e:add_tag("player")
e:add_tag("hero")
print("Has player tag:", e:has_tag("player"))
```

---

## Global Entity Table

The `Entity` table is a global table created by the engine during initialization. It provides a constructor for creating new entity objects and static methods for finding entities.

---

## Global Entity Table Methods

### `Entity.new()`
Creates a new entity and automatically registers it with the engine.  

**Returns:** `Entity` object  

**Notes:**
- **Automatically registered** - the new entity is immediately added to the engine's entity list
- **Engine-managed** - the entity is managed by the engine
- **Default values** - entity starts at position (0,0), active=true, draw_order=0
- **Unique ID** - each entity gets a unique UUID generated automatically
- **Position object** - a new `Point` object is created and assigned to the entity
- **Empty tag list** - new entities start with no tags

**Example:**
```lua
local e = Entity.new()
print("Entity created with ID:", e.id)  --> "550e8400-e29b-41d4-a716-446655440000"
print("Default position:", e.position.x, e.position.y)  --> 0, 0
print("Default active:", e.active)  --> true
print("Default draw order:", e.draw_order)  --> 0
print("Initial tag count:", #e.tags)  --> 0
```

### `Entity.find_by_tag(tag)`
Finds all entities that have the specified tag.

**Arguments:**
- `tag` → string (tag to search for)

**Returns:** Lua table containing all matching entities (1-based indexed)

**Notes:**
- **Tag normalization** - tags are automatically capitalized and truncated to 16 characters
- **Case insensitive** - "player", "Player", and "PLAYER" all match the same tag
- **Array-like result** - returns a table where `#result` gives the count and `result[1]`, `result[2]`, etc. access entities
- **Empty result** - returns an empty table if no entities have the tag
- **Safety limit** - limited to 1000 entities maximum for performance

**Example:**
```lua
-- Find all player entities
local players = Entity.find_by_tag("player")
print("Found", #players, "players")

for i, player in ipairs(players) do
    print("Player", i, "ID:", player.id)
end

-- Find all enemies
local enemies = Entity.find_by_tag("enemy")
print("Found", #enemies, "enemies")

-- Tags are normalized automatically
local heroes = Entity.find_by_tag("hero")  -- Will find entities tagged with "HERO"
```

### `Entity.find_first_by_tag(tag)`
Finds the **first** entity that has the specified tag.

**Arguments:**
- `tag` → string (tag to search for)

**Returns:** a single `Entity` if found, or `nil` if no entity has that tag.

**Example:**
```lua
local player = Entity.find_first_by_tag("player")
if player then
    print("First player entity:", player.id)
end
```

---

### `Entity.find_by_id(uuid_string)`
Finds a specific entity by its UUID string.

**Arguments:**
- `uuid_string` → string (UUID of the entity to find)

**Returns:** Entity object if found, `nil` if not found

**Notes:**
- **Exact match** - requires the exact UUID string
- **Nil return** - returns `nil` if no entity with that UUID exists
- **Performance** - efficient lookup through engine's entity list
- **Useful for references** - can store UUIDs and look up entities later

**Example:**
```lua
-- Store a reference to an entity's ID
local player_id = player.id

-- Later, find the entity by ID
local found_player = Entity.find_by_id(player_id)
if found_player then
    print("Found player:", found_player.id)
    print("Player position:", found_player.position.x, found_player.position.y)
else
    print("Player not found")
end

-- Can also use for entity references across scripts
local enemy_id = "550e8400-e29b-41d4-a716-446655440000"
local enemy = Entity.find_by_id(enemy_id)
```

### `Entity.count()`
Returns the number of live entities currently registered with the engine.

**Returns:** integer count of entities.

**Example:**
```lua
print("Entity count:", Entity.count())
```

---

### `Entity.publish(event_name, data)`
Publishes a message to all entities subscribed to the given event name.

**Arguments:**
- `event_name` → string, event/topic name
- `data` → any Lua value (table, number, string, etc.)

**Returns:** `true` on success.

**Notes:**
- Payload is internally converted to an `EseLuaValue` and delivered to subscribers.

**Example:**
```lua
Entity.publish("level_loaded", { name = "intro", time = os.time() })
```

---

## Entity Object Properties

Each `Entity` object has the following properties:

- `id` → unique UUID string (read-only)  
- `active` → boolean (read/write, controls whether entity is processed)  
- `visible` → boolean (read/write, controls whether entity is considered for rendering)  
- `persistent` → boolean (read/write, controls whether entity survives scene clears)  
- `draw_order` → integer (read/write, controls rendering order - higher values render later)  
- `position` → a `Point` object (read-only reference; assigning another `Point` copies its coordinates)  
- `bounds` → `Rect` or `nil` (read-only, local collision bounds when a collider is present)  
- `world_bounds` → `Rect` or `nil` (read-only, world-space collision bounds)  
- `components` → a **components proxy table** (read-only reference)  
- `data` → a Lua table for storing arbitrary script data (read/write)  
- `tags` → a Lua table containing all entity tags (read-only)

**Notes:**
- **ID is immutable** - cannot be changed once assigned
- **Active state** - inactive entities are not processed by the engine
- **Visible state** - `visible=false` prevents the entity from being drawn while still allowing logic
- **Persistent state** - persistent entities can be preserved across scene clears
- **Draw order** - controls rendering order (0 = background, higher = foreground)
- **Position reference** - you can modify the existing `Point` or assign another `Point` value whose coordinates will be copied into the entity's internal point
- **Components proxy** - provides array-like access and management methods
- **Data table** - persistent Lua table for storing custom properties and state
- **Tags table** - read-only table showing all current tags (1-based indexed)
- **Type validation** - active must be boolean, draw_order must be integer

**Example:**
```lua
local e = Entity.new()
print("Entity ID:", e.id)          -- UUID string like "550e8400-e29b-41d4-a716-446655440000"

-- Modify entity state
e.active = true                    -- Enable entity processing
e.draw_order = 10                  -- Set render order (higher = later)

-- Modify position (cannot assign new Point)
e.position.x = 50                  -- Set X coordinate
e.position.y = 75                  -- Set Y coordinate

-- Store custom data
e.data.health = 100                -- Custom health property
e.data.name = "Player"             -- Custom name property
e.data.inventory = {}              -- Custom table property
e.data.inventory.gold = 50         -- Nested data
e.data.inventory.items = {"sword", "shield"}

-- Add tags
e:add_tag("player")
e:add_tag("hero")

-- Access tags
print("Tag count:", #e.tags)       --> 2
print("First tag:", e.tags[1])     --> "PLAYER"
print("Second tag:", e.tags[2])    --> "HERO"

-- Access custom data
print("Health:", e.data.health)    --> 100
print("Name:", e.data.name)        --> "Player"
print("Gold:", e.data.inventory.gold)  --> 50
```

---

## Entity Object Methods

### `entity:add_tag(tag)`
Adds a tag to the entity.

**Arguments:**
- `tag` → string (tag to add)

**Returns:** `true` if tag was added, `false` if tag already exists or on failure

**Notes:**
- **Tag normalization** - tags are automatically capitalized and truncated to 16 characters
- **Duplicate prevention** - cannot add the same tag twice
- **Case insensitive** - "player" and "Player" are treated as the same tag
- **Length limit** - tags longer than 16 characters are truncated
- **Memory management** - tags are properly allocated and managed

**Example:**
```lua
local e = Entity.new()

-- Add tags
local success = e:add_tag("player")
print("Added player tag:", success)  --> true

success = e:add_tag("hero")
print("Added hero tag:", success)    --> true

-- Try to add duplicate
success = e:add_tag("player")
print("Added duplicate player tag:", success)  --> false

-- Tags are normalized
e:add_tag("lowercase tag")          -- Becomes "LOWERCASE TAG"
e:add_tag("very long tag that should be truncated")  -- Becomes "VERY LONG TAG TH"

print("All tags:")
for i, tag in ipairs(e.tags) do
    print("  " .. i .. ": " .. tag)
end
```

### `entity:remove_tag(tag)`
Removes a tag from the entity.

**Arguments:**
- `tag` → string (tag to remove)

**Returns:** `true` if tag was removed, `false` if tag was not found

**Notes:**
- **Tag normalization** - tags are normalized before removal (same as add_tag)
- **Case insensitive** - "player", "Player", and "PLAYER" all match the same tag
- **Memory cleanup** - removed tags are properly deallocated
- **Array reordering** - remaining tags are reordered to maintain contiguous array

**Example:**
```lua
local e = Entity.new()
e:add_tag("player")
e:add_tag("hero")
e:add_tag("combat")

print("Before removal, tag count:", #e.tags)  --> 3

-- Remove a tag
local removed = e:remove_tag("hero")
print("Removed hero tag:", removed)  --> true

print("After removal, tag count:", #e.tags)   --> 2

-- Tags are reordered
print("Remaining tags:")
for i, tag in ipairs(e.tags) do
    print("  " .. i .. ": " .. tag)
end

-- Try to remove non-existent tag
removed = e:remove_tag("nonexistent")
print("Removed nonexistent tag:", removed)  --> false
```

### `entity:has_tag(tag)`
Checks if the entity has a specific tag.

**Arguments:**
- `tag` → string (tag to check for)

**Returns:** `true` if entity has the tag, `false` otherwise

**Notes:**
- **Tag normalization** - tags are normalized before checking (same as add_tag)
- **Case insensitive** - "player", "Player", and "PLAYER" all match the same tag
- **Fast lookup** - efficient tag checking
- **Boolean return** - simple true/false result

**Example:**
```lua
local e = Entity.new()
e:add_tag("player")
e:add_tag("hero")

-- Check for tags
print("Has player tag:", e:has_tag("player"))      --> true
print("Has hero tag:", e:has_tag("hero"))          --> true
print("Has enemy tag:", e:has_tag("enemy"))        --> false

-- Case insensitive
print("Has PLAYER tag:", e:has_tag("PLAYER"))      --> true
print("Has Player tag:", e:has_tag("Player"))      --> true

-- Use in conditional logic
if e:has_tag("player") then
    print("This is a player entity")
end

if e:has_tag("hero") then
    print("This is a hero entity")
end

-- Check multiple tags
local is_player_hero = e:has_tag("player") and e:has_tag("hero")
print("Is player hero:", is_player_hero)  --> true
```

### `entity:dispatch(funcName, ...)`
Calls a function on all **Lua components** attached to the entity.  

**Arguments:**
- `funcName` → string (function name to call)  
- `...` → variable arguments passed to the function  

**Returns:** `true` if dispatched successfully, `false` if no function was found or executed

**Notes:**
- **Lua components only** - only affects components of type `EntityComponentLua`
- **Function search** - looks for the named function in each Lua component's script
- **Argument passing** - all arguments after funcName are passed to the target function
- **Return value** - returns true if any function was executed, false otherwise
- **Error handling** - if a component's function errors, other components are still processed

**Example:**
```lua
-- Dispatch update function to all Lua components
local success = e:dispatch("on_update", 0.016)  -- 16ms delta time
if success then
    print("Update dispatched successfully")
else
    print("No update function found in Lua components")
end

-- Dispatch custom function with multiple arguments
e:dispatch("on_damage", 25, "fire", true)  -- damage amount, damage type, critical hit
```
---

## Components Proxy

The `components` property is a **proxy table** that provides array-like access and management methods for entity components.

### Array Access
You can access components by index (1-based):

```lua
local comp = e.components[1]       -- Get first component
local comp2 = e.components[2]      -- Get second component
```

### Properties
- `count` → number of components (read-only)  

### Methods
- `add(component)` → adds a component to the entity  
- `remove(component)` → removes a component from the entity  
- `insert(component, index)` → inserts a component at a specific index  
- `pop()` → removes and returns the last component  
- `shift()` → removes and returns the first component  
- `find(typeName)` → returns a table of indexes of components of the given type  
- `get(id)` → returns a component by its ID string  

**Notes:**
- **1-based indexing** - Lua-style indexing, not 0-based
- **Component ownership** - components added to entities are managed by the entity
- **Type-based search** - `find()` searches by component type name
- **ID-based retrieval** - `get()` finds components by their UUID string
- **Array-like operations** - supports common array operations like insert, pop, shift

**Example:**
```lua
-- Add components
local sprite = SpriteComponent.new("player.png")
e.components:add(sprite)

local collider = ColliderComponent.new(50, 50)
e.components:add(collider)

local script = EntityComponentLua.new("player_script.lua")
e.components:add(script)

print("Component count:", e.components.count)  --> 3

-- Access by index
local first_comp = e.components[1]            -- First component (sprite)
local second_comp = e.components[2]           -- Second component (collider)

-- Find components by type
local sprite_indices = e.components:find("Sprite")
print("Sprite components at indices:", table.unpack(sprite_indices))

-- Get component by ID
local comp = e.components:get(sprite.id)
if comp then
    print("Found component by ID:", comp.id)
end

-- Remove component
e.components:remove(collider)
print("After removal, count:", e.components.count)  --> 2

-- Insert at specific position
e.components:insert(collider, 1)   -- Insert at beginning
print("After insert, count:", e.components.count)   --> 3

-- Array operations
local last_comp = e.components:pop()           -- Remove and return last
local first_comp = e.components:shift()        -- Remove and return first
print("After pop/shift, count:", e.components.count)  --> 1
```

---

## Automatic Lua Component Callbacks

When an entity has a **Lua component** (`EntityComponentLua`), the engine automatically calls the following functions if they are defined in the script:

- `entity_init()` → called **once** when the component is first created/initialized  
- `entity_update(delta_time)` → called **every frame** with the frame's delta time  

This allows Lua scripts to define per-entity behavior without needing to be manually dispatched.

### Example Lua Component Script

```lua
-- Called once when the entity is created
function ENTITY:entity_init()
    print("Entity initialized with ID:", self.id)
    self.data.health = 100
    self.data.max_health = 100
    self.data.speed = 50
    self.data.name = "Player"
    
    -- Add appropriate tags
    self:add_tag("player")
    self:add_tag("hero")
end

-- Called every frame
function ENTITY:entity_update(dt)
    -- Move entity to the right at speed units per second
    self.position.x = self.position.x + self.data.speed * dt
    
    -- Keep entity within screen bounds
    if self.position.x > 800 then
        self.position.x = 800
    elseif self.position.x < 0 then
        self.position.x = 0
    end
    
    -- Health regeneration
    if self.data.health < self.data.max_health then
        self.data.health = math.min(self.data.max_health, self.data.health + 10 * dt)
    end
end
```

---

## Collision Events

Entities automatically receive collision callbacks if they have **collider components**.  
The following functions are dispatched to Lua components if defined:

- `entity_collision_enter(other)` → called when collision starts  
- `entity_collision_stay(other)` → called while collision continues  
- `entity_collision_exit(other)` → called when collision ends  

**Notes:**
- **Automatic detection** - collision events are automatically detected by the engine
- **Other entity reference** - the `other` parameter is the entity being collided with
- **Component requirement** - entity must have a collider component to receive collision events
- **Function dispatch** - these functions are automatically dispatched to all Lua components

### Example

```lua
function ENTITY:entity_collision_enter(other)
    print("Collided with entity:", other.id)
    print("Other entity position:", other.position.x, other.position.y)
    
    -- Check if other entity is an enemy using tags
    if other:has_tag("enemy") then
        -- Take damage
        self.data.health = self.data.health - 10
        print("Took damage! Health:", self.data.health)
    end
    
    -- Check if other entity is a powerup
    if other:has_tag("powerup") then
        self.data.power_level = self.data.power_level + 1
        print("Power level increased to:", self.data.power_level)
    end
end

function ENTITY:entity_collision_stay(other)
    -- Called every frame while colliding
    if other:has_tag("damage_zone") then
        -- Continuous damage
        self.data.health = self.data.health - 5 * 0.016  -- 5 damage per second
    end
end

function ENTITY:entity_collision_exit(other)
    print("Stopped colliding with:", other.id)
end
```

---

## Pub/Sub: Entity Events

Entities can participate in a simple pub/sub event bus built on the engine's pub/sub system.

### `entity:subscribe(event_name, function_name)`
Subscribes an entity to an event so that a given Lua function on that entity will be invoked when the event is published.

**Arguments:**
- `event_name` → string topic name
- `function_name` → string name of a method on the entity (for example, `"on_level_loaded"`)

**Returns:** `true` on success.

```lua
player:subscribe("level_loaded", "on_level_loaded")

function ENTITY:entity_on_level_loaded(data)
    print("Level loaded:", data.name)
end
```

### `entity:unsubscribe(event_name, function_name)`
Unsubscribes a previously registered handler.

**Arguments:**
- `event_name` → string topic name
- `function_name` → string method name used when subscribing

**Returns:** `true` on success.

---

## Restrictions

- **ID is read-only** - cannot be modified:

```lua
-- ❌ Invalid - will cause Lua error
e.id = "new-uuid"
-- Error: "Entity id is a read-only property"
```

- **Position type-checked** - assigning `position` must use a `Point` object:

```lua
-- ❌ Invalid - will cause Lua error
e.position = { x = 100, y = 200 }
-- Error: "Entity position must be a EsePoint object"

-- ✅ Correct - assign another Point (coordinates are copied)
local p = Point.new(100, 200)
e.position = p

-- ✅ Also correct - modify existing Point's values
e.position.x = 100
e.position.y = 200
```

- **Components cannot be reassigned** - the components property is read-only:

```lua
-- ❌ Invalid - will cause Lua error
e.components = {}
-- Error: "Entity components is not assignable"
```

- **Data must be a table** - cannot assign non-table values to data:

```lua
-- ❌ Invalid - will cause Lua error
e.data = "string value"
-- Error: "Entity data must be a table"

-- ✅ Correct - assign a table
e.data = {health = 100, name = "Player"}
```

- **Tags are read-only** - cannot directly modify the tags table:

```lua
-- ❌ Invalid - will cause Lua error
e.tags[1] = "new_tag"
-- Error: Tags table is read-only

-- ✅ Correct - use tag methods
e:add_tag("new_tag")
e:remove_tag("old_tag")
```

---

## Metamethods

- `tostring(entity)` → returns a string representation:  
  `"Entity: 0x... (id=..., active=..., components=N)"`  

**Notes:**
- The `tostring` metamethod provides a human-readable representation for debugging
- The string format includes ID, active state, and component count

---

## Complete Example

```lua
-- Create a player entity
local player = Entity.new()
print("Created player:", player)  --> Entity: 0x... (id=..., active=true, components=0)

-- Set up player properties
player.active = true
player.draw_order = 10
player.position.x = 400
player.position.y = 300

-- Store player data
player.data.health = 100
player.data.max_health = 100
player.data.speed = 50
player.data.name = "Player"
player.data.inventory = {gold = 0, items = {}}

-- Add player tags
player:add_tag("player")
player:add_tag("hero")
player:add_tag("combat")

-- Add components
local sprite = SpriteComponent.new("player_sprite.png")
player.components:add(sprite)

local collider = ColliderComponent.new(32, 32)
player.components:add(collider)

local script = EntityComponentLua.new("player_script.lua")
player.components:add(script)

print("Player component count:", player.components.count)  --> 3
print("Player tag count:", #player.tags)  --> 3

-- Verify components
print("Sprite component:", player.components[1].id)
print("Collider component:", player.components[2].id)
print("Script component:", player.components[3].id)

-- Verify tags
print("Player tags:")
for i, tag in ipairs(player.tags) do
    print("  " .. i .. ": " .. tag)
end

-- Find components by type
local sprite_indices = player.components:find("Sprite")
print("Sprite components at:", table.unpack(sprite_indices))

-- Dispatch custom function
player:dispatch("on_spawn")

-- Create enemy entity
local enemy = Entity.new()
enemy.position.x = 200
enemy.position.y = 200
enemy.data.type = "enemy"
enemy.data.damage = 20

-- Add enemy tags
enemy:add_tag("enemy")
enemy:add_tag("boss")

-- Add enemy components
local enemy_sprite = SpriteComponent.new("enemy_sprite.png")
enemy.components:add(enemy_sprite)

local enemy_collider = ColliderComponent.new(24, 24)
enemy.components:add(enemy_collider)

local enemy_script = EntityComponentLua.new("enemy_script.lua")
enemy.components:add(enemy_script)

-- Entity management
print("=== Entity Summary ===")
print("Player ID:", player.id)
print("Player position:", player.position.x, player.position.y)
print("Player health:", player.data.health)
print("Player components:", player.components.count)
print("Player tags:", #player.tags)

print("Enemy ID:", enemy.id)
print("Enemy position:", enemy.position.x, enemy.position.y)
print("Enemy type:", enemy.data.type)
print("Enemy components:", enemy.components.count)
print("Enemy tags:", #enemy.tags)

-- Component operations
local first_comp = player.components:shift()  -- Remove first component
print("Removed first component, new count:", player.components.count)

player.components:insert(first_comp, 1)       -- Insert back at beginning
print("Reinserted component, new count:", player.components.count)

-- Tag operations
print("Player has hero tag:", player:has_tag("hero"))  --> true
print("Enemy has boss tag:", enemy:has_tag("boss"))    --> true

-- Remove a tag
player:remove_tag("combat")
print("After removing combat tag, count:", #player.tags)  --> 2

-- Find entities by tag
local all_players = Entity.find_by_tag("player")
print("Found", #all_players, "players")

local all_enemies = Entity.find_by_tag("enemy")
print("Found", #all_enemies, "enemies")

local all_heroes = Entity.find_by_tag("hero")
print("Found", #all_heroes, "heroes")

-- Find entity by ID
local found_player = Entity.find_by_id(player.id)
if found_player then
    print("Found player by ID:", found_player.id)
    print("Player has hero tag:", found_player:has_tag("hero"))
end

-- Verify final state
print("Final player components:", player.components.count)
print("Final enemy components:", enemy.components.count)
print("Final player tags:", #player.tags)
print("Final enemy tags:", #enemy.tags)
```

---

## Tag System Use Cases

### Entity Categorization
```lua
-- Categorize entities by type
player:add_tag("player")
enemy:add_tag("enemy")
item:add_tag("item")
npc:add_tag("npc")

-- Categorize by behavior
player:add_tag("combat")
enemy:add_tag("combat")
player:add_tag("movable")
enemy:add_tag("movable")
```

### Entity Selection
```lua
-- Find all combat entities
local combat_entities = Entity.find_by_tag("combat")
for i, entity in ipairs(combat_entities) do
    -- Process combat logic
    entity:dispatch("on_combat_update")
end

-- Find all movable entities
local movable_entities = Entity.find_by_tag("movable")
for i, entity in ipairs(movable_entities) do
    -- Process movement
    entity:dispatch("on_movement_update")
end
```

### Entity References
```lua
-- Store entity references by ID for later lookup
local player_id = player.id
local enemy_id = enemy.id

-- Later, find entities by ID
local found_player = Entity.find_by_id(player_id)
local found_enemy = Entity.find_by_id(enemy_id)

if found_player and found_enemy then
    -- Process interaction
    found_player:dispatch("on_enemy_encounter", found_enemy)
end
```

### Dynamic Tag Management
```lua
-- Add temporary tags
player:add_tag("invulnerable")
player:add_tag("powered_up")

-- Remove when no longer needed
player:remove_tag("invulnerable")
player:remove_tag("powered_up")

-- Check tag combinations
if player:has_tag("player") and player:has_tag("powered_up") then
    print("Player is powered up!")
end
```
