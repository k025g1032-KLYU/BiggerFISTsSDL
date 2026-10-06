# Stage 3: player combat

This milestone ports ordinary Punch, Rocket Punch, player damage, and camera
feedback from the original PC game. The original KamataEngine checkout remains
read-only. The SDL project owns its own copies of `UpperArm.obj`, `arm.mtl`,
`ARM.png`, `whoosh.wav`, `mokugyo.wav`, and `charge_hit.wav`.

## Run the previews

From the SDL project directory, build `BiggerFISTsSDL` in Visual Studio or run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --config Debug --target BiggerFISTsSDL
& '.\build\Debug\BiggerFISTsSDL.exe' --player-combat-preview
```

Use WASD to move, the mouse to look, Q/E to charge a left/right Punch, and the
left/right mouse buttons to charge a Rocket Punch. Shift plus a mouse button
selects Punch instead. Release before full charge to cancel; release after full
charge to commit. R resets the player, both arms, and the Targets. In the combat
preview only, H simulates a projectile, J a ChargeUnit impact, and K a
BossSpreadBurst impact. Damage, HP, shield layers, attack launches, and rocket
reconnections are logged to the console. `--first-person-preview` keeps the
same scene without test damage keys.

## Gameplay boundary

`FirstPersonGame` receives `GameInput` at fixed 1/60-second steps. `PunchArm`
owns each independent arm's attack state; `PlayerVitality` owns HP, shields,
and recharge; `PlayerMovement` owns displacement and impact reaction. None of
these classes require SDL, KamataEngine, or GPU types.

`CombatHitTarget` supplies a stable ID, previous/current centers, radius, and
an optional Punch radius multiplier. `FindArmHits` performs a relative swept
sphere test and returns ordered `CombatHitEvent` records. The caller applies
damage and feedback. `FirstPersonGame` maps its Targets into this interface
and records one hit per target ID for each arm attack. Future enemies should
use unique IDs for each spawned instance and supply their previous fixed-step
position. `ReceivePlayerHit` is the corresponding entry point for future
enemy/projectile attacks and returns the original-style `PlayerHitResult`.

`FirstPersonScenePresenter` reads player and arm state to place the fists and
upper-arm models. `CombatFeedback` reads per-step events to play WAVs and
gamepad vibration. `player-combat.cfg` controls combat timing, movement reaction,
shakes, and step bob. `combat-feedback.cfg` controls WAV files, gain, and rumble.
Both files are copied alongside the executable for Debug and Release builds.
Invalid or missing combat settings stop the preview with a filename/key error.

## Original parity and later integration

| Original rule | SDL stage 3 implementation |
| --- | --- |
| Early release cancels; full charge commits | Per-arm `PunchArm` state machine |
| Two arms charge and attack independently | Separate left/right `PunchArm` instances |
| Rocket travels through targets and returns | Swept hit resolver, range clip, return phases |
| Projectile removes one shield; Charge/Burst clear all | `PlayerVitality::ReceiveHit` |
| Shield regenerates one layer every 3 seconds | Fixed-step `PlayerVitality::Update` |
| Charge causes shielded knockback or unshielded hold then knockback | `PlayerMovement` impact states |
| Burst causes knockback | `PlayerMovement::StartBurstImpact` |
| Battlefield blocks displacement; the Institute circle is available when its arena is loaded | `PlayerMovement` boundary and opt-in circle resolution |
| Punch/launch/player hit shakes, walking bob | `FirstPersonCamera` |
| Fully charged fists shake visually | `PunchArm` visual offset, applied by the presenter |
| Punch swing, hit, launch, reconnect, Charge impact sounds | SDL WAV feedback |

Player defeat is exposed as an event; scene transition belongs to the later
game-flow milestone. The combat preview's Target model stands in for later
enemies. Enemy AI, boss-specific reactions, Ultimate Rocket Punch, Dodge Boost,
full HUD/vignette, gamepad input mapping, and rocket flame rendering remain
outside this milestone.

Build both configurations, run CTest, and use these smoke checks after edits:

```powershell
& '.\build\Debug\BiggerFISTsSDL.exe' --first-person-preview --smoke-test
& '.\build\Debug\BiggerFISTsSDL.exe' --player-combat-preview --smoke-test
```
