# Stage 4: playable battle previews

The SDL project is based on checkpoint `c068249`. The KamataEngine project was
read only during this work. Day1, Day2, and Boss can each progress from Ready
through Countdown and Playing to Clear using the normal input, attack, and
collision paths. GameOver can be reset with R and restarted with Enter. These
are battle previews, not the Title/Story/Tutorial/Ending flow or final HUD.

## Run

Build `BiggerFISTsSDL` in Debug or Release and run from the SDL project:

```powershell
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=missile-unit
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=bomber
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=charge-unit
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=day1
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=day2
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=boss
```

Enter starts the countdown; R resets after Clear or GameOver. WASD moves,
Space dodges, Q/E charge ordinary punches, and the left/right mouse buttons
charge independent Rocket Punches. Hold both mouse buttons to form Ultimate,
then release to fire. The window title reports phase, HP, base HP, wave, and
Boss HP. `--first-person-preview` retains its original controls.

## Gameplay and presentation boundary

- `CombatSession` owns the 1/60 second battle update, dynamic targets,
  continuous collision, damage, phases, reset, and Clear conditions. It emits
  feedback events and never calls SDL or GPU APIs.
- `EncounterDirector` reads the three waves in each Day config, respecting
  capacity, spawn delay, replacement, and completion conditions.
- `BattleBoss` uses the original action weights, cooldowns, Cruise quadrant
  dead zones and retargeting, Boxer entry dash/orbit and strike timing, Needle
  Combo dash/strike windows, crystal slots, Full Barrage lock, and death timing.
  Boxer and Needle attack events carry the previous/current tip positions,
  radius, and per-attack ID. `CombatSession` sweeps these against the moving
  player and suppresses duplicate hits from the same attack. Missing the first
  Needle strike cancels the combo into Boxer. A hit stuns the player; the
  finishing impact damages HP through the shield, knocks the player back, and
  triggers the original delayed control shutdown.
- Six Spike and four Cube crystal slots rotate behind the Boss. Each slot
  launches, fires, reloads, or is consumed by Full Barrage. Crystal projectiles
  spawn at the matching slot and fly in 3D toward the player.
- `FirstPersonScenePresenter` draws slot models, charge cues, carried Bomber
  bombs, and simplified defeat colors. `CombatFeedback` plays copied original
  Boss/battle WAV files and rumble from battle events; player feedback uses
  player events. The original project is not referenced at runtime.
- New timing, geometry, and feedback values are loaded from
  `boss-combat.cfg` and `combat-feedback.cfg`. Invalid values report file and
  line or key. CMake copies all required WAV files into each build output.

## Verification performed

- Debug and Release builds succeeded; CTest passed 20/20 in both.
- `BattleReplayTests` drives Enter, mouse look, Rocket Punch, Ultimate,
  movement, and Dodge through normal `GameInput`. Day1, Day2, and Boss reached
  Clear at 30, 60, and 120 rendered FPS with identical fixed-step counts:
  Day1 5077, Day2 9582, Boss 4111. No direct damage call is used for these
  playthroughs. Existing direct-damage tests remain as unit tests.
- The replay also checks GameOver -> R -> Ready -> Enter -> Playing, plus
  rendered model presence during combat and appropriate model cleanup at
  Clear. Boss tests cover Boxer entry movement and finite swept windows,
  Needle dash, first-strike miss, finishing impact and three unique attack IDs,
  crystal launch/reload and all ten Full Barrage slots. Player tests cover
  Needle stun, shield-bypassing HP damage, knockback and delayed shutdown.
- All six battle previews rendered 30 GPU frames in Debug and Release. A
  1200-frame Release Boss run measured 0.12 ms update and 0.99 ms render per
  frame; a 600-frame Day2 run measured 0.10 ms update and 0.90 ms render per
  frame on the test machine. Use `--smoke-test --smoke-frames=1200` to repeat;
  the default smoke test is 30 frames. The Boss Playing window was also
  visually inspected with the copied arena, Boss, halo, and player models.
- The original `Boss.cpp` and `CombatTuning.h` were compared for the principal
  action intervals, hit radii, Cruise selection, and crystal slot values.
  The Generated/Outputs Release executable opened its Title screen when run
  with the original project as its working directory. Automated DirectInput
  key presses did not advance it to combat, so an executable side-by-side
  comparison of battle timing, positions, and sound remains unverified. This
  limitation does not replace the input-driven SDL tests.

## Deferred presentation differences

The Boss needle/root pose, Boxer dash animation, elaborate halo light,
Rocket fire, explosions, ChargeUnit turn/reacquire pose, Ultimate staged
return pose, and cockpit/emergency visuals use simpler model and tint cues.
The game flow and final HUD belong to Stage 5. These differences do not alter
the tested health, attack, wave, or Clear rules, but exact visual and audio
matching should be checked against a current runnable original build before
release.
