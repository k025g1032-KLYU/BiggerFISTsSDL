# Presentation parity audit

## Baseline and evidence

- Original read-only source: `KamataEngine20260622/DirectXGame`, commit
  `89de23714402d5168e11482463e03c63c0e00338` (clean checkout).
- SDL rollback checkpoint: `7dd928f3a5b191e536b7e7f9898703a3c8a662db`.
  This pass is uncommitted on top of that checkpoint.
- The copied complete cockpit OBJ is SHA-256
  `B9D1E5B2981F410C10AFDE3CC9C22FF1EA32AA28EB29F2DE59EAC49E5FEC440D`;
  its PNG is
  `BEA1C7B5F4230689DB403197DBB9373A5F84399B4AB807E663980D09C7BC1AAF`.
  The SDL MTL keeps a local valid texture path because the original MTL's
  `NEW/cockpitTexture.png` path is absent in the copied asset tree.
- Source comparison and automated checks are evidence of code and state
  alignment. They do not establish pixel or audio parity. Matching screenshots
  from the running original and an actual listening pass are still open.

## Source-to-SDL checklist

| State or feature | Original source and rule | SDL correction and evidence | Remaining verification |
| --- | --- | --- | --- |
| Model draw order | `main.cpp`: world, additive effects with read-only depth, transparent background, first-person with depth off, sprites/text | `MeshRenderer` now has matching layer-specific pipelines; layer smoke renders the full cockpit | Compare occlusion and alpha in both running versions |
| Cockpit mesh and visibility | `CombatScene` draws the complete model independently of HUD/cockpit connection | Full `cockpit.obj` replaces the cut-face variant; `Application` no longer uses tutorial connection to hide the model; presentation preview checks the first-person instance | Inspect before/after startup, F1 third-person and F2 model cycle against original |
| Cockpit/helmet startup and shutdown | `Player` and `CombatTuning` put the cockpit mount under the sinking body; the mount's local camera sink adds to the body sink | `SyncCockpit` now applies both sinks to the rendered eye and cockpit mount, with separate startup and emergency distances; the helmet follows that eye | Numeric mount tests and SDL captures at ticks 0, 120, 288 and emergency tick 520 pass; matching original frames remain open |
| Mesh materials | OBJ normals and MTL `Ka/Kd/Ks/d` supply the original material data | Loader preserves normals and material values; GPU vertex/shader path carries normal and ambient/opacity; loader tests cover parse and invalid data | Compare lighting, transparency and color space against original GPU output |
| Punch and arm HUD | Original `DrawHud` has idle gray, charging yellow, charged green, deployed orange/red and status labels | `FullGameUi` follows these states and adds original shield caution/danger colors | Compare exact text rasterization/placement at 1280x720 and 1600x900 |
| Ultimate fist/flame | Original combined fist uses its texture without yellow multiplication; `CombatColors::kMissileTail` is white | SDL combined fist and flame tint white; staged split return uses both `PunchArm` return paths and 0.75 s docking | Check formation, separation, return, dock and restart as video |
| Target frames | Original uses separate left/right colors and model-dependent target height | `TargetDisplayKind` replaces ID-prefix guesses; UI uses the rendered scene camera and interpolation | Compare target height and movement in both resolutions |
| Battle overlays | Original `DrawGameplayHud` shows countdown, Boss HP, system offline, clear/failure/retry; `DrawDamageVignette` overlays red | SDL UI now emits the same state text and red overlay | Compare exact positions/alpha and Game Over state visually |
| Curtain/title/ending | Original phase transitions use a vertically moving curtain and timed Title prompt | SDL curtain now moves vertically; Title blinks and Ending has return prompt | Compare duration and page transitions against original |
| Observation view | Original V/R3 cycles observation and driving alignment | `FirstPersonCamera` has explicit modes and Tutorial waits for alignment | Test mouse/gamepad operations in running game |
| Tutorial voice | Original waits for system voice to finish before advancing; personnel reminder has its own lifetime | Tutorial confirmation waits on feedback voice state; skip/phase changes stop voice; no-device path can continue | Listen to the whole Tutorial and verify timing on real audio hardware |
| System and battle audio | Original ducks rocket launch on shutdown, fades offline cue before restart and owns engine/one-shots by scene | SDL applies launch ducking, early restart, offline fade, pre-refills engine loop and clears battle voices on phase exit; Boss Charge launch now emits the original RocketShot cue once per dash | Listen through Title to Ending; verify device-less and gamepad behavior |
| Boss hit audio and damage overlay | Original Boxer/Needle strikes use `sharp hit.wav`; ordinary projectile hits have no impact clip; the final Needle hit uses the Charge impact clip. Red overlay appears only when HP decreases. | Player-hit feedback now carries every hit and its Boss sharp-hit source without changing the damage type, uses the original shield/HP volumes, and limits the overlay to HP loss with the appropriate projectile/Charge timing. | Listen to all hit types on hardware and compare the overlay at matching states |
| Boss strike sound timing | Original Boxer starts the strike sound when windup begins, and each Needle Combo strike starts one sound at its own windup | `BattleBoss` exposes the strike-start event and `CombatSession` emits one `BossStriking` cue at that point, before the collision window; tests check Boxer and all three Needle strikes | Compare audible timing against the original attack animation |
| ChargeUnit wall impact | Original keeps Charging through the wall-contact step, applies swept player collision, then reacquires on the next step; a battlefield wall hit plays `charge hit.wav` at 0.75 gain | SDL now retains the collision step, emits `ChargeWallImpact` only for a battlefield wall, and changes state on the following step; the combat test checks event and transition order | Listen to the wall hit and compare moving-player contact at the wall |
| Boss cues and defeat effects | Original crystal riser starts when launch begins; death launches upward, bounces, pauses, has repeated small explosions and one final explosion | SDL follows the same staged death movement and event sequence; `BattleBossTests` checks rise/bounce/explosion and `CombatSessionTests` checks event counts | Compare launch direction, timing and visible effect placement against the running original |
| Boss/enemy poses and visual effects | Original `Boss`, enemy and effect managers drive local poses and OBJ effects at fixed 1/60 s; Full Barrage turns up, ascends, travels, dives, turns to Spread, fires, then changes to ChargeForm | SDL tracks needle poses separately from modes and uses the same ordered Full Barrage phases; Boxer and Needle Combo collision tips now follow their animated cone roots; Charge uses the original cone-tip and body-contact volumes | Capture matching original attack/death frames |

