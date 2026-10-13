# Model transform port: original to SDL

This record covers authored OBJ origins, world placement, scale, rotation, and
program-controlled visual poses. The original `KamataEngine20260622/DirectXGame`
tree was read only. Its gameplay rules and the SDL collision data were not
changed for visual alignment, except for the Boss crystal slot positions, which
now use the original local XY halo before the Boss parent yaw is applied.

## Coordinate and model rules

- Game scenes preserve the authored OBJ origin and scale. OBJ X is mirrored on
  import to convert the right-handed mesh basis to the game's left-handed
  world; triangle winding is reversed with it. The standalone model preview
  still centers its model for inspection.
- The SDL transform is local scale, Z/X/Y rotation, translation, then optional
  parent world matrix. Angles in gameplay are radians; scene and asset config
  angles are degrees. `ModelScene::TryBuildModelWorldTransform` checks parent
  matrix finiteness before composing it.
- `battlefield-scene.cfg` holds static placements. `battle-assets.cfg` holds the
  required OBJ path and base scale, rotation, and local offset of each dynamic
  model. Missing or invalid pose fields fail loading with a file/line error.
  Dynamic models have zero automatic preview spin.
- The original `cube` OBJ is copied for completeness. The active SDL battle
  scene uses the finished arena meshes rather than drawing the original
  development blockout.
- The later presentation pass separates world, additive effects, transparent
  background, and first-person models. See `presentation-parity-audit.md` for
  the current rendering order and its verification status.

## Source comparison

| Model or state | Original source and values | SDL mapping | Status |
| --- | --- | --- | --- |
| Arena floor, walls, base | Scene creation in original Day scenes | `battlefield-scene.cfg`: 4 by 4 floor tiles, walls at x=+-24, z=-16/7/30/42, base at z=-16 | Static placements copied; rendered in GPU smoke |
| Body and arms | `Player.cpp::UpdateTransforms`, `RobotArm.cpp::UpdateTransforms` | Body follows player at half height; fists and upper arms use shoulder/elbow calculations in `FirstPersonScenePresenter`; startup sink offsets 2.6/2.8 | Numeric path implemented; manual original-frame comparison pending |
| Cockpit, helmet, tutorial banner, clouds | Original player/cockpit and tutorial scenes | Copied OBJ assets; cockpit follows body, helmet follows camera; banner at z=41.85; clouds use deterministic spawn and fixed-step movement | Implemented; original translucency and material appearance may differ |
| MissileUnit | `MissileUnit.cpp` drop/land/takeoff rotations | -90 degree X on landing, interpolated takeoff X, lateral yaw in flight | Implemented |
| Bomber | `Bomber.cpp` ring speed 5 radians/second and loaded bomb | Ring Z spin and carried bomb Y spin; carried bomb scale tracks reload | Implemented |
| ChargeUnit | `ChargeUnit.cpp` drop/takeoff/charge transforms | Landing roll, takeoff blend, tracked yaw, charge booster flame | Implemented with simplified warning pulse |
| Missiles, bombs, crystals | Original projectile and Boss sources | Direction/age rotation, missile tail flame, crystal slot local pose and launch elevation | Implemented; particle effects simplified |
| Boss core and three cones | `Boss.cpp::UpdateTransforms` | Root/core/cone parent matrices; Cruise, Charge, Needle, Boxer, Spread, Full Barrage, entrance and defeat poses | Full Barrage movement order is source-aligned; Boxer has alternating pullback, hold, strike and recovery poses; Needle Combo now pulls all three cones back and advances them in order; original-frame visual comparison remains open |
| Boss crystal halo | `Boss.cpp::UpdateCrystalTransforms` | Six Spike and four Cube instances rotate in local XY around a Boss parent, with original model scales and launch height | Numeric slot positions tested |
| Punch, Rocket Punch, Ultimate | `RobotArm.cpp`, `UltimateRocketPunch.cpp` | Fist/upper arm pose and rocket flame; Ultimate formation, Ready combined fist, flying combined fist/flame, split return | Staged return and separate docking now use the original phase structure; manual pose comparison pending |
| Shield and explosion OBJ | Original damage/defeat effects | Visual events create copied shield and explosion planes with independent visual lifetime | Geometry and state cues implemented; particles and exact color blending remain simplified |

## Verification

- `GameTests` checks child-to-parent world transform and reset. `BattleAssetTests`
  checks the required model catalog and static front wall. `BattleBossTests`
  checks initial cone and crystal poses. `CombatSessionTests` checks shield and
  explosion events. `BattleReplayTests` checks Day1, Day2, and Boss Clear from
  normal input/collision at 30, 60, and 120 rendered FPS; it also checks that
  the Ultimate Ready model and flying flame are present.
- Debug and Release builds succeeded. All 23 CTest cases passed in each build.
  Both builds rendered 30 GPU frames in each of nine modes: first-person,
  player-combat, three single-enemy previews, Day1, Day2, Boss, and full game.
  The Boss and full-game windows were also exercised at 1600x900 during this
  transform pass.
- An SDL Boss window was visually inspected. An original executable opened its
  Title screen, but automated DirectInput keys did not reach a battle scene.
  Therefore exact side-by-side frame matching, material/alpha matching, and
  moving-pose inspection against the running original are **not verified**.
  Source values and numeric tests are the current comparison evidence.

## Remaining visual differences

Boss Boxer and Needle Combo now use source-level pose paths, while their
existing collision timing is unchanged. The model tips and collision volumes
still need to be checked together in matching original/SDL footage. Likewise,
Ultimate return docking, shield/explosion blending, and cockpit startup have
since received source-level parity work documented in
`presentation-parity-audit.md`; exact visual comparison remains open. These
remaining differences are visual;
the fixed-step input-driven battle clear tests continue to pass. Before release,
capture matching gameplay states from a manually operated original executable
and the SDL build, then tune these poses and material effects from the images.
