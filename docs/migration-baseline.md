# PC game migration baseline

## Saved starting points

The original game and the SDL port are separate Git repositories. At the start
of this migration round, both working trees were clean:

| Project | Branch | Commit | Role |
|---|---|---|---|
| `KamataEngine20260622/DirectXGame` | `remake_v0.1` | `89de237` | Read-only behavior reference |
| `BiggerFISTsSDL` | `v0.35` | `9caf085` | Saved SDL starting point |

The SDL commit already contains the first-person Target and two-fist preview.
The work after this baseline is in the SDL repository only. The copied source
under `reference/KamataEngine/Game/` is reference material, not a build input.

## Rebuild and startup checks

- Original: `MSBuild.exe DirectXGame.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m` passed on 2026-10-06. Its executable stayed running for a three-second startup check. This check did not verify its visuals or controls.
- SDL starting point: Debug/x64 build passed, and CTest passed 10/10 before this round's code changes.
- SDL first-person check after the fixed-step change: Debug/x64 build passed, CTest passed 11/11, and `BiggerFISTsSDL.exe --first-person-preview --smoke-test` rendered 30 GPU frames and exited successfully.

## Behavior and visual comparison list

| Area | Original game reference | SDL status after this round | Next comparison |
|---|---|---|---|
| First-person movement and look | `Player`, `CameraController`; W/A/S/D and mouse | Player movement, turning lag, camera follow and mouse look present | Compare travel distance and camera motion in the same test scene |
| Left and right Punch | Independent `RobotArm` state machines; Q/E and Shift + mouse | Independent charge, cancel, punch and recovery present | Compare charge, swing and recovery timing by observation |
| Target | `DummyTarget` and swept collision | Static Target, one-hit destruction and R reset present | Compare hit distance and hit timing |
| Rocket Punch and ultimate | `RobotArm`, `UltimateRocketPunch` | Not migrated | Port original states and hit rules |
| Battlefield, enemies and projectiles | `CombatScene`, `EnemyManager`, `ProjectileManager` | Not migrated | Add dynamic scene instances and one enemy type at a time |
| Waves and boss | `EncounterDirector`, `EncounterCatalog`, `Boss` | Not migrated | Verify each encounter and boss transition |
| Title, story and ending | `GameManager`, `CinematicController` | Not migrated | Compare phase transitions and visual assets |
| HUD, audio and controller feedback | `GameHUD`, Kamata audio and XInput | Not migrated | Add SDL presentation and audio adapters |

For a visual baseline, the SDL first-person preview must show one static Target
near the center and two fist models near the lower sides of the view. The
original game contains additional battlefield, UI and story visuals that have
not yet been compared side by side. Capture matched screenshots during each
feature's migration; do not claim visual parity from a successful smoke test.

## Current first-person controls

| Input | SDL first-person preview |
|---|---|
| W/A/S/D | Move relative to camera direction |
| Mouse motion | Look around |
| Q/E | Hold to charge left/right Punch; release after full charge |
| Shift + left/right mouse | Charge left/right ordinary Punch |
| R | Restore Target and reset both arms |
| Space | Change background color |
| F1/F2, F8, F11 | Window size, display and fullscreen controls |
| Escape | Exit |

The original's game flow and combat controls are defined in `GameManager.cpp`
and `CombatScene.cpp`. Only the controls listed above are currently available
in the SDL first-person preview.
