# DEADBEEF FACADE

## Description

After graduating from Cal Tech, you land a sweet gig as a janitor. Your boss, Ape Man, gives you the task of cleaning up the basement of the company building. To your dismay, the basement is infested by hordes of rats. With only your Zune to keep you company, you must slaughter the rats to the beat of your big chunes, your foray into the depths mirroring your slow descent into madness in a cinematic slow-burn that's sure to bring plenty of zany laughs for the whole family.

## Dependencies and Build

After cloning the repository, run

```
git submodule init
git submodule update
cd whitgl
```

Run the `get_inputs` script corresponding to your operating system. For example, for Linux type

```
./scripts/linux/get_inputs.sh
```

For MacOS, type

```
./scripts/osx/get_inputs.sh
```

Finally,

```
cd ..
./setup.sh
make
```

## Specification

### World

The world consists of multiple levels joined by elevators which can be passed through in one direction. Each level consists of a rectangular floor plan, divided into tiles, with solid walls occupying some tiles and floors that can be walked on occupying other tiles. In addition to static walls and floors, a level can also include doors. Doors overlay a floor tile and behave like walls normally, but they can be unlocked using a specific color of key picked up previously, consuming the key. After being unlocked, a door can be walked through.

### Entities

The primary entity encountered in each level is the rat. Rats are represented by a 2D billboard texture that always faces the player. Rats by default are stationary but move towards the player when they have line of sight, and deal damage when they come into direct physical contact with the player. However, rats may have one or more of the following upgrades:

| Upgrade     | Description                                                                |
|-------------|----------------------------------------------------------------------------|
| Pathfinding | The rat uses A* pathfinding to find a route to the player.                 |
| Speed burst | The rat periodically sprints towards the player briefly.                   |
| Poison dart | The rat periodically shoots a dart at the player, dealing damage remotely. |
| Spawn       | The rat periodically spawns another rat.                                   |

These upgrades will usually be reflected in a different texture for the rat. Especially difficult rats will sometimes drop keys, which are represented with a billboard texture and rest statically on the floor where they drop.

Keys overlay a floor tile and can be walked over, at which point they are picked up. Keys are used to unlock doors as previously mentioned. They may be dropped by rats or be present on the ground when a level is loaded.

Health packs overlay a floor tile and can be walked over, at which point they are used. Health packs restore 1/4 of the player's health.

### Gameplay

The goal is to pass through all the levels without dying. The player has finite health, which is depleted when attacked by rats. Gameplay is intrinsically rhythmic and tied to the music of a level. Every rat has a vulnerability beat pattern (a 2-bar loop), and the user must hit the rat by left-clicking precisely at each one of its vulnerable beats in order to kill the rat. A vulnerable beat that is not shot will remain present in the loop and loop around, and the rat will remain alive until that beat is hit. Given that the beat of the music is fixed, easy-to-kill rats will have a beat pattern that is a sparse covering of the music beat. Harder-to-kill rats will have a beat pattern that forms a more dense representation of the actual beat (including hi-hats and other more frequent and subtle percussion rather than simply kicks or other sparse, prominent elements of the beat). These beat patterns will be pre-programmed in their gradations of difficulty based on the music used.

The beat pattern of each rat is represented by a series of notes that fall from the top of the screen when that rat is targeted. The targeted rat also flashes in sync with its vulnerability beat pattern. The targeted rat is the closest rat under the player's crosshairs when the player first left clicks.

### Engine

The world is drawn using a raycasting Wolfenstein-style engine written in OpenGL. This allows the levels to be drawn in roughly constant time regardless of the size of the level. The engine is implemented using `whitgl`, a thin wrapper around OpenGL that also handles keyboard and mouse events, image loading, and sound.

### Pausing, Saving and Loading

The player can pause at any point in the game and save into one of eight save slots. This will save the player's current level, position and orientation in the level, health and keys, and all door states and entity positions and states into a file that can be loaded at any time.
