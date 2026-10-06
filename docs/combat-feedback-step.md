# Combat sound and gamepad feedback

## Try it

Build `BiggerFISTsSDL` and run:

```powershell
& '.\build\Debug\BiggerFISTsSDL.exe' --first-person-preview
```

Hold Q or E until charged, release, and hit a Target. The copied
`enemy_is_hit.wav` plays at the original punch gain of 0.70; a connected
gamepad rumbles for 180 ms. Hold either mouse button without Shift until
charged and release. `RocketShot.wav` plays at gain 0.35 and the gamepad
rumbles for 120 ms at launch. A later rocket hit plays the same impact WAV at
gain 0.45 but does not start another rumble. The camera feedback still follows
the separate rules described in [camera feedback](camera-feedback-step.md).

These two WAV files are copies of the original game files. They are stored in
`assets/sound/` in the SDL repository, and CMake copies them beside the built
executable in `build/Debug/assets/sound/` or `build/Release/assets/sound/`.
The original files are not changed. A missing or invalid WAV reports its full
path and stops the first-person preview, so a broken package cannot silently
lose its required combat sounds.

`config/combat-feedback.cfg` holds the two WAV filenames, three gains, and the
motor strengths and durations. CMake places it beside the executable. You can
change these values without editing C++; rebuild to copy the edited config
and sound assets into the run folder. The loader rejects missing keys,
duplicates, invalid numbers and WAV paths outside `assets/sound/` with a file
and line diagnostic.

## Code path

1. `FirstPersonGame` reports `punchHit`, `rocketHit`, and left/right rocket
   launch events without calling SDL.
2. `Application` reads those events after each fixed simulation step and
   calls the appropriate `CombatFeedback` method.
3. `Platform/CombatFeedback.cpp` loads WAV with SDL, opens one playback device,
   and binds a small pool of `SDL_AudioStream` voices. Streams allow close
   impacts to overlap instead of waiting for the previous sound to finish.
4. The same platform class opens the first SDL gamepad, handles device add and
   remove events, and sends timed motor values through `SDL_RumbleGamepad`.
   Rocket launch has higher vibration priority than a punch hit. Vibration
   stops when the program exits.

The initial config values come from the original `CombatTuning.h`. SDL3 itself
is the already pinned dependency; this step adds no external library.

Audio device or gamepad absence is nonfatal: the preview continues and logs
that the optional device is unavailable. A gamepad can connect after startup.
This step provides feedback only; it does not map gamepad buttons or sticks to
movement and attacks yet. Punch swing, rocket reconnect, and other original
sound cues are also still pending.

## Verification

`CombatFeedbackTests` uses SDL's dummy audio driver to load both original WAVs,
queue all three combat cues, and check that missing config or WAV files report
their names.
The first-person smoke test checks that the packaged WAV files load and the
scene renders 30 frames. These automated checks cannot prove audible output
on the user's speakers or vibration on a physical controller; use the preview
for those checks.
