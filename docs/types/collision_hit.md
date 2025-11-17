# EseCollisionHit Lua API

`EseCollisionHit` describes a collision detected by the engine. It is exposed to Lua as a read-only object.

---

## Global `EseCollisionHit` Table

Contains constants used by hit objects:
- `TYPE` → `{ COLLIDER = 1, MAP = 2 }`
- `STATE` → `{ ENTER = 1, STAY = 2, LEAVE = 3 }`

---

## Hit Object Properties (read-only)

- `kind` → number (`EseCollisionHit.TYPE.*`)
- `state` → number (`EseCollisionHit.STATE.*`)
- `entity` → `Entity` involved in the collision
- `target` → target `Entity` (for collider-collider)
- `rect` → `Rect` of the collider (when `kind == TYPE.COLLIDER`)
- `map` → `Map` (when `kind == TYPE.MAP`)
- `cell_x`, `cell_y` → integers (when `kind == TYPE.MAP`)

All properties are read-only in Lua.

---

## Example

```lua
for _, hit in ipairs(hits) do
  if hit.kind == EseCollisionHit.TYPE.COLLIDER then
    print("collider hit rect:", hit.rect)
  elseif hit.kind == EseCollisionHit.TYPE.MAP then
    print("map hit cell:", hit.cell_x, hit.cell_y)
  end
end
```


