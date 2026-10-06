# First-person movement, two controllable fists and a destructible Target

## Run

Build `BiggerFISTsSDL` in Visual Studio, then set its command argument to
`--first-person-preview`. Alternatively, from the SDL project directory:

```powershell
& '.\build\Debug\BiggerFISTsSDL.exe' --first-person-preview
```

The window captures relative mouse motion. Move the mouse to look, use W/A/S/D
to move relative to the camera direction. Hold Q for the left punch or E for
the right punch. Shift + left/right mouse button also charges the corresponding
punch; an unshifted mouse button is reserved for a later Rocket Punch step.
Release before full charge to cancel; after full charge, release to punch.
Move closer to the Target with W, punch it to remove it, and press R to reset
the Target and both arms for another attempt.
Press Escape to quit. Space
toggles the background. F1, F2, F8 and F11 retain the existing display actions.

The starting eye is `(0, 5.85, 0)`, facing world `+Z`. One copied original
`Target.obj` with `target.png` stands at `(0, 4.55, 8)`. Its configured spin is
zero. `LfistTEST.obj` and `Rfist.obj`, each with its copied PNG, form the idle
left and right fists near the bottom corners of the view. The Target stays in
place until a punch hits it.

## Why the original files are copied but not compiled

`reference/KamataEngine/Game/` contains byte-for-byte copies of the original
`Player`, `CameraController`, `GameMath`, `CombatTuning` and `RobotArm` files. Their C++
types depend on KamataEngine and other game systems. The SDL project therefore
ports the relevant behavior into small native classes instead of linking the
old engine into the new executable.

## The update path

1. `App/main.cpp` selects `--first-person-preview`.
2. `App/Application.cpp` loads `first-person-scene.cfg` and the copied Target,
   LfistTEST and RFist OBJ/MTL/PNG files. It asks SDL to capture relative mouse motion.
3. `Input/Input.cpp` sums mouse `xrel`/`yrel` events for one frame and tracks
   both mouse buttons. `InputActions` maps W/A/S/D and the configurable Q/E
   punch keys to `GameInput`.
4. `Game/FirstPersonGame.cpp` sends mouse deltas to `FirstPersonCamera`, then
   sends the current camera yaw and movement input to `PlayerMovement`.
5. `PlayerMovement` accelerates or decelerates on the X/Z plane, clips the
   player to the original battlefield bounds, and turns the body yaw toward
   the camera yaw. The body itself is state only; no body mesh is drawn.
6. `FirstPersonCamera` follows the player's eye anchor with the original
   exponential smoothing and aims according to yaw and pitch.
7. `PunchArm` updates the two independent charge and punch states.
   `FirstPersonGame::UpdateFistPoses()` positions the fist meshes, pitches them
   along the original forearm direction and turns them with body yaw. The camera
   turns directly, so both fists follow with the body's turning lag.
8. `MeshRenderer` renders the same static Target and two moving fists from
   the updated camera.
9. `FirstPersonGame` checks fist movement against the Target. A hit removes
   the Target instance from the scene; R inserts its original instance again.

## Punch state and timing

Each arm has its own `Ready -> Charging -> Charged -> Punching -> Recovery ->
Ready` sequence. A release during `Charging` returns that arm to `Ready`.
Once `Charged`, releasing starts the punch. The other arm keeps its own state
and can charge while the first arm punches. The original values are centralized
in `FirstPersonTuning.h`: charge `0.55` seconds, punch `0.22` seconds, recovery
`0.30` seconds, charge retraction `0.30` world units, and punch extension `3.0`
world units. `PunchArm.cpp` follows the original sine-shaped extension, which
extends the fist during the first half of the punch and returns it during the
second half. Frame time is capped at the same `maxMovementDeltaTime` used for
player movement.

In the original game, Q/E charge ordinary punches. Holding Shift while pressing
the left/right mouse button selects ordinary punches too. Releasing Shift while
still holding the mouse does not change the selected attack. The keyboard
bindings can be changed in `config/input.cfg`.

## Target collision and reset

