# Startup and full-game UI source parity

## Baseline

- Original source: `KamataEngine20260622/DirectXGame` at
  `89de23714402d5168e11482463e03c63c0e00338`; inspected read-only.
- SDL baseline before this pass: `7dd928f3a5b191e536b7e7f9898703a3c8a662db`
  with 63 pre-existing modified or untracked paths. They were preserved.
- Before edits, Debug and Release CTest each passed 25/25. The new
  `FullGameUiTests` first failed on seven source-confirmed UI differences.
- Source-level agreement does not establish pixel parity. The current original
  executable can run and was captured at Title; matching captures for later
  states and audio listening are separate checks.

## Source-to-SDL comparison

| State | Original source rule | SDL baseline difference | Correction and check |
| --- | --- | --- | --- |
| Full, quick and Boss startup | `CockpitPresentation` uses 4.8/1.5 s, 0.45/0.2 s lines, .35 s all-green delay, .65 s smooth activation; `CombatScene::DrawDebug` draws blackout and checks before enabled HUD | Startup data matched, but `FullGameUi` drew HUD under blackout | Preserved existing timing; reordered draw commands. `CockpitPresentationTests` checks titles, thresholds, HUD stages, activation and cue |
| Tutorial startup | `CombatScene::UpdateGameHUD` gives TutorialDirector visibility priority over generic startup line visibility | Generic startup line visibility overrode Tutorial HUD gates | Tutorial gates now take priority; draw-list test checks armor stays hidden during visual-link startup |
| Emergency reboot | `CockpitPresentation` uses .5 s shutdown, 1.0 s reboot and a black overlay; non-tutorial battle HUD is hidden while active | HUD appeared under the overlay; battle status could appear during startup | Emergency overlay, text and HUD suppression use original draw order; draw-list tests cover shutdown and reboot |
| Battle HUD | `GameHUD` uses separate empty armor/base/shield colors, shield spacing of 5 px, armor y=634, shield y=588/593, text scale 1.2 and no generic background panels | Shared empty tint, 3 px shield gap, shifted labels and extra panels | Restored source values and removed extra panels; draw-list tests cover coordinates and empty shield |
| Dodge and target locks | Original Dodge ready requires Ready state and expired cooldown; lock preview is allowed for Ready or charged Rocket only, with left/right box offsets | Cooldown alone controlled indicator; Punch charge could retain a Rocket lock; no overlap offset | Added read-only readiness query, arm-state gate and ±1.5 px offsets; tests cover Dodge and independent arm lock behavior |
| Tutorial and free practice | `TutorialDirector` panel is 560×122 at (348,100); Punch/Rocket hit statuses share one instruction line; caption and continue prompt last ten seconds | Panel was larger; instructions split over two lines; skip/continue prompts differed | Matched text, dimensions and prompt conditions; draw-list tests cover all six steps, startup, independent left/right hit progress and practice caption |
| F3 practice data | `CombatScene::DrawDebug` uses unbacked text rows at original coordinates for controls, player, arms, enemies, projectiles, camera and patterns | SDL used a short five-line box; practice Boss HP was absent when F3 was hidden | Replaced with source-positioned rows using read-only SDL combat state. Practice Boss HP is visible with F3 hidden and suppressed with F3 shown. Release still offers F1–F8 practice commands by accepted port design |
| Title and story | `GameManager` blinks Title prompt on fixed 1 s cycle; `CinematicController` draws story prompts throughout story playback, NEXT at x=1080 and SKIP at x=1095 | Title used rendered-frame time; story prompts hid during motion and used different coordinates/scale | Blink time now advances at fixed steps; story prompt condition and positions follow original. `GameFlowTests` and draw-list tests cover these cases |
| Curtain and Ending | Original cinematic draw returns before battle HUD, with moving curtain; Ending hold displays return prompt | SDL could render HUD behind a partial curtain | Battle HUD suppressed during cinematic; curtain and Ending commands are checked |

## Current verification

- Debug and Release `BiggerFISTsSDL` and all 26 named CTest targets built. CTest
  passed 26/26 in each configuration. The input-driven full-game test still
  clears Day1, Day2 and Boss and returns from Ending to Title.
- Full game, first person, player combat, MissileUnit, Bomber, ChargeUnit,
  Day1, Day2 and Boss each passed a 30-frame GPU smoke test in Debug and Release
  (nine modes per configuration).
