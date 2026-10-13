# Tutorial effects, practice Boss defeat, and fullscreen UI

## Scope and source comparison

- The original KamataEngine project was inspected read-only. Existing SDL
  working-tree changes were retained.
- The crystal texture has translucent pixels, so the background can affect its
  appearance. No crystal blending or color was changed in this pass; a matched
  original/SDL capture is still required to judge the size of the difference.
- The original Boss stops drawing its body in `Defeated`, while its final
  explosion continues. The SDL practice scene now follows this visibility rule
  and stays in practice instead of advancing to Ending.

## Corrections

| Item | Correction | Verification |
| --- | --- | --- |
| Tutorial Defense shield | Convert the applied player hit to the same shield visual event used by combat; both Tutorial and combat draw the queued effects through one presenter path. A hit that does not consume a shield layer does not create the effect. | `TutorialEncounterTests` checks the real projectile hit and event count; `BattleReplayTests` checks the shield and explosion models in the Tutorial scene and their expiration. Exact GPU moment remains to be captured. |
| Practice Boss defeat | Keep the Boss visible during `Defeating`, hide core and parts at `Defeated`, and let the final explosion finish. | `BattleReplayTests` checks one final explosion, visible effect geometry, absent Boss body, effect expiration, and continued `Ready` practice state. Exact GPU moment remains to be captured. |
| Emergency shutdown | Maximum black overlay alpha is 0.65 instead of 0.90. Full and quick startup opacity and fade timing are unchanged. | `CockpitPresentationTests` checks start, middle, and final opacity. The 1600x1000 capture shows the scenery remains visible under the final overlay. |
| Fullscreen UI | Title art uses uniform cover scaling. Story background, curtains, and screen-wide overlays fill the output. Story mask spans the output width. Main text and scene content keep uniform safe-area scaling; outer HUD indicators and prompts anchor to the output edges. Target frames use the actual output dimensions for projection. | `FullGameUiTests` checks 1280x720, 1920x1200, and 2560x1080 bounds. Debug captures at 1280x720 and 1600x1000 and a native 1920x1080 fullscreen Title capture show no side blanking. |

## Validation and limits

- Debug and Release application builds succeeded; all 27 CTests passed in each
  configuration. Existing normal-input Day1, Day2, and Boss clear tests remain
  part of the suite.
- Debug GPU smoke checks passed for the full game, first-person preview,
  player-combat preview, and all six battle previews. Release full-game smoke
  also passed after the final source change.
- The 1600x1000 Title, Story, and emergency shutdown frames and the native
  1920x1080 fullscreen Title were inspected. The 2560x1080 ultrawide layout
  was checked numerically, not on an ultrawide monitor.
- A matching original fullscreen capture is not available. Tutorial Defense
  shield and practice Boss final explosion are verified by gameplay/presenter
  state tests, but their exact rendered frames still need manual comparison.

## Shield impact color follow-up

- `PlayerVitality` now reports a semantic shield impact state after applying
  damage: two or more layers left, one layer left, or broken. Visual events and
  the presenter pass that state through to the blue, yellow, and red shield
  models. New hit types do not need their own color branch.
- Projectile hits from 3 to 2, 2 to 1, and 1 to 0 layers are covered, as are
  ChargeUnit and BossSpreadBurst breaking several layers in one hit. Hits that
  consume no shield do not create a shield visual event.
- `build/shield-charge-820.bmp` is a fixed-tick Debug capture of the practice
  ChargeUnit breaking all three shield layers. Its red impact effect was
  inspected; the same replay at tick 900 shows the effect expired. This capture
  is an SDL rendering check, not a matching original pixel comparison.