## Repeatable checks

1. Build Debug and Release, then run every CTest configuration. `BattleReplayTests`
   must clear Day1, Day2 and Boss through input/collision at 30, 60 and 120
   rendered FPS. `PlayerCombatTests` checks the split-return-to-restart cycle.
2. Run `BiggerFISTsSDL.exe --presentation-preview=boot --presentation-tick=0`,
   repeat at ticks 120 and 288, and run
   `BiggerFISTsSDL.exe --presentation-preview=cockpit`. Add `--smoke-test`
   for the 30-frame GPU check. Add `--presentation-capture=FILE.bmp` to
   save the first rendered frame and inspect the actual GPU output. The
   preview is static and does not replace
   playing through the actual Tutorial and emergency restart.
   For a battle preview, use `--capture-frame=FILE.bmp` and optionally
   `--capture-at-frame=N --smoke-test --smoke-frames=M` with `N < M`.
   For example, Boss at rendered frame 120 is captured with
   `--battle-preview=boss --capture-frame=FILE.bmp --capture-at-frame=120`
   `--smoke-test --smoke-frames=180`. This combination advances exactly one
   fixed 1/60-second simulation step per rendered frame; frame 0 is the first
   rendered frame after one update. With the same data, Debug and Release
   frame-120 Boss BMP files have the same SHA-256 hash.
   For scripted combat states, add `--replay-input=FILE` to the fixed-frame
   capture command. The included `presentation-combat.replay` is copied beside
   the executable. For example, from the project root:

   `build/Debug/BiggerFISTsSDL.exe --player-combat-preview
   --replay-input=build/Debug/presentation-combat.replay
   --capture-frame=build/presentation-combat.bmp --capture-at-frame=45
   --smoke-test --smoke-frames=180`

   Script ticks start at 0. `set T leftRocket 1` holds that control from tick
   T until `set T leftRocket 0`; `press T projectileHit` emits a one-tick
   action; `mouse T look X Y` applies a one-tick mouse delta. The sample
   charges left Rocket at tick 10, releases at 55, applies a test projectile
   hit at 80, charges right Punch at 100, and resets at 165. Other held
   controls: `leftPunch`, `rightPunch`, `rightRocket`, `shift`, `moveX`,
   `moveY`. One-tick actions: `start`, `confirm`, `skip`, `skipPhase`,
   `switchView`, `dodge`, `reset`, `chargeHit`, `burstHit`. Axis values are
   -1..1. Invalid scripts fail with a path and line number. A replay is only
   accepted with `--smoke-test`, `--capture-frame`, and `--capture-at-frame`.
   `practice T BossSpawn` and the other `PracticeCommand` enum names replay
   one practice command at tick T. The included
   `presentation-full-game.replay` enters Tutorial through Title and the
   Prologue skip, reaches free practice, then enables practice data, spawns
   Boss and triggers system offline. With `--smoke-frames=600`, capture at
   tick 240 for startup, 490 for free practice or 520 for system offline.
   It also begins the practice Boss Full Barrage at tick 590. With
   `--smoke-frames=840`, capture tick 740 for its landing/firing state.
