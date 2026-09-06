# Scooby-Doo: First Frights! Practice Mod

This Project is a Practice Mod for Scooby-Doo: First Frights! PC that implements features useful for glitch hunting and speedrunning practice.

## Features

Currently, These Features are only available for player 1, but they will be implemented for player 2 soon as well.

### Player Coords and Physics
- Coordinate Display
- Save Coords (Hotkey on F1)
- Load Coords (Hotkey on F2)
- Goto Coords
- Lock Y Coord
- Toggle Gravity

### Level Info
- Level Mode Display
- Toggle Level Mode
- Scene ID Display

## How the tool works

The tool is located inside dinput8.dll. When the game calls the dinput8 library, it first looks in the folder it's located inside for `dinput8.dll`, since our .dll has the same name it executes it first, and this program uses it to render the debug window and create a thread to execute the logic. The .dll then calls the actual .dll and the game runs normally. The implementation of this tool could be done other ways but this will make the geometry debug renderer and other features that require editing functions within `Scooby.exe` much easier in the future. This also makes it so taking this `.dll` out of the folder will turn off the cheat, leaving `Scooby.exe` clean.

## Installation
Download `dinput8.dll` latest release from releases and put it in the folder that `Scooby.exe` is in.

TODO: write the guide for the build it yourself option.
## Usage

Run `Scooby.exe` like normal.

## Future Plans

### Short Term
- Add features for player 2
- Add Health Display
- Freeze Health
- Add Camera Coord Viewer

### Medium Term
- Make Better UI
- Infinite Jump Hack

### End Goal
- Toggle Collision Viewer
- Implement All Defunct Debug Menu
- Perhaps have it render as an overlay in the game rather than a separate window.

## Bugs
### Known Issues
- Goto fields don't update as the player moves until the function is used once.
- Goto fields only let you enter input at the start of the input before they are dirtied.

The following issues are just how the game works and will not be fixed until I edit the actual functions in the `.exe` or change their implementations, so where possible I will provide solutions.

- Game doesn't update coordinates until the player updates (moves, attacks, jumps, etc.).
- Turning off Gravity causes the player to infinitely respawn if they hit a respawn plane. Solution: Turn Gravity back on and wait for a while.
- Locking Y position doesn't really work and the player keeps falling. Solution: Turn off gravity.

## Bugs, Feedback, and Suggestions

If you have encounter any bugs or have feedback / suggestions for the tool going forward please let me know.