- Current SDL GPU captures were inspected at 1280×720:
  `build/ui-title-1280.bmp`, `build/ui-boot-{0,120,288}-1280.bmp`,
  `build/ui-cockpit-1280.bmp` and `build/ui-full-{240,490,520}-1280.bmp`.
  The full-game replay shows Tutorial startup, F3 free-practice data and
  emergency shutdown using real fixed-tick input.
- Full-game capture, with later F4 presses withheld for each battle, reached
  normal Day1, Day2 and Boss HUD states in `build/ui-{day1,day2,boss}-hud-1280.bmp`.
  An unattended Day1 run reached zero base HP and displayed `MISSION FAILED`
  and its retry prompt in `build/ui-gameover-3300-1280.bmp`. The Ending hold
  and return prompt appear in `build/ui-ending-hold-1280.bmp`.
- `build/ui-title-1600.bmp`, `build/ui-boot-120-1600.bmp` and
  `build/ui-full-490-1600.bmp`, `build/ui-boss-hud-1600.bmp`,
  `build/ui-gameover-1600.bmp` and `build/ui-ending-1600.bmp` show the
  1280×720 design scaled to 1600×900. The runtime `display.cfg` was restored
  after capture. The original F3 debug text contains long, unbacked lines
  that remain difficult to read over bright scenery. Its source positions are
  retained for parity.
- The September 26 original executable was launched from
  `KamataEngine20260622/提出/1_Game` and its 1280×720 client area was captured
  at Title in `build/original-title-prompt-1280.png`. Compared with
  `build/ui-title-1280.bmp`, 943 of 921,600 pixels differ (0.1023%); the
  average absolute RGB difference rounds below 0.005 per channel. A second
  capture with the blinking prompt absent differs at 36 of 882,109 pixels
  after excluding the prompt region. This establishes Title pixel parity at
  1280×720, including the prompt position and source artwork.
- Seven original submission images exist under
  `KamataEngine20260622/提出/2_Screenshot`. They are dated August 27, while
  the current original source and executable are dated September 26. Their
  HUD places SHIELD on the right; current `GameHUD` source places it on the
  left, and the SDL UI follows that later layout. The old submission images
  are useful for identifying art and scene elements, but are not a valid
  pixel reference for the current HUD.
- `config/presentation-ui-flow.replay` is copied next to both executables and
  replays Title → Tutorial → Day1 → Day2 → Boss → Ending → Title through normal
  input actions and F4 phase skips. This is a UI route check; the separate
  `FullGameIntegrationTests` is the input-and-collision clear check. For an
  Ending hold GPU capture from the project root, run:

  `build/Debug/BiggerFISTsSDL.exe --replay-input=build/Debug/presentation-ui-flow.replay --capture-frame=build/ui-ending.bmp --capture-at-frame=870 --smoke-test --smoke-frames=902`

## Startup opacity and opening-curtain follow-up (2026-10-14)

- The user selected 0.75 opacity for the full startup blackout instead of the
  original 0.94. Quick/Boss startup remains 0.78 and emergency reboot remains
  0.65. This is an intentional presentation difference.
- When the curtain opens into Tutorial, Day1, Day2, or Boss, the UI draws the
  same full-viewport startup blackout beneath the moving curtain. Startup
  checks and HUD still wait for the normal startup phase; only the underlying
  opacity is shown early. This removes the brightness jump at curtain exit.
- `FullGameUiTests` checks all four phases, overlay order, and opacity
  continuity. Debug and Release builds and the relevant UI, cockpit, and flow
  tests pass. Fixed-input Debug captures `build/opening-110.bmp` and
  `build/opening-130.bmp` show the curtain opening and the first startup check.

## Verification still required

- Capture matching original and SDL states at 1280×720 beyond Title. The
  packaged original executable remains open with a 1280×720 client area;
  direct PowerShell invocation had returned immediately because it is a GUI
  application. Screen capture works when its window is placed on top, but
  automated keyboard focus was not acquired in this desktop session, so
  matching boot checkpoints, Tutorial steps, practice F3, Day1/Day2/Boss HUD,
  Game Over and Ending are still visually unverified against this executable.
- The original game uses a fixed 1280×720 window, so a 1600×900 original image
  is not an equivalent reference. SDL 1600×900 captures check scaling and
  clipping only.
- Listen to startup cue and engine loop across retry and phase changes; this
  pass changes UI timing and draw order, not audio playback rules.