3. Run all existing `--battle-preview` modes, first-person and full game at
   1280x720 and 1600x900. Capture the same source state in the original
   executable for visual comparison. Record every mismatch with phase, tick,
   resolution, source file/rule, SDL screenshot and correction.
4. Listen to a full Title-to-Ending playthrough and perform the original/SDL
   voice and effect comparison. A WAV hash or queued-audio test does not prove
   an audible match.

## Known limits

- Final local verification after the Boss strike timing and ChargeUnit wall
  feedback changes: Debug and Release builds succeeded; CTest passed 25/25
  in each configuration. The full-game integration test player now changes
  lateral direction every 120 ticks so it survives the original collision
  step retained at a ChargeUnit wall impact.
  Full game, first person, player combat, MissileUnit, Bomber, ChargeUnit,
  Day1, Day2 and Boss each passed a 30-frame GPU smoke run in both builds
  after the final changes.
  Boot and cockpit presentation previews also passed before the final
  hit-feedback change, which did not alter rendering. The existing
  input-driven replay covers Day1, Day2 and Boss Clear at 30/60/120 rendered
  FPS; `FullGameIntegrationTests` covers Title through Ending and return.
- Saved SDL GPU captures are in `build/presentation-boot-0.bmp`,
  `build/presentation-boot-120.bmp`, `build/presentation-boot-288.bmp`,
  `build/presentation-cockpit.bmp` and
  `build/presentation-cockpit-1600.bmp` and
  `build/presentation-boss-tick-80.bmp` and
  `build/presentation-boss-tick-120.bmp`. Deterministic Boss captures are
  `build/presentation-boss-fixed-Debug.bmp` and
  `build/presentation-boss-fixed-Release.bmp`. The build directory is ignored
  by Git.
- SDL captures at boot ticks 0, 120 and 288 and completed cockpit were inspected
  at 1280x720; completed cockpit was also inspected at 1600x900. They show
  the complete cockpit frame, tutorial board and scaled HUD, but they have no
  matched original frames yet.
- SDL Boss preview frames 80 and 120 were inspected: the entrance ChargeForm
  starts with a compact cone silhouette, then changes to the spread silhouette
  after landing. These are SDL-only observations, not an original/SDL match.
- The scripted player-combat GPU run at tick 45 produced an identical BMP in
  Debug and Release (SHA-256 `BC31BA537D0921526D1DCF133C219DAD6A1D3A3B1C1C5E7F73E724C912098A08`).
  The no-input tick-45 BMP hash differs. The tick-56 capture shows the launched
  arm, and the run log records the projectile hit at tick 80. These checks
  confirm replay input reaches the fixed simulation and renderer; they do not
  establish original-game visual parity.
