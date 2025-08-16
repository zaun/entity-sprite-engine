# ESE: Entity Sprite Engine

---

### **Project Summary**

ESE is a modern 2D game engine built on an **Entity-Component System (ECS)** architecture. This
design allows for flexible and efficient game development, where each object in your game is an
**entity** composed of one or more **components**. The engine itself is written in **C** for
performance, while all game logic and scripting are handled in **Lua**, providing a powerful and
accessible scripting environment.

### **Target Genres**

The Entity Sprite Engine is designed to be versatile, making it ideal for a wide range of classic
2D game genres. Its flexible architecture is well-suited for:

* **Arcade Games:** Fast-paced action titles like *Breakout*, *Centipede*, *Galaga*, and *Snake*.
* **Side-Scrollers:** Platformers and shooters in the style of *Commander Keen* or *Duke Nukem*
* **Top-Down RPGs:** Role-playing games with a top-down perspective, similar to classic Zelda
titles, *Gauntlet* or *Rogue*.
* **Card and Puzzle Games:** Everything from matching and sliding puzzle games to logic-based water
and laser path games.
* **Point-and-Click Adventures:** Narrative-driven games like *Space Quest*, *Monkey Island*,
or *Eco Quest*.

### **Core Concepts**

* **Entities:** The fundamental objects in your game. An entity is essentially a container for
components and doesn't have any inherent functionality on its own.
* **Components:** The building blocks of your entities. Components hold data and define an entity's
properties and behaviors. Common components include:
    * **SpriteComponent:** Handles all visual rendering, including texture, position, and animation.
    * **ScriptComponent:** Links an entity to a Lua script that defines its game logic.
    * **ColliderComponent:** Manages collision detection and physics interactions.
* **Systems:** Global managers that process all entities with a certain type of component. For
example, a `RenderSystem` would iterate through every entity with a `SpriteComponent` and draw it
to the screen.

### **Features**

* **High Performance:** Written in C, the engine core is fast and lightweight.
* **Flexible Scripting:** Use Lua to rapidly prototype and implement all your game logic, from
character movement to complex AI.
* **Modular Design:** The ECS allows you to easily add new components and systems without modifying
existing code.
* **Cross-Platform (Potential):** Designed with portability in mind.

### **Getting Started** (todo)

[Instructions on how to get started, e.g., how to clone the repository, build the engine, and run a
demo project. This section would include specific commands for building the C engine and setting up
the Lua scripts.]

### **Example: Creating a Player Entity**

This is a conceptual example of how you would build a player character.

1.  **Create an Entity:** A simple object that will represent your player.
2.  **Add a `SpriteComponent`:** Give the player a visual representation by attaching a sprite with
an animation for walking.
3.  **Add a `ColliderComponent`:** Define a collision box so the player can interact with the game
world and other entities.
4.  **Add a `ScriptComponent`:** Attach a Lua script (e.g., `player_script.lua`) that handles input,
movement logic, and health.

The engine would then use its internal systems to process these components. The `RenderSystem` draws
the player sprite, the `PhysicsSystem` handles collisions, and the `ScriptSystem` executes the
Lua logic.