The Target uses a sphere with radius `1.25`; each fist uses radius `0.55`.
These match the original dummy and fist collision values. The Target has one
hit point and a punch deals one damage. Only the `Punching` phase can hit.
`Collision::Intersects` checks the current fist position. The sphere sweep also
checks the path from the previous position to the current one, reducing missed
hits when the fist moves farther in one frame than its collision radius.
The collision code uses only sphere overlap and a moving-sphere-versus-static-
sphere sweep; it is not a general physics system.

On destruction, `FirstPersonGame` removes the Target's `ModelInstance` from
`ModelScene.models`. Its GPU mesh remains loaded so R can restore the original
instance without reading the OBJ or PNG again. R also returns both arms to
`Ready`. The player and camera remain where they are, making repeated tests
easy. The original Target starts at Z `8`; walk forward before punching because
it begins outside ordinary punch range.

## Idle fist placement

The original `RobotArm::CalculateHomeFistPosition()` is:

```text
fist = player + shoulder height + signed side offset + forward offset
```

The copied values are shoulder height `2.85`, half-width `1.55` and ready-fist
forward offset `1.85`. At yaw `0`, the left fist origin is
`(-1.55, 4.55, 1.85)` and the right origin is `(1.55, 4.55, 1.85)`.
The original ready pose rolls the left fist by `+90` degrees and the right
fist by `-90` degrees around each model's local Z axis. The config stores
these as `rotation 0 0 90` and `rotation 0 0 -90`.
The original `RobotArm` also puts the idle elbow below the shoulder. With an
upper-arm length of `2.0` and a ready-fist forward offset of `1.85`, this gives
the forearm and fist an X pitch of about `-62.5` degrees. The runtime computes
that pitch from the same values. Without it, the OBJ forearms extend nearly
horizontally and appear upside down in the first-person view.
The config format still accepts older two-angle `rotation x y` lines; the
optional third angle is Z. During play, both fists' positions and rotations are
updated from their arm states and the player's body yaw. The X pitch follows
the original elbow geometry. The opposite Z rolls apply in `Ready` and
`Recovery`; the original game removes them while charging and punching.

`SceneLoader` usually centers a preview mesh from its bounds. This gameplay
scene clears the Target and both fist preview centers so their OBJ origins remain
at the world positions expected by the original game.

`GameInput.moveY` has the earlier preview convention (`W = -1`), so
`FirstPersonGame` negates it to obtain the original player's `moveForward`
convention (`W = +1`). This conversion happens in one place.

## Values copied from the original behavior

The values in `FirstPersonTuning.h` match the original `CombatTuning.h`:
acceleration `6`, deceleration `7`, top speed `4.8` world units/second,
body turn speed `1.8` radians/second, player root Y `1.7`, eye offset `4.15`,
mouse sensitivity `0.0025` radians/pixel, maximum pitch `1.15` radians,
camera follow responsiveness `20`, and snap distance `3`. Battlefield bounds
and player radius are also copied. `GameConfig.maxMovementDeltaTime` caps
movement during a long frame at `0.05` seconds; rendered animation time still
uses the real elapsed time.

The original also has Rocket Punch, moving enemies, dodge, controller look,
camera shake, impact sound, vibration and extra camera modes. Those systems
are outside this step. `gameplay.cfg` still
controls the older model preview mode; first-person movement values are
centralized in `FirstPersonTuning.h` for now. The Target position and mesh are
editable through `config/first-person-scene.cfg`.

## Verification

```powershell
& '.\build\Debug\FirstPersonGameTests.exe'
& '.\build\Debug\CollisionTests.exe'
& '.\build\Debug\BiggerFISTsSDL.exe' --first-person-preview --smoke-test
```

The test executable checks movement, diagonal speed, camera yaw/pitch and
follow, both fists' idle poses, early charge cancel, full charge, independent
punch/recovery, Shift + mouse behavior, Target damage and reset. `CollisionTests`
checks touching spheres, missed sweeps and hits between frame endpoints. The
smoke mode loads three meshes and PNGs, renders 30 GPU frames, then exits. For
a visual/manual check, run without `--smoke-test`: the Target begins near the
center and parts of both fists appear near the bottom corners while W/A/S/D
and mouse motion change the viewpoint. Move forward to approach the Target,
then charge and release Q or E; it should disappear. R restores it.