- The SDL and original `LfistTEST/Lfist.png` files have the same SHA-256
  (`77B3A66F93FE9B5ECA271264F6941BEE8B3FD0F9D30BE591D1E89E4642B67DB7`).
  The texture itself has large black regions, so the dark fist surface in SDL
  is not evidence of a mismatched copied texture. Lighting and UV placement
  still require a matched original frame.
- A full-game replay reached Title, Prologue, Tutorial startup, free practice
  and system offline. SDL captures are
  `build/presentation-full-boot-240-Debug.bmp`,
  `build/presentation-full-practice-490-Debug.bmp` and
  `build/presentation-full-offline-520-Debug.bmp`. The practice tick-490 BMP
  matched in Debug and Release (SHA-256
  `3E7D62A2DEE80E7055E4D4FCE3E8B3CEEFB964D2DD10C3F6EF94888C1F8E0F31`).
  These are repeatable SDL states; original screenshots remain outstanding.
- The practice Boss Full Barrage tick-740 capture is
  `build/presentation-full-barrage-740-Debug.bmp`. Debug and Release BMPs
  matched (SHA-256
  `017FFE3A0C98284A98049482FF437C2185BF8396BC7437B7D44D14749D68188A`).
  The source-based Boss test verifies positive pitch before ascent, negative
  pitch before landing, Spread before the first shot, all ten shots at the
  deployment height, and ChargeForm after firing. This validates sequence
  and timing rules without claiming an exact original image match.
- Fixed-frame GPU capture now accepts scripted held and one-tick game input.
  The included sample was verified for Rocket charge/launch and simulated
  player damage in player-combat preview. Scripted full-game Game Over and
  emergency restart captures remain to be authored and checked against the
  original executable.
- Exact frame-by-frame original comparison and full audio listening have not
  been completed in this pass. No pixel-parity claim is made.
- The renderer retains the original-style material inputs but does not emulate
  every KamataEngine lighting detail. This needs visual calibration, not a new
  lighting system.
- Detailed Boss winding and recovery poses, gamepad hardware rumble, and exact
  moving effects remain subject to in-game comparison. Particle systems and
  Blender skeleton animation are outside this pass.
- The `presentation-boxer.replay` script reaches the free-practice Boxer mode
  through Title and Tutorial. Its tick-700 SDL GPU capture is
  `build/presentation-boxer-700-Debug.bmp`; Debug and Release captures match
  (SHA-256 `AD0B05E50D93D2A8DBE759E933B708C331B3221DEBFEF2388819DE3FB9281403`).
  The Tutorial board obscures part of the Boss, so numeric pose tests are the
  current evidence for the Boxer path.
  A matched original capture and a less obstructed angle are still needed.
- The `presentation-needle.replay` script reaches the free-practice Needle
  Combo. `BattleBossTests` checks all three pullback paths and the ordered
  strike-root motion. Debug and Release tick-650 GPU captures match (SHA-256
  `70D4F2F3515B5358F1F283E8ED4E20E38C17A9AF441EF2D9DB8137FCD49F1A9A`).
  The Tutorial board still obscures much of the Boss in the fixed camera
  capture, so exact original-frame comparison remains open.
- Full Barrage's movement and attack order now follows the source sequence.
  Exact visual alignment of the Boss parts and crystal effects still needs
  corresponding frames from the running original executable.
- Needle Combo collision now uses the active cone's fixed-step world root plus
  the original 2.2-unit forward tip offset. The former short, centered strike
  path differed from that root by about 16.3 world units at the first strike.
  The previous tip remains continuous across the three strike windows; the
  finisher sweeps toward the original Boss-forward target on the next fixed
  update. The third strike switches to ChargeForm before rendering, matching
  the original post-hit transition. `BattleBossTests` checks the active tip
  against the cone pose and checks sweep continuity across strike IDs. Debug
  and Release each passed all 25 CTests and the 720-frame Needle GPU replay.
  The tick-650 screenshot still has the Tutorial board in front of the Boss;
  an unobstructed original/SDL frame comparison remains outstanding.

