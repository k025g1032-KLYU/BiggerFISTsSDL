# Audio source parity

## Reference and assets

- The original `KamataEngine20260622/DirectXGame` source was inspected read-only. Its `CombatScene`, `TutorialDirector`, and `GameManager` determine when sounds play.
- The previous asset audit matched all 33 original WAV paths used by the game to SDL copies by SHA-256. SDL has 34 WAV files because `explosion.wav` and `battle_explosion.wav` are identical copies. Unreferenced original WAV and MP3 files are not part of playback.
- This pass preserves damage, collision, enemy, and wave rules. Audio events contain no SDL types.

## Event mapping

| Original source event | SDL source and output | Check |
| --- | --- | --- |
| Each arm begins Punch or reconnects Rocket | `BuildPlayerAudioEvents` emits one `PunchSwing` or `RocketReconnect` per arm | Two simultaneous arm flags produce two events each |
| Each dummy hit | Each `targetHits` entry produces the corresponding Punch or Rocket impact | Static target hits are counted individually, including Tutorial and practice |
| Enemy defeat | `CombatSession` emits `EnemyDefeated` without a second generic impact | Direct Punch/Rocket defeats produce one defeat cue |
| Ultimate defeats several enemies in one fixed tick | `CombatSession::ProcessUltimateHits` emits one `EnemyDefeated` if any enemy was defeated | Input-driven grouped enemy test checks two defeats and one cue |
| Boss core hit | Successful normal hit emits Punch or Rocket impact; successful Ultimate hit emits `BossDamaged` | Battle session event test checks normal Rocket |
| Projectile intercepted by a fist | Projectile explosion plus the attack's impact | Battle session event test checks one of each |
| Boss Spread Burst releases | `BossSpreadBurstExploded` plays even if the player is outside the hit area | Practice Boss test checks one cue; damage still uses the existing range check |
| Projectile reaches player shield | `ShieldedProjectileExploded` at 0.30, selected from shield loss | Tutorial and battle tests check the shield outcome; visual scale is not used to choose sound |
| MissileUnit fires | `ProjectileLaunched` at 0.18 | Actual enemy windup emits one cue; practice test missile spawn is silent |
| Boss mode change and crystal riser | Separate gains 0.50 and 0.55 | Config parser and audio test verify both |
| Tutorial personnel voice | Personnel gain 0.95; system voices stay at 0.90 | Config parser and audio test verify personnel gain |
| Battle Clear, Game Over, reset, or retry | Battle voices stop on the fixed update that changes the state | Music remains owned by the flow and changes immediately on scene transition |
| BGM changes | Old track stops and the new track queues immediately | Audio test checks both streams without a 0.4-second fade |

The other mapped cues, including landing, Charge, Boss attacks, cockpit, shutdown, engine, footsteps, and victory BGM, retain their previous source-aligned files and gains. `combat-feedback.cfg` is the source for the new gain values and reports invalid values with a filename and line number.

## Verification limits

- `CombatFeedbackTests` uses SDL's dummy audio driver. It verifies loading, gain configuration, event routing, overlapping stream allocation, immediate music switching, and stop/queue behavior. It does not establish audible similarity.
- `CombatSessionTests`, `TutorialEncounterTests`, and existing input-driven battle tests verify fixed-update event counts and gameplay regressions. Visual smoke tests verify startup and rendering, not loudness.
- Direct listening against the original executable for Title through Ending, Tutorial, practice, simultaneous arms, and retry remains pending until both versions can be auditioned on an audio device.
