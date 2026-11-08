# Undergod Animation Studio

A code-driven animation test bench and studio for **Undergod Card** (nqxw1kqw/undergod-card).

This repository is the home of the game's character/VFX assets and a standalone studio
executable that loads them and plays every animation for testing and iteration:

- Actors: **Chick** (player) and **Frog King** (boss)
- Clips per actor: idle, attack, hit, defend, ult_charge — plus the player's
  ult_throw and the full "Sao Roi" ultimate cinematic (spear, camera zoom,
  impact VFX, shake, flash) defined in data/cinematic.json
- Transport: play/pause, loop, restart, speed control, timeline scrub, frame step
- Effect toggles (shake / flash / particles / dim / reduced-effects), live readout
- Hot-reload of clip data (edit JSON -> press R) — the "animator by code" workflow

Built with C++17 + DxLib, mirroring the game repo's layout and build scripts
(build.ps1 / verify.ps1; portable logic tests via tests/run_portable.sh).

See STUDIO_BRIEF.md for the full specification and asset inventory.
