# Tutorial and practice source parity

## Scope and source

- Original reference: `KamataEngine20260622/DirectXGame/Game/CombatScene.cpp`,
  `TutorialDirector.cpp`, `EnemyProjectile.cpp`, and `CombatTuning.h`.
  The original checkout was read only.
- The SDL checkout already had unrelated uncommitted work. This pass kept it.
- Gameplay continues at a fixed 1/60 second step. Rendering reads tutorial
  and practice state; it does not control target or projectile timing.

## Behavior checked against the original

| Case | Original rule | SDL correction |
| --- | --- | --- |
| Tutorial targets | Six Target objects at X = -4, 4, -8, 8, -12, 12; Z = 8, 8, 16, 16, 24, 24; Y = 4.55; radius = 1.25 | `TutorialEncounter` creates six matching targets and resets them on entering Punch and Rocket Punch. |
| Rocket Punch targets | Restore all targets only when all are gone, both fists have reconnected, and 0.8 seconds have passed | Removed per-target immediate respawn. The fixed-step timer runs only while all conditions hold. |
| Punch targets | Original has no automatic reset after the Punch step begins | **Intentional improvement approved for this port:** if all six are gone and either arm has not completed its Punch task, restore them after 0.8 seconds to prevent a stalled tutorial. |
| Defense projectile | A scaled Target model, radius 0.45, speed 6.5; first spawn after 1.0 seconds and later spawns after 1.2 seconds of no active projectile | Uses the Target asset at 0.36 scale and one projectile at a time. Its timer pauses during flight. The flight follows the original outbound/turn/base-run rule. |
| Defense success | An intercepted projectile or a player contact succeeds, including a contact whose damage is temporarily suppressed | `TutorialEncounter` consumes the real collision result and reports contact to `TutorialSession`. Interception and player contact produce one explosion event; a shield absorption uses 0.55 visual scale. |
| Tutorial lock frames | Hidden targets are excluded during Defense; the live training projectile can be locked | `FullGameUi` filters by tutorial visibility and projectile kind. |
| Practice damage | A spawned enemy, projectile, or Boss is damageable while the practice session remains outside the battle flow | `CombatSession` accepts normal input-driven hits in Practice while retaining the `Playing` guard for formal battles. Reset clears combat objects and hit state; practice stays in `Ready`. |

## Verification

- `TutorialEncounterTests` covers target layout, Punch recovery, Rocket
  reconnection and delay, Defense timing, collision, one-shot impact, and
  30/60/120 rendered-FPS equivalence.
- `CombatSessionTests` covers direct and input-driven practice damage, reset,
  and the formal battle `Ready` guard. `FullGameUiTests` checks lock-frame
  filtering; `BattleReplayTests` checks the training projectile asset/scale.
- Debug and Release builds and all CTest targets passed. Full game, both
  first-person modes, and all six battle previews passed GPU smoke tests.
- The scripted full-game replay reached Tutorial and free practice and produced
  a GPU capture. Matching original executable screenshots for Punch, Rocket
  Punch, Defense, and practice are still pending. Source and numeric parity
  do not establish pixel or audio parity.
- The original reduces explosion sound volume when a shield absorbs the
  projectile. The SDL tutorial currently uses its shared projectile-explosion
  cue at one configured gain; matching that per-impact volume remains an
  audio-only difference for a later listening pass.
