# Input alignment with the original PC game

Source reference: `KamataEngine20260622/DirectXGame/Game/CombatScene.cpp`,
`CameraController.cpp`, `CombatTuning.h`, and `TutorialDirector.cpp`.
The source project was inspected without modification.

| Input | SDL full-game behavior |
| --- | --- |
| WASD / left stick | Move |
| Mouse / right stick | Look; observation uses the original 1.35 speed multiplier |
| Q/E, LT/RT | Charge ordinary left/right Punch |
| Left/right mouse | Charge left/right Rocket Punch |
| Shift + left/right mouse | Charge ordinary left/right Punch; releasing Shift mid-charge keeps the selected attack type |
| LB/RB | Charge left/right Rocket Punch, independent of keyboard Shift |
| Space / A | Dodge during combat; confirm where the flow expects confirmation |
| Enter / Start | Start or retry a battle |
| V / R3 | Observation and return to driving view |
| F1/F2 | Third-person and cockpit-model inspection during Tutorial, Day1, Day2, and Boss |
| F3 / View | Toggle combat data during Tutorial, Day1, Day2, and Boss |
| Ctrl+F1/F2 | Set 1280x720 / 1600x900 during combat and practice |
| F8 | Change display during formal battles; test shutdown in free practice |
| Ctrl+F8 | Change display during free practice |

Tutorial VisualLink and Mobility block attacks. Punch accepts Q/E, LT/RT,
and Shift+mouse but blocks Rocket Punch. RocketPunch unlocks Rocket Punch.
Attack type is selected at the start of charging. The full game consumes
inspection actions in the fixed 1/60-second update; practice spawn commands
remain separate. Existing replay scripts using `practice ThirdPerson`,
`practice CycleCockpitModel`, or `practice ToggleData` still work.

`--first-person-preview`, other previews, Title, and story screens retain
their display shortcuts. No damage, collision, wave, or camera position values
were changed for this input pass. Automated tests cover SDL keyboard/mouse
events, a virtual SDL gamepad, fixed-step action buffering, tutorial attack
gating, and replay compatibility. Physical gamepad hotplug and feel still
require a controller on the target PC.
