# Stage 4 battle port status

The original KamataEngine project is a read-only behavior reference. All models,
textures, sound files, and gameplay code used by this preview live in the SDL
project. The Stage 3 checkpoint is Git commit `4c069fd`.

## Run

Build `BiggerFISTsSDL` with the `Debug` or `Release` CMake configuration, then
run one of these commands from the SDL project directory:

```powershell
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=missile-unit
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=bomber
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=charge-unit
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=day1
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=day2
& '.\build\Debug\BiggerFISTsSDL.exe' --battle-preview=boss
```

Press Enter to start the three-second countdown, R to reset, WASD to move, and
Space to Dodge Boost. Move the mouse to look. Q/E charge an ordinary Punch for
each arm; the left/right mouse buttons charge Rocket Punch. Hold both mouse
buttons through the additional Ultimate charge and release both to fire it.
The window title reports player HP, base HP, wave, and Boss HP. Phase changes,
hits, and base damage are logged in the terminal. The ordinary
`--first-person-preview` keeps its earlier controls.

## Implementation boundary

- `CombatSession` owns the fixed-step battle phase, player/base health, dynamic
  enemies, projectiles, unique IDs, hit resolution, reset, and clear conditions.
- `EncounterDirector` reads the Day1/Day2 wave definitions and enforces counts,
  capacity, delayed replacement, and wave advancement.
- `BattleBoss` owns a simplified Boss state machine, the 20 HP core, Idle
  movement/crystal shots, distance-weighted attack choice, a takeoff/flight/
  landing Cruise relocation, proximity-triggered Spread Counter, attacks,
  and the mandatory one-HP Full Barrage damage lock.
- `FirstPersonGame` owns Dodge, Ultimate, and the player. Battle target hits are
  resolved after enemy/projectile movement so sweeps compare the same fixed
  step. Ultimate and arm interceptions resolve before projectile impacts.
- `FirstPersonScenePresenter`, `MeshRenderer`, and `CombatFeedback` read battle
  state/events and own models, tint, nonuniform scale, WAV playback, and gamepad
  vibration. Gameplay code does not include SDL, KamataEngine, or GPU types.
- `battle-assets.cfg`, `battlefield-scene.cfg`, the encounter files, and the
  combat config files define asset paths, initial arena, wave data, and tuning.
  Invalid keys, missing values, and invalid ranges stop loading with a file
  and reason; encounter errors also report line numbers.

## Verified behavior

The tests cover the 30/60/120 rendered-FPS fixed-step comparison, wave limits
and progression, battle reset/countdown, Day1/Day2 Clear, Boss core damage and
Full Barrage lock, Idle crystal fire and proximity pressure, Ultimate collision
against Boss, Charge impact, Missile
windup and turn toward the base, and Bomber lateral bomb motion. Debug and
Release CTest passed 19/19. All three single-enemy previews and Day1, Day2,
Boss previews completed 30-frame GPU startup smoke tests. Smoke tests prove
asset loading and frame rendering; they do not prove a complete playthrough.
On this machine, switching the supported SDL GPU swapchain to MAILBOX reduced
the Day1 preview's measured average render time from about 164 ms/frame to
2.5 ms/frame. VSYNC remains the fallback when MAILBOX is unavailable.

## Differences still requiring work or manual comparison

- Boss action weights and cooldowns now follow the original values, but Cruise
  target selection omits the original dead-zone hysteresis and in-flight
  retargeting. Charged-Rocket dodge, exact Boxer/Needle hit shapes, and
  crystal halo/reload logic are not yet reproduced. Idle movement, proximity
  Spread Counter, crystal shots, mandatory Full Barrage lock, and core damage
  are present.
- ChargeUnit has battlefield/base-safe movement and separate impact/miss
  braking, but its visual turn/reacquire pose and exact obstacle timing are
  simplified. Bomber movement speed and bomb oscillation match the original
  ranges; its loaded-bomb and ring visuals are still absent.
- Ultimate damage, recoil, return timing, system offline, and basic feedback
  are present. Its staged return pose and the original cockpit/emergency
  visuals are not reproduced. Complex fire and explosion effects are simple
  model/tint cues.
- Direct side-by-side checks of enemy timing, positions, model orientation,
  attack feedback, and full Day1/Day2/Boss playthrough have not been done.
- The previous 3-6 FPS GPU startup regression came from swapchain waiting in
  VSYNC mode. MAILBOX removes that measured bottleneck on this machine, but a
  full-length interactive playthrough and sustained frame-time check remain
  necessary to establish real-time playability.

Stage 4 therefore has working battle previews and automated gameplay checks,
but original parity and performance acceptance remain open. Do not treat this
preview as a completed production battle port.
