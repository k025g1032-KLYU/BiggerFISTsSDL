# Model pose and audio alignment

The original `KamataEngine20260622/DirectXGame/Game` source is the behavior
reference and remains read only. This pass does not change combat collision,
damage, or wave rules.

## Changes

- The OBJ loader mirrors local X and reverses triangle winding. This is one
  mesh-basis conversion used by every scene model, including fists, the
  tutorial banner, and Boss cones. It preserves each OBJ origin and UVs.
- Boss core and cone poses move toward their target every fixed 1/60 second.
  The ordinary blend factors match the original `Boss.cpp::UpdateNeedlePose`:
  6 per second, or 18 during Cruise. Spread windup has a timed start and end.
  Rotation blending uses the shortest path across the angle boundary.
  Active strike positions already follow the attack timer and bypass the
  extra pose blend. Both values are in `boss-combat.cfg`.
- Rendering blends the previous and current fixed-step pose with the clock's
  remaining-time fraction. This covers camera and fists, enemy and projectile
  positions, and Boss root, cones, core and crystal halo. Rendering never
  supplies a position to collision code. Reset copies the current pose to
  the previous pose before drawing.
- `combat-feedback.cfg` maps the additional original WAVs and gain values.
  The event source emits landing, Charge prepare/dash, Missile launch, Boss
  dodge/strike/special cues, projectile explosion, footstep, shield/HP hit,
  cockpit start, system offline and restart. The engine loop starts after
  cockpit startup, fades from startup gain, and stops on offline, scene end,
  or battle end. Missing WAVs and bad cue lines report the path and line.

## Checks

- `ObjLoaderTests` checks the converted mesh basis and winding, plus the
  original tutorial quad's left-to-right UVs after its 180-degree Y rotation.
- `BattleBossTests` checks the fixed-step core midpoint, Spread midpoint and
  shortest angle, reset, and the rendering midpoint.
- `FirstPersonGameTests` and `FixedStepTests` check camera/fist render
  interpolation and the fixed-step clock fraction.
- `BattleReplayTests` still reaches Day1, Day2, and Boss Clear by input and
  collision at 30, 60, and 120 rendered FPS. It also checks an enemy landing
  cue occurs once.
- `CombatFeedbackTests` loads every configured WAV, checks engine loop
  queue/stop, BGM transition, tutorial voice, and invalid cue diagnostics.
- Debug and Release each passed all 23 CTest cases. Both configurations
  passed GPU smoke tests for full game, first-person, player-combat, three
  single-enemy previews, Day1, Day2, and Boss. Full game and Boss each
  rendered 60 frames at 1600x900 in Debug; the other smoke checks used
  1280x720. Release full-game startup also passed three consecutive runs.

## Visual and listening comparison still needed

GPU smoke tests prove that models, textures, UI, and WAVs load and render or
queue. They do not establish that an image looks or sounds identical to the
original. Compare matching original and SDL frames for both fists, the
tutorial banner, the arena, each enemy, and Boss modes at 1280x720 and
1600x900. Listen through Title, Tutorial, Day1, Day2, Boss, and Ending to
check cue balance and timing on the target speakers. In particular, the
Boxer/Needle windup and recovery, Ultimate docking, fire and explosion
blending, and cockpit transitions still use simpler visual poses. Particle
effects and Blender bone animation remain outside this pass.