- Boxer collision now follows the active cone's pullback/strike position and
  points its 2.2-unit tip toward the strike target. The first strike test
  checks the event center against the rendered part root at every active
  fixed step, including the target endpoint fallback direction. It also
  retains the swept-moving-player test and windup/recovery pose checks.
- Boss Charge now has the original turn, pullback, charge, impact and wall
  impact sequence, including up to three charges when the tip misses. The
  damaging sweep uses the cone tip 6.4 units ahead of the core with a 0.65
  radius; separate core contact applies a short push without damage. Source
  values are in `boss-combat.cfg` and `player-combat.cfg`. Tests check tip
  and core event geometry, one-hit registration, repeat and stop rules, and
  player knockback. An input-driven practice test checks one shield-breaking
  tip hit and return to Idle. Its launch sound fires once when the charge
  begins, following `Boss::DidBeginConeChargeThisFrame` in the original.
  `presentation-charge.replay` reaches the practice charge from Title and
  Tutorial. The tick-660 SDL capture is
  `build/presentation-charge-660-Debug.bmp`; Debug and Release match (SHA-256
  `7B8A8F743D6AD054A074DE2CD20D50EFCA53EB55307ED708767B5E36A7B70CDC`).
  Both 720-frame runs complete and record one shield-breaking hit. The board
  partly obscures the Boss, as it does in the original Tutorial scene.
  Exact original camera and audible sound parity remain unverified.
- A further source comparison found two player-hit feedback differences:
  Boxer/Needle contact lacked the original `sharp hit.wav`, and the SDL red
  overlay appeared when shields absorbed damage. The hit source is now
  carried to the audio layer; ordinary projectile hits remain silent, the
  Needle finisher uses Charge impact, and only HP loss starts the overlay.
  Multiple hits in one fixed step retain their individual sound sources.
  The Boss sharp-hit shield/HP gains are 0.45/0.65 in
  `combat-feedback.cfg`, matching the original tuning values.
  The tick-700 post-hit SDL capture shows the shield effect without a red
  HP overlay; Debug and Release match (SHA-256
  `FEE30FE1080D2BA746C5F3A72A27D01A4C2AFDC157652EC46C651D53580E0B5A`).

## Boss calibration pass 1: source geometry and action timing

This pass compared `DirectXGame/Game/Boss.cpp` and `Game/CombatTuning.h` with
the SDL Boss and its presentation hierarchy. The original checkout was read
only. It establishes a numeric baseline before the deferred effect and final
image comparison pass.

- The root, core, three cone roots, six Spike slots and four Cube slots keep
  the original parent order. The cones and crystals use Boss-local transforms
  before the Boss world transform. `BattleReplayTests` checks the resulting
  core/cone/crystal world origins at spawn and during Needle Strike.
- `BattleBossTests` checks the source values for Spread, Needle Strike,
  Charge Form and Cruise cone poses, core pitch, crystal halo radius/back
  offset, and wrapped halo angle. These are numeric checks; the textured
  original and SDL images have not yet been matched frame by frame.
- Boxer now keeps the original 8-second entry timeout. Its 8.5-second combat
  duration begins with the first strike, and it exits to Cruise after a
  completed strike. The source's Approach, Orbit and bounded Closing Step
  movement use the same distance, speed, duration and cooldown values from
  `boss-combat.cfg`. The parser rejects contradictory Boxer distances.
- The existing Boxer and Needle tests cover active cone tips, windup, strike
  sweeps, recovery and repeated hit control. Charge and Full Barrage tests
  cover their ordered fixed-step phases and movement. Day1, Day2 and Boss
  still Clear using normal input and collision at 30, 60 and 120 render FPS.
  The replay agent now reacts to the Boxer windup cue before dodging, because
  the longer source timing exposes multiple strikes.
- Debug and Release both built. Each passed 25/25 CTests and the full game
  plus eight preview GPU smoke modes (30 rendered frames each). These checks
  confirm the implementation and regression behavior, not final visual
  equivalence. An unobstructed original/SDL matched-frame and motion-video
  comparison remains for the effects-complete calibration pass.
