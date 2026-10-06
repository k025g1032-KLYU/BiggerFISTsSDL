# First-person camera feedback

## What changed

The SDL first-person preview now uses the original prototype's camera shake
timing for two actions:

| Action | Strength | Duration | Trigger |
|---|---:|---:|---|
| Ordinary Punch | 0.12 | 0.16 seconds | The fist damages a Target |
| Rocket Punch | 0.07 | 0.12 seconds | The fist launches |

The original `CombatScene::PlayHitFeedback()` does not shake the camera when a
Rocket Punch hits a dummy Target. The rocket impact is still reported as a
separate gameplay event. The [combat feedback step](combat-feedback-step.md)
connects this event to its WAV sound.

The original uses current sphere overlap for an ordinary Punch and a swept
sphere for a flying rocket. `FirstPersonGame::ProcessTargetHits()` now follows
that distinction. It reports `punchHit` or `rocketHit` in `FirstPersonEvents`.

`FirstPersonCamera::AddShake()` stores the strength and remaining time.
`Follow()` applies the two sine-wave offsets from the original camera to the
rendered camera position, with linear attenuation as the timer ends. It leaves
the camera yaw, pitch, and followed anchor unchanged. Its timer advances in
the fixed 1/60-second simulation update. Overlapping shake requests keep the
strongest strength and longest duration; a new request after a previous shake
expires starts with its own strength.

The values live in `FirstPersonTuning.h`; the gameplay code does not contain
SDL or GPU calls. `Application.cpp` logs whether a Target was destroyed by an
ordinary Punch or Rocket Punch.

## Try and verify

```powershell
& '.\build\Debug\BiggerFISTsSDL.exe' --first-person-preview
```

Approach the first Target, hold Q or E until charged, then release. The view
shakes briefly only when the fist hits. Press R, then hold a mouse button
without Shift until charged and release: the view shakes on launch even before
the rocket reaches a Target. R restores the two Targets and both arms.

`FirstPersonGameTests` checks shake on a Punch hit, no shake on a miss, and
decay back to the followed anchor. `RocketPunchTests` checks launch shake and
that a rocket hit does not restart it. The GPU smoke test verifies startup and
30 rendered frames; it does not compare the visual effect against the original.

Impact WAV sound and SDL gamepad vibration are implemented in the
[combat feedback step](combat-feedback-step.md). Player damage reactions remain
for later steps.
