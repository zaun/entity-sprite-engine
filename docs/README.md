# ESE: Entity Sprite Engine

A modern 2D game engine built with C/C++ and Lua scripting, featuring an Entity-Component System (ECS) architecture with cross-platform support for macOS and Linux.

## 🚀 Features

### Core Engine
- **High Performance C/C++ Core**: Fast, lightweight engine written in C with C++ extensions for graphics
- **Entity-Component System**: Flexible ECS architecture for modular game object design
- **Lua Scripting (LuaJIT)**: Fast LuaJIT integration for game logic and rapid prototyping
- **Cross-Platform**: Native support for macOS (Metal) and Linux (OpenGL/GLFW)

### Graphics & Rendering
- **Sprite System**: Animated sprites with frame-based animation support
- **Shader Support**: Custom GLSL pipeline (glslang/SPIR-V + SPIRV-Cross for Metal) for advanced effects
- **Tilemap Rendering**: Efficient tilemap system with support for multiple map types (grid, hexagonal, isometric)
- **Camera System**: 2D camera with viewport management
- **Draw Lists**: Optimized rendering with z-index depth sorting

### Game Systems
- **Collision Detection**: Rectangle-based collision system with component integration
- **GUI System**: Style-driven GUI primitives and theming
- **HTTP Client**: Async HTTP requests exposed to Lua
- **Job Queue**: Asynchronous job queue for background work
- **Audio**: Platform audio playback (CoreAudio/AudioToolbox on macOS, OpenAL on Linux)
- **Asset Management**: Centralized asset loading for textures, sprites, maps, and scripts
- **Input Handling**: Cross-platform input state management
- **Memory Management**: Custom memory allocator with garbage collection for Lua

### Platform Support
- **macOS**: Native Metal rendering with Cocoa window management
- **Linux**: OpenGL rendering with GLFW window management
- **Asset Loading**: Platform-agnostic filesystem abstraction

## 🏗️ Architecture

### Entity-Component System
ESE uses a true ECS architecture where:

- **Entities**: Simple containers that hold components
- **Components**: Data structures that define entity properties and behaviors
- **Systems**: Process entities with specific component combinations

### Available Components
- **SpriteComponent**: Handles visual rendering and animation
- **ColliderComponent**: Manages collision detection and physics
- **MapComponent**: Renders tilemaps and world geometry
- **TextComponent**: Renders on-screen text
- **ShapeComponent**: Renders simple shapes; includes path variant
- **LuaComponent**: Links entities to Lua scripts for game logic

### Core Systems
- **RenderSystem**: Processes all visual components
- **CollisionSystem**: Handles collision detection and response
- **ScriptSystem**: Executes Lua scripts for entity behavior
- **AssetSystem**: Manages loading and caching of game resources

## 📦 Building

### Prerequisites
- CMake 3.16 or higher
- C/C++ compiler with C99 and C++11 support
- LuaJIT (vendored; built automatically)
- Linux only: OpenGL, GLFW3, GLEW, OpenAL development packages

### Build Commands

```bash
# Clone the repository
git clone https://github.com/zaun/entity-sprite-engine.git
cd entity-sprite-engine

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make
```

### Platform-Specific Builds

#### macOS
```bash
# Force macOS build (uses Metal rendering)
cmake -DFORCE_PLATFORM_MAC=ON ..
make
```

#### Linux
```bash
# Force Linux build (uses OpenGL/GLFW)
cmake -DFORCE_PLATFORM_LINUX=ON ..
make
```

## 🎮 Getting Started

### Running the Example
The `examples/simple` directory contains a working demo that showcases:

- Entity creation and component management
- Sprite rendering and animation
- Collision detection
- Tilemap rendering
- Lua script integration

```bash
# From the build directory

# macOS (app bundle)
open examples/simple/simple_demo.app

# Linux (executable)
cd examples/simple
./simple_demo
```

### Basic Entity Creation
```lua
-- Create a new entity
local player = Entity.new()

-- Add a sprite component
local sprite_comp = EntityComponentSprite.new()
sprite_comp.sprite = "game:player_idle"
player.components.add(sprite_comp)

-- Add a collider component
local collider_comp = EntityComponentCollider.new()
collider_comp.rects.add(Rect.new(0, 0, 32, 32))
player.components.add(collider_comp)

-- Add Lua script for behavior
local script_comp = EntityComponentLua.new()
script_comp.script = "player.lua"
player.components.add(script_comp)
```

### Asset Loading
```lua
-- Load sprite atlas
if asset_load_atlas("game", "player.json", true) then
    print("Player sprites loaded successfully")
end

-- Load map data
if asset_load_map("game", "level.json") then
    print("Level map loaded successfully")
end

-- Load and set shaders
if asset_load_shader("game", "shaders.glsl") then
    set_pipeline("game:vertexShader", "game:fragmentShader")
end
```

## 📁 Project Structure

```
ese/
├── src/
│   ├── core/           # Engine core, asset management
│   ├── entity/         # ECS implementation
│   ├── graphics/       # Rendering and sprite systems
│   ├── platform/       # Platform-specific code (macOS/Linux)
│   ├── scripting/      # Lua engine integration
│   ├── types/          # Core data structures
│   └── utility/        # Helper functions and data structures
├── examples/
│   └── simple/         # Working demo application
├── docs/               # API documentation
└── CMakeLists.txt      # Build configuration
```

## 🔧 Development

### Adding New Components
1. Create component header and implementation files in `src/entity/components/`
2. Implement required component interface functions
3. Add Lua bindings in the component's Lua integration file
4. Register the component type in the Lua engine

### Platform Abstraction
The engine uses platform-specific implementations for:
- Window management
- Rendering (Metal on macOS, OpenGL on Linux)
- Filesystem operations
- Time and input handling

### Memory Management
- C-style memory management for engine core
- Automatic garbage collection for Lua objects
- Custom memory allocator with safety checks

## 📚 Documentation

Detailed API documentation is available in the `docs/` directory:
- `global.md` - Global engine and Lua APIs
- `entity.md` and `entitycomponent.md` - Entity and component system
- `display.md` and `gui.md` - Rendering/display and GUI
- `map.md` and `mapcell.md` - Map and map cell systems
- `point.md`, `rect.md`, `vector.md`, `ray.md`, `tileset.md`, `uuid.md` - Core types

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request
