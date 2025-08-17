# Lua Support - Engine API Reference (Internal Docs)

This section outlines the primary Lua functions and objects available for scripting within the engine. All engine-defined objects (`Entity`, `Point`, `Rect`, `UUID`, `Vector`, `EntityComponentLua`, `EntityComponentSprite`) are exposed to Lua as metatables.

## Functions

  * **`print(String message)`** Output a string to the engine's console.

  * **`asset_load_atlas(String path)`** Returns Boolean. Loads a sprite atlas from the resource directory into the default group. Returns `true` on success, `false` on error (e.g., file not found, invalid format).

  * **`asset_load_atlas(String path, String group_name)`** Returns Boolean. Loads a sprite atlas from the resource directory into the named group. Returns `true` on success, `false` on error (e.g., file not found, invalid format).

  * **`asset_load_script(String path)`** Returns Boolean. Loads a script into the engine.

  * **`asset_load_script(String path, String group_name)`** Returns Boolean. Loads a script into the engine.

  * **`asset_remove(String group_name)`** Returns Boolean. Removes all assets in the named group. Returns `true` on success, `false` on error (e.g., group not found).

  * ** TODO: **

      * Add either an `entites` list or a set of accessor functions to access the overall engine entity list.
      * Add the ability to load JSON. I have cJSON in the project, so either expose that or something else?
      * Add an `asset_exists(String group_name)` function to check if a group is loaded.
      * Consider a `get_last_error()` function for more specific error details on Boolean returns.

---

## Objects

### Entity

  * ** Constructors: **

      * **`Entity.new()`** Creates a new entity and automatically adds it to the engine's active entity list.
      * **`EntityComponentLua.new()`** Creates a new Lua script component. This component allows an entity to run Lua code for its logic.
      * **`EntityComponentSprite.new()`** Creates a new sprite rendering component. This component allows an entity to be rendered visually.

  * ** Properties: **

      * **`active`** Boolean, settable, gettable. If `false`, no rendering, no updates are made and no scripts are run.
      * **`id`** UUID, gettable. A unique identifier for the entity.
      * **`draw_order`** Number, settable, gettable. Determines the rendering order of sprites (higher numbers draw on top).
      * **`position`** Point, settable, gettable. The 2D position of the entity in pixels.
      * **`components`** Collection (array-like table). Contains all components attached to this entity.
      * **`components.count`** Number, gettable. The number of components currently attached to the entity.
      * **`data`** Table, settable, gettable. User data attached to the Entity, allowing for arbitrary Lua values to be stored.

  * ** Functions: **

      * **`components.add(EntityComponent component)`** Adds an `EntityComponent` to the collection.
      * **`components.remove(EntityComponent component)`** Removes an `EntityComponent` from the collection.
      * **`components.insert(EntityComponent component, Number index)`** Inserts an `EntityComponent` at the specified index, shifting subsequent items down.
      * **`components.pop()`** Removes and returns the last `EntityComponent` in the collection.
      * **`components.shift()`** Removes and returns the first `EntityComponent` in the collection.
      * **`components.find(String componentType)`** Returns a list of indexes of matching components.
      * **`components.get(String id)`** Returns a the matching component.

### EntityComponentLua

  * ** Constructors: **

      * **`EntityComponentLua.new()`** Creates a new `EntityComponentLua` with no attached script.
      * **`EntityComponentLua.new(String script_name)`** Creates a new `EntityComponentLua` with the passed script name. The script will be loaded and executed when the component becomes active.

  * ** Properties: **

      * **`active`** Boolean, settable, gettable. If `false`, the script's `entity_update` will not be called.
      * **`id`** UUID, gettable. A unique identifier for this component.
      * **`script`** String, settable, gettable. The name of the Lua script file associated with this component. Setting this property will reload and re-initialize the script in use for this component.

### EntityComponentSprite

  * ** Constructors: **

      * **`EntityComponentSprite.new()`** Creates a new `EntityComponentSprite` with no attached sprite.
      * **`EntityComponentSprite.new(String sprite_name)`** Creates a new `EntityComponentSprite` with the passed sprite name. The sprite must have been loaded via `asset_load_atlas`.

  * ** Properties: **

      * **`active`** Boolean, settable, gettable. If `false`, the sprite will not be rendered.
      * **`id`** UUID, gettable. A unique identifier for this component.
      * **`sprite`** String, settable, gettable. The name of the sprite to be rendered. Setting this property will set the sprite with that name loaded from the atlas (or `null` if an invalid name is provided), updating the visual representation.

## Point (All coordinates in pixels)

  * ** Constructors: **

      * **`Point.new(Number x, Number y)`** Creates a new `Point` object with `x` and `y` coordinates.
      * **`Point.zero()`** Creates a new `Point` object initialized to `(0, 0)`.

  * ** Properties: **

      * **`x`** Number, settable, gettable. The X-coordinate.
      * **`y`** Number, settable, gettable. The Y-coordinate.

### Rect (All dimensions in pixels)

  * ** Constructors: **

      * **`Rect.new(Number x, Number y, Number w, Number h)`** Creates a new `Rect` (rectangle) object with `x`, `y` (top-left corner), `width` (`w`), and `height` (`h`).

  * ** Properties: **

      * **`x`** Number, settable, gettable. The X-coordinate of the top-left corner.
      * **`y`** Number, settable, gettable. The Y-coordinate of the top-left corner.
      * **`width`** Number, settable, gettable. The width of the rectangle.
      * **`height`** Number, settable, gettable. The height of the rectangle.

  * ** Functions: **

      * **`contains_point(Number point_x, Number point_y)`** Returns Boolean. Returns `true` if the specified point is contained within the rectangle, `false` otherwise.
      * **`intersects(Rect other_rect)`** Returns Boolean. Returns `true` if the passed `Rect` intersects with this rectangle, `false` otherwise.
      * **`area()`** Returns Number. The area of the rectangle in square pixels.

