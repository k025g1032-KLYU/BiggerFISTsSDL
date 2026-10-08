# Stage 5: full game flow and interface

The SDL executable now starts at Title when launched without preview arguments.
The KamataEngine project remains a read-only reference. Required images, sounds,
and the cockpit model are copied into this project and into each CMake build
configuration. The copied cockpit model has a front-facing variant with reduced
opacity so the scene remains visible through the original texture.

## Run

Build `BiggerFISTsSDL` in Visual Studio and run the executable without arguments:

```powershell
& '.\build\Debug\BiggerFISTsSDL.exe'
```

The existing `--first-person-preview`, `--player-combat-preview`, and all six
`--battle-preview=...` arguments keep their earlier behavior. `--smoke-test
--smoke-frames=30` renders a bounded full-game startup check. The executable
reads assets and configuration next to itself, so its working directory does
not have to be the source directory.

## Controls

| Context | Keyboard and mouse | Gamepad |
|---|---|---|
| Title / retry | Enter | Start |
| Story | Space or Enter to advance; K to skip | A to advance; Start to skip |
| Tutorial / battle | WASD move; mouse look; Q/E punch; left/right mouse Rocket Punch; Space dodge | Left/right sticks; triggers punch; shoulders Rocket Punch; A dodge |
| Camera view | V | Right stick click |
| Phase skip | F4 in Tutorial, Day1, Day2, and Boss | — |

F4 first enters Tutorial free practice, then advances to Story1 on a second
press. Day1 and Day2 connect through Story2 and Story3; Story3 leads directly
to Boss. A completed Ending returns to Title after confirmation.

## Boundaries and data

- `GameFlow` owns only the phase and story presentation state. It receives
  `GameInput` and battle results at a fixed 1/60-second step.
- `TutorialSession` owns input gates and progress. It observes player state and
  hit events, and does not access SDL or GPU resources.
- `CombatSession` continues to own Day1, Day2, and Boss combat. The application
  creates one session per battle phase; the UI reads its state.
- `UiDrawList` holds image, rectangle, and font commands in 1280 x 720 design
  coordinates. `UiRenderer` draws them after the 3D pass with aspect-preserving
  scaling. Story images use a dedicated 500-pixel clipping region.
- `config/story-pages.cfg` supplies story page dimensions, scroll positions,
  dialogue counts, and transition speeds. Missing or malformed required files
  stop startup with a path and reason.
- `CombatFeedback` shares one audio device among combat sounds, BGM, and
  Tutorial voice, and owns the hot-pluggable gamepad used by input and rumble.
  Audio playback and rumble are optional at runtime.

## Verification

`GameFlowTests` covers story progression, skips, retries, and rendering-rate
independent fixed updates. `TutorialSessionTests` covers all six checks and
free practice. `FullGameIntegrationTests` replays normal input and collisions
through Day1, Day2, Boss, Ending, and back to Title. Stage 4 battle replay tests
continue to check 30/60/120 FPS equivalence. `CombatSessionTests` also checks
Game Over, Enter/Start retry, and restored player/base health. Run the complete
suite with:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build -C Debug --output-on-failure
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build -C Release --output-on-failure
```

On 2026-10-08, Debug and Release builds succeeded, and all 23 CTest tests
passed in each configuration. Full-game GPU smoke tests passed in both builds
and from a copied directory with no original-project path. All eight legacy
preview modes and the multi-model preview passed their GPU smoke tests before
the final window-title-only change. The copied build also started with an
unavailable audio device and continued without sound. Removing a required
story image caused startup to fail with its file path, as intended.

Screenshots were inspected at 1280 x 720 and 1600 x 900 for Title, story,
Tutorial, battle HUD, and Ending. A scripted F4/K route reached Ending, then
Title again in the phase log. The background key-injection screenshot script
was inconsistent when capturing the returned Title; the deterministic flow
test verifies that transition. Game Over visuals were not captured in a live
GPU session. No physical gamepad was available to verify unplug handling or
rumble, and audible BGM/voice transitions were not listened to directly.

## Presentation differences

The original story images, Title image, font atlas, HUD placement reference,
cockpit texture, BGM, and Tutorial voice are used. HUD geometry is drawn with
SDL rectangles and text; it does not yet reproduce every original color pulse,
animation, and lock-target selection priority. The cockpit uses a translucent
front-facing copy of the original mesh to keep combat readable with this
renderer. Startup, game flow, and Tutorial transitions are code controlled;
Blender skeletal animation and the detailed fire/explosion effects remain
outside Stage 5.
