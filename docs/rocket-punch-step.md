# Rocket Punch port

## Try it

Build `BiggerFISTsSDL`, then run:

```powershell
& '.\build\Debug\BiggerFISTsSDL.exe' --first-person-preview
```

Hold the left mouse button for the left Rocket Punch, or the right mouse button
for the right one. Release before the 0.55-second charge completes to cancel.
Release after full charge to launch. Q and E still charge ordinary left and
right punches; Shift + mouse also selects ordinary Punch. Once a mouse press
has selected an attack, changing Shift while holding the button does not change
that attack. The opposite arm remains available while one fist is flying.

There are two static Targets at Z 8 and Z 12. A Rocket Punch can pass through
and destroy both. Press R to restore both Targets and reset both arms. To watch
the full flight and return, aim at the Targets and wait after firing. Escape
closes the preview.

## How the code is divided

1. `App/Application.cpp` loads the scene and makes a `FirstPersonTarget` state
   for every static `target` or `target_*` model. It logs launch, impact and
   reconnection events.
2. `Game/FirstPersonGame.cpp` maps the held keyboard and mouse state into a
   `PunchArmInput` for each arm. It owns Target HP, applies swept fist collision
   to every live Target, and exposes events to the application.
3. `Gameplay/Player/PunchArm.cpp` owns charge, flight, range, base avoidance,
   staged return, docking and recovery. It only reads gameplay data; it does
   not use SDL, KamataEngine or GPU objects.
4. `Rendering/FirstPersonScenePresenter.cpp` copies gameplay state into the
   model scene. A detached fist uses its rocket position and pitch/yaw; a
   destroyed Target instance is removed, and R restores it from the saved
   scene without loading its OBJ or PNG again.

The original `RobotArm.cpp` in `reference/KamataEngine/Game/` remains a
reference copy and is not compiled into the SDL executable. The source game
was not edited.

## Flight, collision and return

Each arm stores its own selected attack. A short press cancels while Charging;
at Charged, releasing commits the Rocket Punch. The entire fist model launches
forward at 100 world units per second. Its travel is capped at 80 units and
clipped to the battlefield edge with the original 3-unit margin. A shot toward
the research institute begins its safe evacuation return before the fist
enters the base collider.

At each fixed 1/60-second simulation step, `FirstPersonGame` checks the segment
from the fist's previous position to its current position against every live
Target sphere. This swept sphere check catches hits between frames. A hit does
not stop the rocket, so the same flight can destroy more than one Target. The
return goes through turn up, ascent, turn home, fly home, align and dock. The
arm enters Recovery after reconnection, then becomes Ready. R can reset it
from any phase.

Original combat values are centralized in `FirstPersonTuning.h`. The scene
positions and asset paths remain in `config/first-person-scene.cfg`. These
Targets are static test objects; moving enemies and dynamic scene instances
belong to a later migration step.

## Verification

```powershell
& '.\build\Debug\RocketPunchTests.exe'
& '.\build\Debug\BiggerFISTsSDL.exe' --first-person-preview --smoke-test
```

`RocketPunchTests` checks early cancel, full charge, launch speed, independent
arms, clipped range, base avoidance, staged return, reconnection, damage to
two Targets, presentation removal and reset. The smoke test confirms scene
loading and 30 rendered 3D frames. It cannot confirm visual parity by itself;
play the preview beside the original for pose and timing comparison.

The preview still has no rocket flame, impact audio, controller vibration or
camera shake. The original's moving enemies and ultimate attack are also not
ported yet.