### UUID

  * ** Constructors: **

      * **`UUID.new()`** Generates and returns a new Universally Unique Identifier.

  * ** Properties: **

      * **`value`** String, gettable. The string representation of the UUID (e.g., "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx").

  * ** Functions: **

      * **`reset()`** Creates a new UUID value for this object.

### Vector (All components in pixels)

  * ** Constructors: **

      * **`Vector.new(Number x, Number y)`** Creates a new 2D `Vector` object with `x` and `y` components.

  * ** Properties: **

      * **`x`** Number, settable, gettable. The X-component.
      * **`y`** Number, settable, gettable. The Y-component.

  * ** Functions: **

      * **`set_direction(String direction, Number magnitude)`** Sets the vector's `x` and `y` components based on a cardinal or intercardinal `direction` (e.g., "N", "S", "E", "W", "NW", "NE", "SW", "SE") and a `magnitude`.
      * **`magnitude()`** Returns Number. The magnitude (length) of the Vector.
      * **`normalize()`** Normalizes the Vector, changing its magnitude to 1 while preserving its direction.

  * ** TODO: **

      * Consider adding common vector operations like `add(Vector other)`, `subtract(Vector other)`, `multiply(Number scalar)`, `dot(Vector other)`, `distance(Vector other)`.

---

## Globals

### InputState

The InputState object provides read-only access to the current frame's input state from keyboard and mouse devices. This is a global object and can be access from any location.

  * ** Properties: **

      * **`keys_down`** Table (read-only). Boolean array indexed by key constants. True if the key is currently being held down.
      * **`keys_pressed`** Table (read-only). Boolean array indexed by key constants. True if the key was pressed this frame (transition from up to down).
      * **`keys_released`** Table (read-only). Boolean array indexed by key constants. True if the key was released this frame (transition from down to up).
      * **`mouse_x`** Number (read-only). Current X coordinate of the mouse cursor in pixels.
      * **`mouse_y`** Number (read-only). Current Y coordinate of the mouse cursor in pixels.
      * **`mouse_scroll_dx`** Number (read-only). Horizontal scroll wheel delta for this frame.
      * **`mouse_scroll_dy`** Number (read-only). Vertical scroll wheel delta for this frame.
      * **`mouse_buttons`** Table (read-only). Boolean array indexed by mouse button numbers (0-7). True if the mouse button is currently pressed.
      * **`KEY`** Table (read-only). Contains key constants for use with the key arrays. Examples include `KEY.A`, `KEY.SPACE`, `KEY.ENTER`, `KEY.UP`, `KEY.MOUSE_LEFT`, etc.

#### Key Constants

The `KEY` table contains constants for all supported input keys:

  * **Letters:** `A` through `Z`
  * **Numbers:** `0` through `9`
  * **Function Keys:** `F1` through `F15`
  * **Modifiers:** `LSHIFT`, `RSHIFT`, `LCTRL`, `RCTRL`, `LALT`, `RALT`, `LCMD`, `RCMD`
  * **Navigation:** `UP`, `DOWN`, `LEFT`, `RIGHT`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`, `INSERT`, `DELETE`
  * **Special:** `SPACE`, `ENTER`, `ESCAPE`, `TAB`, `BACKSPACE`, `CAPSLOCK`
  * **Symbols:** `MINUS`, `EQUAL`, `LEFTBRACKET`, `RIGHTBRACKET`, `BACKSLASH`, `SEMICOLON`, `APOSTROPHE`, `GRAVE`, `COMMA`, `PERIOD`, `SLASH`
  * **Keypad:** `KP_0` through `KP_9`, `KP_DECIMAL`, `KP_ENTER`, `KP_PLUS`, `KP_MINUS`, `KP_MULTIPLY`, `KP_DIVIDE`
  * **Mouse:** `MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE`, `MOUSE_X1`, `MOUSE_X2`

#### Usage Example

```lua

-- Check if space key is currently held down
if InputState.keys_down[InputState.KEY.SPACE] then
    -- Jump logic
end

-- Check if 'A' key was just pressed this frame
if InputState.keys_pressed[InputState.KEY.A] then
    -- Attack logic
end

-- Check mouse position and left click
if InputState.mouse_buttons[0] then -- Left mouse button
    print("Clicking at:", InputState.mouse_x, InputState.mouse_y)
end

-- Check for scroll wheel movement
if InputState.mouse_scroll_dy ~= 0 then
    -- Zoom logic based on scroll direction
end
```

---

## Lua Script Structures

Engine-level errors will be logged to the console but generally will not crash or stop the script, allowing it to continue best it can.

### Lua EntityComponentLua Script

Scripts attached via `EntityComponentLua` should return a table (module) containing the following optional functions:

```lua
local EntityModule = {}

-- Called once when the entity is initially made active in the engine,
-- or when this component is attached to an already active entity.
function EntityModule:entity_init()
    -- Initialization logic for the entity goes here.
end

-- Called once per frame for the entity, allowing for per-frame updates.
-- @param delta_time (double) The time in seconds since the last frame.
function EntityModule:entity_update(delta_time)
    -- Game logic that needs to run every frame goes here.
end

return EntityModule
```

### Lua Startup Script

The designated startup script (configured in the engine). This script is the "config" or "game setup". At the moment there aren't config files. Should return a table (module) containing the following function:

```lua
local StartupModule = {}

-- Called once by the engine when it first starts up, before the main game loop.
function StartupModule:startup()
    -- Engine-wide initialization, level loading, initial entity creation etc., goes here.
end

return StartupModule
```