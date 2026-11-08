# Studio Brief — Undergod Animation Studio v1

## Purpose
A standalone DxLib app that imports Undergod Card's assets and lets the developer
play, scrub, and iterate every character animation by editing data/code and
hot-reloading — a code-driven animator / animation test bench.

## Source material (game repo: ~/projects/undergod-card, folder UndergodCard/)
Assets live under assets/pastel/ — copy them into this repo unchanged:
- characters/: individual 512x512 PNG frames, pattern char_<actor>_<anim>_<n>.png, n = 0..3
  - chick (player): idle, attack, hit, defend, ult_charge, ult_throw (6 anims x 4 frames)
  - frog (boss):    idle, attack, hit, defend, ult_charge          (5 anims x 4 frames)
- vfx/: spear.png, spear_impact.png (512x512)
- cards/: punch.png, heavy.png, shield.png, cookie.png (256x256) — include for completeness
- STYLE.md (art style notes)
Data (copy to data/): characters.json, enemies.json, cards.json, balance.json, cinematic.json
- enemies.json: single boss "frog" (Ếch Vương, maxHp 120, stage threshold 60%,
  gentleCharge 14, challengeCharge 34 — from the game's difficulty profiles).
- cinematic.json: the player ultimate "sao_roi" — a timed step list the studio's
  cinematic player must support: dimScreen, cameraZoom(to, duration),
  playSprite(target, anim), moveSpear(phase, from, to, duration),
  impact(callback), spawnVFX(sheet, at, frames), shake(intensity, duration),
  flashWhite(duration), end. Total 3600 ms, skippable.

## Animation model
- Clips are DATA: a clip = ordered frames (image files) + per-frame durations +
  optional timed effect cues (offset/scale/rotation/tint/alpha keyframes and
  discrete cues: shake, flash, particles, popup, dim, zoom) using the same timing
  language the game centralized in phases 2-4 (see the game repo's
  include/game.hpp, src/game.cpp, src/ui.cpp — REUSE its approaches and, where the
  code separates cleanly, its code).
- Terminal clips (death, victory) do not exist as frames in the game: compose them
  from existing frames + the phase-4 treatment (fade/tilt/desaturate/dust for the
  defeated actor, bounce for the survivor) and mark them as composed clips.
- The engine core (clip parsing, timeline sampling, cue firing) must be portable
  C++17 with no DxLib dependency, exercised by tests/run_portable.sh (GCC), the
  same pattern as the game repo.

## Studio app (DxLib, Windows)
- Stage view rendering the current actor at game-like scale over a simple arena backdrop.
- Actor picker (chick / frog), clip picker (all clips incl. composed + cinematic sao_roi).
- Transport: play/pause (Space), restart, loop toggle, speed 0.25x-3x,
  timeline scrubber + frame step (arrow keys), time/frame/cue readout.
- Effect toggles: shake, flash, particles, dim, reduced-effects master switch
  (mirroring the game's shared setting).
- Hot-reload clip/cinematic data with R; show a brief "reloaded" notice; a broken
  JSON must keep the previous data and show the error instead of crashing.
- Keyboard-first controls; list them in README.md.

## Deliverables & rules
- VS solution + build.ps1 + verify.ps1 following the game repo's pattern.
- Portable tests: all clips parse; frame files exist and indices are in range;
  durations > 0; sampler determinism (same t -> same pose); cues fire exactly once
  and in order; loop wrap is seamless. Print RESULT lines like the game repo.
- Commits: (1) assets+data import, (2) portable animator engine + tests,
  (3) studio app, (4) docs/build scripts. Push to origin main at the end.
- Do NOT modify the game repo. DxLib code is statically verified only on this
  Linux server — say so plainly in the final summary.
