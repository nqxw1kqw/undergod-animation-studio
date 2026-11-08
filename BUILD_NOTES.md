# Build and verification notes

## Portably tested on this Linux host

`tests/run_portable.sh` compiles with GCC, C++17, `-O2 -Wall -Wextra -Wpedantic`.
It creates an isolated temporary executable, runs against this repo's data and
assets, uses a temporary asset/data copy for failure fixtures, and cleans up.
The engine and console checks have no Windows/DxLib includes or link dependency.

The passing RESULT groups are:

```text
RESULT clip_inventory_frames_durations PASS
RESULT sampler_determinism PASS
RESULT cue_exactly_once_ordering_and_large_dt PASS
RESULT seamless_loop_wrap PASS
RESULT cinematic_all_9_actions_sampling_and_skip PASS
RESULT composed_terminal_tracks PASS
RESULT transactional_reload_invalid_json_and_validation PASS
RESULT timeline_nudge_clamp_resort_duration_edits PASS
RESULT timeline_save_reload_roundtrip_equality PASS
RESULT timeline_invalid_save_preserves_file_bytes PASS
RESULT onion_skin_loop_aware_frame_neighbors PASS
```

These cover all 15 delivered clips, the 44 unique character paths and index order,
positive durations, repeated/stateless sampling, large and partitioned time
advances, simultaneous cues and time-zero cues, per-cycle exactly-once ordering,
loop periodicity and transform continuity, all nine cinematic action types,
quadratic spear motion, boundary zoom interpolation, impact/end once-only and
skip behavior, composed treatments, malformed JSON, invalid cue/track/frame data,
and transactional preservation/recovery across reload failure. Loop tests prove
timeline/frame wrapping and track continuity; they do not prove pixel similarity
between the last and first source drawings.

The import was independently byte-compared against the source tree: 6 chick
animations × 4, 5 frog animations × 4, 2 VFX, 4 cards, STYLE.md, and the five JSONs.
The game repo was only read/copied, never modified.

## Statically checked only; Windows remains unverified

The VS project XML parses, every declared source/header exists, and the solution
configuration mappings and runtime copy paths were inspected. The app source
also passes a MinGW C++17 syntax check with **temporary hand-declared DxLib API
stubs** outside the repo. This is only a syntax check, not an actual SDK build or
API-signature validation. No DxLib SDK was available here.

All DxLib rendering and input remain statically reviewed only:

- Actual Visual Studio/MSBuild compilation, DxLib header compatibility, x64 library
  linking and CRT matching; PowerShell build/verify execution.
- Window initialization, timing, font creation, UTF-8 rendering, mouse coordinate
  mapping, key constants, press detection and button/drag behavior.
- PNG decoding, grayscale GraphFilter results, blend/tint/alpha composition,
  rotation, pivot, mirroring, render-target switching and cleanup.
- Stage appearance, zoom/shake clipping, 2×2 VFX sheet crops, spear motion,
  particles, dust, popups, reduced effects and readability of controls/readouts.
- Live app reload, including retention of old GPU handles on failure, notifications,
  rapid transport changes and long interactive sessions.
- The Windows smoke screenshots, timeout behavior and report files; `--smoke`
  is implemented but has **not been run** here. Its result cannot substitute for
  manual inspection and keyboard/mouse checks.

On Windows, run `verify.ps1`, inspect `bin/Release/artifacts/*.png`, then manually
exercise all controls, looping and scrub boundaries. Try malformed clips JSON,
malformed cinematic JSON and a missing/undecodable image while the app is running;
R should retain the previous scene and show the error. Restore valid files and
reload, including changes to durations, tracks and simultaneous cues.

Round 2 timeline tests cover cue clamp/sort/selection, frame floors, proportional
track retiming, full-field save/reload equality, cinematic byte preservation and
invalid edits anywhere in the catalog leaving clips.json byte-identical.
The engine stays DxLib-free. Transactional replacement uses POSIX rename on Linux
and MoveFileExW on Windows; the Windows replacement path is only statically
checked here. Interactive row selection/scrolling, punctuation/Shift/Ctrl input,
error notices, runtime write permissions and Windows file replacement still need
Windows verification. No actual DxLib build or interactive test was performed.

Onion neighbor tests cover every delivered frame with and without looping, both
endpoints, a single-frame clip and invalid indices. Onion rendering order, alpha,
pivot alignment, mirroring and O input remain statically verified only.

Backdrop cycling (arena/dark/checkerboard), G toggling, floor/crosshair alignment,
stage clipping and readability over all three backgrounds remain Windows-only
visual/input checks. They follow existing DxLib drawing and key-edge patterns
and were statically syntax checked with the same temporary stubs.

## Reuse and provenance

Source: sibling `~/projects/undergod-card/UndergodCard`, game repository revision
`2a70e0277527e02768afc7431b4356966961d858` at inspection.

- `include/quest_timeline.hpp` is copied unchanged. It supplies stable chronological
  dispatch and exactly-once impact/end callbacks, including skipped cinematics.
- `src/cinematic_data.cpp` extracts `LoadTimeline` from `src/quest_data.cpp`, with
  its action names, fields, target/phase/sheet checks and spear-contact constraint.
  Studio additions reject nonfinite values, invalid effect ranges/durations,
  effects beyond end, and a nonfinal end; steps are stable-sorted when loading.
- `include/game.hpp` supplied the timing/visual structures and shared reduced
  feedback model. `Game::SampleZoom`, `CueStep`, and `SpearTip` in `src/game.cpp`
  supplied cue-boundary zoom sampling, late-event age, quadratic ease-out rise
  and quadratic fall. The studio samples these as pure time functions.
- `Game::Timing`, `PoseFrame`, and `ActorOffset` supplied 250 ms idle/throw frame
  pacing, 125 ms reaction frames, 650 ms attack duration, 320 ms contact, quadratic
  lunge and smooth recovery, and the hit recoil expression. These are represented
  as frame durations and keyframes, rather than a second hardcoded clip system.
- `Game::BeginTerminal` and `src/ui.cpp` supplied 800 ms terminal fade/tilt,
  grayscale/darkening, 12 dust particles with the same velocities, survivor bounce
  `abs(sin(min(age,.8)*7.85))*18`, and particle gravity `160*age*age`.
- `Ui::Actor`/`World` supplied stage dimensions, actor homes (320/1120), 0.5 scale,
  (256,456) foot pivot, mirrored frog, pastel arena palette, spear pivot/scale,
  2×2 impact rendering at 8 fps, shake sin(time*95) and 180 ms tail attenuation,
  flash fade, 70 px/s popups, and reduced-effects suppression/attenuation.
- The game VS projects, solution, `build.ps1`, `verify.ps1`, main-loop pattern and
  `tests/run_portable.sh` were adapted for this repo and its two build targets.
- The game's vendored nlohmann/json header is reused, with its MIT license copied
  beside it. Character/VFX/card assets and gameplay data are unchanged imports.

The new work is the clip schema/catalog, track and interval samplers, validation,
stateless cinematic visual reconstruction, studio UI/transport, transactional
reload and portable tests. No game combat code or DxLib dependency enters the
portable core. No new art was created.
