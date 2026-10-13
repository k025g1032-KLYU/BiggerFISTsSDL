# Enemy and projectile collision / presentation parity

## Baseline and limits

- Source reference: `../../KamataEngine20260622/DirectXGame/Game`. The original project was read only.
- SDL baseline before this pass: HEAD `7dd928f`, with pre-existing uncommitted work retained. Debug and Release CTest each passed 26/26 before editing.
- Gameplay advances at 1/60 second. The overlay shows the **current fixed-step collision state**; its projected rings are an inspection aid, not a second set of colliders.
- Status below means numerical or code comparison with the original. Original executable screenshots at matching ticks are still **pending visual verification**.

## State comparison

| Object / state | Original rule used as reference | SDL result / verification |
|---|---|---|
| MissileUnit spawn, drop, land, takeoff | Center starts at drop height; collision radius 0.9; lies on X while grounded, then stands up. | Same gameplay center/radius and transition. Presenter interpolates position. Numerical test and preview overlay checked. |
| MissileUnit patrol and reversal | Moves along X; yaw approaches patrol direction at 6 rad/s, including after reversing. | Restored gradual yaw and shortest-angle interpolation. `BattleReplayTests` checks 0, 0.5, 1 draw alpha. |
| MissileUnit fire windup and recovery | Yaw approaches forward at 8 rad/s, then patrol yaw at 8 rad/s; missile starts 1.1 units ahead. | Same timing and yaw targets. Fire-state timing already checked by `CombatSessionTests`. |
| Bomber drop, patrol, carried bomb | Collision radius 0.9; ring rotates only after deployment; loaded bomb follows body and scales back up. | Ring time begins after deployment and is interpolated. Carried bomb remains at body center. |
| Bomber firing | Bomb spawns at `GetPosition()`; oscillation phase and patrol direction continue independently. | Removed the extra -1.1 Z spawn offset. Projectile uses the existing oscillation and swept collision. |
| ChargeUnit drop, takeoff, approach, reacquire | Collision radius 0.9; model yaw offset is 1.5 pi and turns toward a target; dash uses a swept sphere. | Existing fixed-step states retained. Visual yaw interpolated; hitbox comes from simulation. |
| ChargeUnit prepare, dash, impact, braking | Prepare pulse and impact nonuniform scale; charge path and player collision use fixed-step centers. | Corrected pulse to multiply the asset's scale and restored impact multipliers 1.12 / 0.90 / 0.75. Gameplay path remains unchanged. |
| Any normal enemy defeated or removed | Launch direction follows actual fist/Ultimate sweep on XZ; horizontal speeds Punch 13, Rocket 26, Ultimate 40; upward speed 11, gravity 14; spin 10 / 13 / 8 rad/s; removal after 1 s or crossing original bounds plus margin 12. | Replaced patrol-X launch with attack-derived direction and all-axis flight. Numerical tests cover all three speeds, repeat hits, rotation and reset. |
| Missile projectile outbound, turn, base run | Radius 0.45; horizontal direction; turns toward base at boundary Z=24 using a 0.15 s blend. | Horizontal direction and prior/current facing available to rendering; 0 / 0.5 / 1 draw alpha tested during turn. Swept collision and base/player order remain in `CombatSession`. |
| Floating bomb travel, spin, interception | Radius 0.65; horizontal forward motion and lateral sine wave; rotates on all axes. | Position and age interpolate; current and previous collision centers come from simulation. Interception removes projectile without enemy defeat flight. |
| Boss crystal spike travel / interception | Radius 0.58; visual slot may be high, but `EnemyProjectile::Initialize` clamps actual projectile Y to combat range 4.2–5.1 and removes vertical travel. | Corrected SDL projectile to use the clamped, horizontal path. Existing test expecting a downward flight was replaced with original behavior. |
| Boss crystal cube travel / interception | Radius 0.70; same altitude clamp, with lateral oscillation and XYZ spin. | Uses simulation center/radius; previous/current age smooths spin. Crystal launch slot remains separate from projectile collision center. |
| Boss entrance, cruise, idle, charge, boxer, needle, barrage, defeat | Core radius 1.75. Attack events expose current/previous tip centers, active window and radius; pose has local part transforms. | Existing `BattleBoss` fixed-step rules and tests retained. Overlay draws only emitted attack events, plus core and crystal projectiles. Hierarchy and input-driven clear tests pass. |
| Punch, Rocket Punch, Ultimate hitting moving enemies | Swept fist/Ultimate vs swept target; one damage per target per attack; Boss defeat launch follows sweep. | Formal hit events now carry attack kind and previous/current positions into enemy defeat. Active player attack spheres and sweep lines appear in overlay. |

## Preview and tests

Run `build/Debug/BiggerFISTsSDL.exe --battle-preview=missile-unit --show-hitboxes` from this project, or replace the mode with `bomber`, `charge-unit`, `day1`, `day2`, or `boss`. F9 toggles the overlay. Green = enemy, yellow = enemy projectile, cyan = Boss core, red = active Boss attack, white = active player attack. Each ring shows the current collision sphere. The dim line joins previous and current fixed-step centers; the arrow shows facing or flight direction. The full game starts with the overlay disabled.

Debug and Release CTest: 26/26. All six battle previews passed a 30-frame GPU smoke run in both configurations with the overlay enabled. `BattleReplayTests` reaches Day1, Day2 and Boss Clear through normal input and collision at 30, 60 and 120 render FPS. Saved local inspection captures: `build/hitboxes-missile.bmp`, `build/hitboxes-projectile.bmp`, and `build/hitboxes-boss.bmp`.

The screenshots confirm that the SDL overlay is drawn over live enemy, missile and Boss models. They do **not** prove visual identity with the original executable. Matching-tick original captures, camera alignment, and manual inspection of every attack/death animation remain pending visual verification.
