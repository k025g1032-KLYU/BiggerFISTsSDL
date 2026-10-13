# Boss, combat feedback, and effects parity pass

## Baseline and scope

- The original `KamataEngine20260622/DirectXGame/Game` tree was read only.
- Before editing, the SDL dirty working tree was copied to
  `../migration-baselines/2026-10-13-boss-effects/` as a tracked patch,
  status list, and untracked-file copies. The starting SDL commit was
  `7dd928f3`.
- The original source is the numeric and event-timing reference. This pass
  does not change damage, hitbox radii, enemy counts, or wave data.
- All effects below use existing OBJ assets and the existing additive layer.
  No particle or skeletal animation system was added.

## Source comparison and correction

| Feature | Original rule | SDL result | Evidence and limit |
| --- | --- | --- | --- |
| Crystal attack | `Boss::QueueCrystalShotRequest` raises a halo slot, then uses an 11-lane remote spawn at battlefield far Z plus 1.25; Spike and Cube have different deterministic lane orders; Cube carries its own oscillation phase; travel is toward the Base | `BattleBoss::UpdateCrystals` emits remote position, direction, and phase; `CombatSession` passes all three into the projectile. Halo slots still rise before the shot | `BattleBossTests` checks lane order, spawn height and Z, direction, and phase. A matching original screenshot remains open |
| Cruise facing | `Boss::MoveCruiseTowardDestination` faces travel while flying, then landing turns toward the player | SDL flight yaw uses the actual travel vector; landing turns toward the player by the configured rate | `BattleBossTests` checks both states numerically. A matching motion capture remains open |
| Boss defeat transition | Original `GameManager` waits for the Boss death sequence, all explosion effects, projectiles, and player flying fists | Boss and Day1/Day2 Clear now wait for the final fixed-step explosion lifetime; Game Over still has priority | `CombatSessionTests` checks that the one final explosion holds Playing for at least 58 subsequent ticks, then permits Clear. Normal input/collision replays still Clear at 30/60/120 render FPS |
| Explosion generation | Original creates explosions on projectile intercept/player/Base impact, enemy flight end, Boss core hit, Spread Burst, and small/final Boss defeat | Each point emits one visual event from `CombatSession`; enemy impact and flight-end explosion are separate events. The renderer reads the event and the fixed simulation clock | Combat and Boss tests check event counts and transitions. Exact placement/brightness against the original GPU output remains open |
| Practice shield | Original shield effect is created from applied player-hit results | Practice and normal combat now use one post-hit adapter; direct practice test damage and real enemy hits both emit the shield event once | `CombatSessionTests` checks the direct path. The actual practice Needle replay shows a blue shield at tick 750 |
| HP red flash | Original flash begins only when HP decreases; shield absorption must not start it | SDL player-hit feedback uses `didDamageHitPoints`; no change to shield-hit behavior | `FullGameUiTests` checks the overlay command; actual practice Needle replay shows no red at shield-only tick 750 and a red overlay after HP changes 5 to 4 at tick 770 |
| Ultimate formation | Original formation progress changes fist, elbow, upper arm, and inward fist roll together | The same formation position is used before fist and upper-arm pose calculations | `BattleReplayTests` checks the formation start, middle, completion, cancellation and return. Matched original video remains open |
| Rocket flame | Original has flight, pre-return fade, turn-up minimum, ascent restoration, return and Ultimate dock poses | `PunchArm` exposes read-only flame scale/dock state; presenter follows it. Ultimate, missile, and Charge flame visibility remains tied to their original active states | `RocketPunchTests` checks ready, flight, return and reconnection; a matched moving-image check is still open |

The explosion lifetime is 0.12 s flash, 0.38 s fire, and delayed 0.85 s smoke;
the final smoke disappears after 0.97 s. Gameplay owns the remaining lifetime
for Clear gating. Rendering owns the visible effect instances and advances only
on fixed updates. The render interpolation factor does not affect battle
results or the transition time.

## Full-game migration inventory after this pass

| Phase | Program and input state | Models/effects/UI/audio status | Remaining original comparison |
| --- | --- | --- | --- |
| Title | Playable start and original skip route covered by `FullGameIntegrationTests` | Original page assets, font atlas, BGM and prompt are connected | Matching title frames and actual listening |
| Prologue and Story0-3 | Page/skip/curtain transitions covered by flow tests | Source-based page and curtain timing is in SDL | Matching scroll frames and voice/BGM transition listening |
| Startup | Full, quick, Boss and reboot states covered by cockpit tests | Cockpit/helmet models, startup checks, HUD enable order and audio cues are connected | Matched startup video, opacity and audio timing |
| Tutorial and practice | Six targets, Punch/Rocket/Defense and practice spawn commands covered by tests | Target, practice projectile, enemy/Boss models and HUD are connected; this pass verified shield and HP flash in actual replay frames | Matched original target and projectile visuals; gamepad hardware |
| Day1 and Day2 | Input-driven wave Clear, Game Over and retry covered by tests | Enemy and projectile models, HUD, flame and corrected explosion lifecycle are connected | Matched battle frames, explosion placement and effect listening |
| Boss | Input-driven Clear and fixed-step behavior tests cover attacks and defeat | Core/parts/crystal models, corrected remote shots/cruise facing, final explosion wait, HUD and cues are connected | Matching cruise/crystal/death video and attack audio |
| Ending | Flow test reaches Ending and returns to Title | Ending page and prompt are connected | Matching Ending screen and music listening |

This inventory means the SDL game remains functionally playable from Title to
Ending. It does not establish visual or audible identity. A release package
from a clean folder is also outside this parity pass.

## Verification performed

- Debug and Release application builds succeeded. All 27 CTests passed in both
  configurations after rebuilding the affected test executables.
- Full game, first-person, player-combat, MissileUnit, Bomber, ChargeUnit,
  Day1, Day2, and Boss GPU modes each rendered 30 frames in Debug and Release.
- `presentation-needle.replay` rendered 771 fixed frames in Debug at 1280x720
  and 1600x900. At tick 750 only shield damage had occurred and the blue
  shield was visible. At tick 770 HP had fallen from 5 to 4 and the red
  overlay was visible. Captures are in `build/migration-baseline-20261013/`
  (`needle-750.bmp`, `needle-770.bmp`, `needle-770-1600.bmp`). The same
  display setting file was restored to 1280x720 after the 1600x900 capture.
- The Day1, Day2 and Boss input-driven Clear replays are in
  `BattleReplayTests`; 30/60/120 render-rate consistency is covered there.
- `presentation-ui-flow.replay` rendered 930 fixed frames in Debug and
  Release. Tick 870 displays Ending; the replay then returns to Title.
  Debug and Release tick-870 BMP files have the same SHA-256 hash
  (`14BE7F3E813372E1896F5F15FA0B7C3057AD70E0F89D6593B582C58FCDD17FCA`).
  This scripted route uses the stage-skip controls; the normal-input combat
  clears are verified separately by `BattleReplayTests`.

## Still requiring direct original comparison

No deterministic captures of the original executable at matching ticks were
produced in this pass. Cruise/crystal/formation and explosion positions are
source-checked and numerically tested, but original-to-SDL motion and pixel
parity remain unverified. A full Title-to-Ending listening comparison and
real-gamepad vibration check also remain. These should be done before release
packaging, alongside the clean-folder launch and playthrough.
