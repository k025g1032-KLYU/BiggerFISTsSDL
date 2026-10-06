# Original application-side player and camera reference

These eight files are unchanged copies from
`KamataEngine20260622/DirectXGame/Game/` in the sibling project:

- `Player.cpp`, `Player.h`: original player state, movement, body yaw and collision bounds.
- `CameraController.cpp`, `CameraController.h`: original look input and camera follow.
- `GameMath.h`: original yaw-based direction and angle helpers.
- `CombatTuning.h`: original movement and camera constants.
- `RobotArm.cpp`, `RobotArm.h`: original left-arm ready pose and model transform.

They are reference material and are intentionally absent from all CMake targets.
They depend on KamataEngine and other game systems, so compiling them directly
in the SDL project would also pull in combat, audio and engine code.

The SDL implementation of this step is in `src/Gameplay/Player/PlayerMovement.*`,
`src/World/FirstPersonCamera.*` and `src/Game/FirstPersonGame.*`. It ports
normal ground movement, the first-person driving camera and both idle-fist
pose. The original project was not edited.
