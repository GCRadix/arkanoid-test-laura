# Arkanoid
 A small 2D game with audio

Controls

  Menu
  
	W - up
	S - Down
	Enter - Select
	Escape - Quit

  Game
  
	W - Release Projectile
	S - Bullettime
	A - Left
	D - Right
	Escape - Pause

Features:
	
	- Hitting multiple tiles concecutively will play rewarding special sounds.
	- Panning and Attenuation are relative to player position.
	- Bullettime will slow down time for projectiles and bring it back to normal 
	  over a duration of 8 seconds. Beware though, everything will be a little faster than beofre after each bullettime.
	- HighScore is set after GameOver, so it's not updated when pausing.

	## Building

The project is a Visual Studio solution. There is no CMake setup.

### Requirements

- Windows 10 or 11
- One of:
  - Visual Studio 2022 with the **Desktop development with C++** workload
  - JetBrains Rider with the Visual Studio C++ build tools (MSVC) installed

### Build and run

1. Clone the repository.
2. Open `Arkanoid.sln` in Visual Studio or Rider.
3. Select the `Debug` or `Release` configuration and the `<x64>` platform.
4. Build the solution.
5. Run it from the IDE.

### Troubleshooting

- **Missing assets on startup:** the game loads its assets relative to the working directory. Start it from the IDE, or set the working directory to the project folder (Visual Studio: *Project Properties → Debugging → Working Directory*).
