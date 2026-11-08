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

- `include/quest_timeline.hpp` retains the copied cursor; round 3 adds parser/serializer declarations. It supplies stable chronological
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

Round 3 A: portable `dual_actor_impact_offsets_and_exchange` checks delivered
attack offsets, reaction boundaries, idle return, missing-cue default and cinematic
hit/defeat timing. Dual actor drawing, pickers, facing and X remain Windows-only,
statically reviewed; no actual DxLib build or interactive test was performed.

Round 3 B adds RESULT groups `cinematic_nudge_clamp_selection`,
`cinematic_save_reload_roundtrip`, and
`cinematic_invalid_save_preserves_file_bytes`. They cover both clamp endpoints,
selection through stable sorting, all serialized step fields, contact/end/range/
nonfinite rejection and destination preservation. Cinematic step panel, Tab,
Ctrl+S notices and Windows atomic replacement remain statically verified only.

Round 3 C: `transport_once_freeze_loop_and_shuttle` tests nonlooping large-delta
clamp/stop, final sprite, retained loop cycle time, speed increments and cap.
Impact marker positioning uses the same portable ImpactTime API as choreography.
J/L/P/V input, marker appearance, final-pose display and all dual-stage/cinematic
interaction remain Windows-only. The current source passes a MinGW C++17 syntax
check with temporary hand-declared DxLib stubs (including new key constants);
this does not validate actual SDK signatures or key definitions. The portable
engine remains DxLib-free. No Windows build, rendering or smoke run was performed.

Round 4 A: portable exports add RESULT groups
`export_trace_boundaries_sampler_and_cues`,
`export_contact_sheet_bmp_header_pixels_and_rects`, and
`export_partial_grid_and_errors`. Every delivered clip's trace is compared with
SampleInterval at all frame starts and the terminal boundary, including cue
identities. Synthetic RGBA frames test both square grids and tall strips,
V4 BMP fields/size, channel order, orientation, alpha, exact rectangle coverage,
unused transparent cells and error reporting. PNG decoding is a caller-supplied
adapter; composition and BMP/JSON writing are entirely portable without new
third-party dependencies. No Windows or DxLib code enters the export engine.


Round 4 B: E exports the primary clip to executable-relative exports/ with folder
creation and a three-path notice; Shift+E retains reduced effects. X exchanges
export the primary clip; Sao Roi exports the currently sampled primary sprite
clip. The DxLib soft-image decoder uses RAII cleanup and passes RGBA8 pixels to
the portable engine. No optional PNG output was added. App input, source PNG
soft-image decoding/alpha, filesystem permissions, notices and Windows BMP
viewing remain statically verified only. Temporary hand-declared DxLib stubs
check C++ syntax, not SDK signatures. No Windows build or runtime test was run.
All 19 portable RESULT groups pass, including every pre-existing test.

Round 5 A: structural_insert_delete_duplicate_minimum_and_cue_repair and
structural_save_reload_roundtrip cover frame copies, deletion, last-frame refusal,
unused clip suffixes, cue repair, and full document save/reload equality.
Structural input and notices follow existing DxLib patterns and are statically
reviewed only; Windows key definitions, interactive behavior and actual SDK
compilation remain unverified. No game repository files were modified.

Round 5 B: history_mixed_edits_exact_undo_redo_and_branch and
history_bound_and_affected_file_isolation test mixed clip/cinematic edits,
full-field restoration, redo, branch invalidation, no-op omission, the 50-entry
bound, affected-file isolation and history clearing. History snapshots remain
portable and DxLib-free. Ctrl+Z/Y dispatch, safe selection/playhead reset and
reload integration are statically reviewed only. Windows SDK build, input,
notices, rendering and interactive save/undo/redo still require Windows checks.
All 23 portable RESULT groups pass, including every pre-existing test. No Windows
build, DxLib runtime test or smoke capture was performed for this round.

Round 5 app source also passes MinGW C++17 -Wall -Wextra -Wpedantic
-fsyntax-only using temporary hand-declared DxLib stubs outside the repository,
including the new key constants. This verifies C++ syntax only, not actual DxLib
SDK signatures or key definitions.


Per-actor hotkey round: all 23 original portable RESULT groups are retained;
six new groups cover catalog-order animation slots, out-of-range no-ops, actor
selection and edit retention, isolated restarts and independent transports,
chick-only cinematic ownership, and actor-specific help plus save/reload order.
The 15 clips and 44 source character frames are unchanged. JSON insertion order
is retained during parsing/saving so F-key slots do not become alphabetical.
The portable core accepts actor-select, slot 1–6 and cinematic actions. Only the
DxLib boundary translates physical keys. F1–F6 and C were previously unbound;
1/2 selection and bracket cue nudges retain their existing roles.

All 29 portable groups pass. App source passes MinGW C++17 warning-enabled syntax
checking with temporary hand-declared DxLib stubs outside the repository. This
checks C++ syntax, not real SDK declarations, key values, Windows rendering or
interactive input. No Windows build, runtime test or smoke capture was performed.

## MinGW build verified (2026-11-08)

The DxLib app now also builds WITHOUT Visual Studio: `build_mingw.sh` compiles
the unmodified app sources with MinGW-w64 g++ 13 against the official DxLib
3.25a GCC edition (folder `13_1_0_x86_64_w64`). The resulting
`UndergodAnimationStudio.exe` was executed under Wine 9.0 and the complete
studio UI rendered correctly — stage with both actors, timeline panel,
transport controls, and the round-6 header/help line ("Active actor: chick |
1/2 switch actor | F1 idle ... | C sao_roi"). Hotkey behavior itself is
covered by the portable RESULT groups; synthetic key injection into Wine did
not reach DxLib's input polling, so live key presses were not machine-tested.
The Visual Studio build in build.ps1 remains the canonical Windows route.
