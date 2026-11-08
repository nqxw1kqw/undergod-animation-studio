# Undergod Animation Studio v1

A standalone C++17 / DxLib animation bench for Undergod Card. Pick an actor and
clip, play or scrub it, edit JSON, and press **R** to reload. The core runs without
DxLib; the desktop app requires Windows. See [BUILD_NOTES.md](BUILD_NOTES.md) for
verification limits and provenance.

## Delivered inventory

| Actor | Frame clips | Composed clips | Cinematic |
| --- | --- | --- | --- |
| chick | idle, attack, hit, defend, ult_charge, ult_throw | defeat, victory | sao_roi (shared scene) |
| frog | idle, attack, hit, defend, ult_charge | defeat, victory | sao_roi (shared scene) |

Every frame clip has four existing PNGs. Defeat uses the actor's four hit frames
with fade, tilt, grayscale, darkening, and dust. Victory uses its four idle frames
with the game's survivor bounce sampled into keys. No new character art is used.
The cinematic uses the primary and target pickers; a missing primary pose falls
back to idle (frog has no ult_throw).

The import contains 44 character frames, two 512×512 VFX images, four 256×256 card
images, STYLE.md, and five unchanged game data JSONs. `data/clips.json` is the new
studio animation catalog. The imported gameplay balance/card JSONs are retained
for completeness; the studio does not run combat.

## Build and run

Linux / GCC (C++17, no external packages or DxLib):

```sh
./tests/run_portable.sh
```

Windows: install Visual Studio 2022 with **Desktop development with C++**, the
Windows SDK, and the VS-compatible x64 DxLib distribution. Supply the directory
containing `DxLib.h` and its libraries:

```powershell
.\build.ps1 -Configuration Release -DxLibDir C:\DxLib
.\verify.ps1 -Configuration Release -DxLibDir C:\DxLib
.\bin\Release\UndergodAnimationStudio.exe
```

You can also open `UndergodAnimationStudio.sln` and set the `DxLibDir` MSBuild
property. Both Debug and Release target x64, v143, C++17 and the static CRT, matching
the game. DxLib's headers supply its library linkage. The solution builds the
studio and a separate console `PortableChecks.exe` without DxLib dependencies.

Build output goes to `bin/<Configuration>/`; assets and data are copied beside
the executables. **The app always uses data beside its executable**, independent
of the launch directory. Edit those runtime JSONs for live iteration, or edit
repo `data/` and rebuild to copy changes. Rebuilding can replace runtime edits.
The console checks use their current working directory; `verify.ps1` sets it to
the output directory before running them.

`verify.ps1` builds, runs portable checks, then launches `--smoke` with a 30-second
timeout. The smoke run decodes all 46 stage images, renders all 15 actor clips,
six cinematic times and reduced effects, and writes PNGs plus RESULT lines to
`bin/<Configuration>/artifacts/`. Inspect the PNGs and exercise the controls on
Windows; capture success alone does not prove visual correctness.

## Controls

Buttons provide the same actions as the keyboard. The stage uses the game's
1440×360 arena, 0.5 actor scale, foot pivot, and side-based facing.

| Input | Action |
| --- | --- |
| 1 / 2 | Select chick / frog, retaining the clip if available, otherwise idle |
| A | Cycle actors (also includes actors added to the catalog) |
| Tab | Switch timeline panel between current clip and cinematic; choose clips with buttons |
| Space | Play / pause; playing again from a finished nonlooping clip restarts it |
| Home | Restart at zero, retaining play/pause state |
| J / L | Restart and pause / resume at next speed step (maximum 3×) |
| P | Toggle loop / play once and freeze on the last frame, retaining local time |
| Page Down / Page Up | Decrease / increase speed by 0.25, clamped to 0.25×–3× |
| Left / Right | Pause and step to previous / next frame boundary |
| Shift + Left / Right | Pause and scrub by 50 ms |
| Drag timeline | Pause and scrub to an absolute local time |
| End | Pause at final pose (just before wrap when looping) |
| K | Skip a skippable cinematic to its clean end |
| S / F / V / D | Toggle shake / flash / particles / dim |
| E | Export primary clip trace, BMP sheet and JSON sidecar |
| Shift+E | Toggle reduced effects master |
| R | Reload both clip and cinematic data and images; restart time at zero on success |
| Up / Down | Select a frame or cue row in the timeline panel (scrolls with selection) |
| [ / ] | Nudge selected cue −/+10 ms; Shift uses 50 ms |
| , / . | Shorten / lengthen selected frame by 10 ms (minimum 20 ms) |
| Ctrl+S | Validate and save all edited clips to runtime data/clips.json |
| O | Toggle onion skin for the selected clip (previous 35%, next 20%, beneath current frame) |
| B | Cycle arena / dark / checkerboard backdrop |
| G | Toggle actor origin crosshair and floor line |
| Esc | Exit |

Cinematic arrow stepping uses 125 ms increments, matching the game's 8 fps impact
sheet. Character stepping honors each frame's individual duration. Time and frame
readouts are zero-based; active cues describe the timeline even when their visual
effect is disabled. Looping wraps the frames and tracks and repeats cues once per
cycle. Terminal clips default to nonlooping and hold their final pose.

Reduced effects suppresses shake, flash, particles, charge pulsing and victory
bounce, locks camera zoom to 1, and attenuates dim to 70/255 of normal, following
the game's shared setting. Individual effect choices are retained when the master
is turned off. Sprite/VFX art, popups, and the defeat fade/tilt remain visible.

A successful reload displays a three-second notice. Parse, validation or image
loading errors keep the entire previous library and image cache and display a
persistent error until the next reload. Initial startup without valid data shows
an error dialog and exits. Reload never writes JSON.

Onion skin uses the current pose and actor pivot for neighboring sprites. Neighbors
wrap when transport looping is on; nonlooping endpoints omit the absent neighbor.
It is disabled for the shared cinematic.

Stage guides mark the untransformed actor foot origin at stage y=320 (both actor
origins in the cinematic). The floor line and crosshair share the stage camera,
so zoom and shake move them with the scene. Backdrop and guides persist across
clip selection and reload.

## In-app clip editing

Select an actor and clip, then use Up/Down in the timeline panel to select a
frame or cue. Frames appear in sequence with zero-based index, file and duration;
cues follow in chronological order with time and type. Editing pauses playback.
Cue nudges clamp to [0, clip duration], stable-sort ties and retain selection.
Duration edits clamp to 20 ms, proportionally retime transform tracks to the new
clip length (preserving loop endpoints), and clamp cues to the new end.

Ctrl+S validates the complete catalog, writes a temporary sibling and atomically
replaces `data/clips.json`. On failure the destination stays untouched and a
persistent error appears; successful save makes in-memory clips match reload.
Edits remain in memory until saved; R discards them by reloading the runtime
files. Save before switching away or exiting if you want to retain your edits;
switching clips itself retains edits. Tab switches the panel to cinematic steps (time, action and parameters). Up/Down
selects a step; [ / ] nudges it by 10 ms, or 50 ms with Shift, clamped to
[0, current cinematic duration], with stable sorting and selection retention.
Ctrl+S saves the panel's file transactionally after validation. Moving impact or
spear fall alone can violate contact timing; adjust both before saving. Invalid
edits remain visible in memory, the error persists, and disk bytes stay untouched.
R discards edits. Copy saved runtime JSONs back to repo data/ before rebuilding.


## Layout

```text
assets/pastel/          unchanged characters/, vfx/, cards/, STYLE.md
data/                 five imported JSONs + studio clips.json
include/animator.hpp  portable clip, sampler and cinematic visual API
include/quest_timeline.hpp  reused game timeline cursor
src/animator.cpp      validation, keyframe sampling, cue intervals, cinematic poses
src/cinematic_data.cpp adapted game cinematic parser
src/main.cpp          Windows stage, input, transport, reload, smoke captures
tests/                portable checks and GCC runner
third_party/nlohmann/ game-vendored JSON header and MIT license
*.sln, *.vcxproj       VS 2022 x64 studio and console checks
build.ps1, verify.ps1 Windows build and verification entry points
```

## Clip format

`data/clips.json` has `version: 1` and `actors`, a map of actor IDs to clip maps.
Frame paths are relative to the application root, including `assets/pastel/`.
All times and durations are **milliseconds**; x/y use game stage pixels, rotation
uses radians, RGB tint uses 0–255, alpha/desaturate use 0–1, scale is a multiplier
of the game's base 0.5. An example clip inside an actor map:

```json
{
  "loop": false,
  "composed": true,
  "frames": [
    {"file": "assets/pastel/characters/char_chick_hit_0.png", "duration": 200},
    {"file": "assets/pastel/characters/char_chick_hit_1.png", "duration": 200},
    {"file": "assets/pastel/characters/char_chick_hit_2.png", "duration": 200},
    {"file": "assets/pastel/characters/char_chick_hit_3.png", "duration": 200}
  ],
  "tracks": {
    "rotation": [{"t": 0, "value": 0}, {"t": 800, "value": 0.65}],
    "alpha": [{"t": 0, "value": 1}, {"t": 800, "value": 0.15}]
  },
  "cues": [{"t": 0, "action": "particles", "duration": 800, "value": 12}]
}
```

Frames must exist and have positive durations; their sum defines clip length.
Optional `tracks` keys: `x`, `y`, `scale`, `rotation`, `r`, `g`, `b`, `alpha`,
`desaturate`. A track is an ordered, nonempty key list with strictly increasing
`t` in [0, clip length]. Values hold before/after the first/last key. Each key's
`ease` controls interpolation **into that key**: `linear` (default), `smooth`
(`u*u*(3-2*u)`), `inQuad` (`u*u`), `outQuad` (`1-(1-u)^2`). Looping tracks must
start at 0 and end at the clip length with matching values, ensuring continuity.
The discrete sprite sequence wraps from its last frame to its first.

Cues require `t`, `action`, positive `duration`, nonnegative `value`; `text` is
optional. Supported actions and values:

| Action | Meaning of value |
| --- | --- |
| shake | Pixel amplitude; sin(time × 95) with the game's 180 ms tail attenuation |
| flash | White overlay opacity, fading over duration (normally 0–1) |
| particles | Count, rendered with deterministic velocities and game ballistic gravity; defeat uses terminal dust velocities |
| popup | Display amount metadata; `text` supplies the displayed string, rising at 70 px/s |
| dim | Overlay opacity held for duration (normally 0–1) |
| zoom | Target camera multiplier, easing out from 1 during duration |
| impact | Choreography contact marker; duration/value use the standard cue fields |

Cue times must be in [0, clip length], including an editable end-boundary cue. Cues may outlive a clip; they remain visible
while paused at its end, and looping can overlap a previous cycle's cue. Keep
values at practical rendering scales. Cues are stable-sorted by time, preserving
file order for ties. Unknown tracks, actions or easing names fail validation.

`SamplePose` is pure. `SampleInterval(clip, previous, now, loop)` returns a pose
and fired cue identities for the half-open interval `(previous, now]`, ordered by
absolute time, then file order. Use previous = -1 to include time-zero cues.
`Player` provides the same behavior with a cursor; repeat advances at the same
time emit nothing. Advance monotonically; use `Seek` to reposition without
firing cues or `Restart` for a fresh playthrough. Each fired identity includes
its cue index, loop cycle and absolute time. Stateless sampling reconstructs
visuals for scrubbing and does not deliver external combat callbacks.

`cinematic.json` retains the game's format and all nine actions: `dimScreen`,
`cameraZoom`, `playSprite`, `moveSpear`, `impact`, `spawnVFX`, `shake`, `flashWhite`,
`end`. It requires one `applyDamage` impact, one final end and one spear fall
ending at impact. The sheet is the game's 2×2 `vfx/spear_impact.png`, up to four
frames at 8 fps. Target names remain `player`/`enemy`; spear phases are `rise` and
`fall`. The shared portable `TimelinePlayer` preserves once-only impact/end
callbacks, including skip; the studio visual sampler reconstructs that timeline
without applying gameplay damage.

## Add an actor or clip

1. Add your existing 512×512 transparent PNG frames to `assets/pastel/characters/`.
   Keep the game's `char_<actor>_<clip>_<index>.png` convention. Do not replace
   imported art as part of composing terminal effects.
2. Add a clip object under `actors.<actor>` in `clips.json`, listing each frame and
   duration, the loop flag, and optional tracks/cues. Use `composed: true` when
   reusing frames with a new treatment. Each actor should have an `idle` clip;
   retain chick/frog and any clip referenced by the cinematic.
3. Rebuild to copy repo changes, or edit the runtime copy; press R. A cycles to
   newly added actors, and the clip buttons enumerate their clips. T cycles the target picker.
   Actors use the existing foot pivot/scale and face toward the opposing side.
4. Run portable checks. Their delivered-inventory assertions intentionally pin
   the 15 v1 clips, 44 unique character frames and four-frame naming/order;
   update those assertions and the smoke image count when deliberately extending
   the inventory. Adjust picker layout if adding more than 18 clips.

Round 3 dual stage: A or 1/2 picks the primary on the left; T (or the target
button) cycles the independent target on the right. X previews an attack exchange
and returns both to idle. Attack, ult_charge and ult_throw trigger target hit at
the first `impact` cue; absent a cue, contact defaults to the clip end. Sao Roi
uses its cinematic impact step, then target hit and composed defeat when available.
Actor facing follows stage side. The cinematic supports either picker, using idle
when an actor lacks a requested pose.

The scrubber marks contact in red (the documented end fallback when no impact cue
is present). P once mode holds the final frame and stops; Space restarts a
finished clip, while J resets and pauses. L increases speed by 0.25× and resumes.

## Portable exports

`include/export.hpp` exposes `ExportTrace(clip, path)` and
`ExportContactSheet(clip, decoder, bmpPath, jsonPath, onePerRow=false)`.
Callers choose output paths and create their parent folders. The decoder supplies
top-down RGBA8 source pixels, all with the same positive dimensions; the engine
has no image-decoder or DxLib dependency. Sheet export uses original frames,
without transform tracks, tint or effects. It lays frames in row-major order in
fixed source-size cells, with ceil(sqrt(frame count)) columns by default, or one
column for tall strips. Unused cells in the final row are transparent.

Trace JSON contains actor/clip identity and samples at zero, every frame start,
and the final clip boundary, for one nonlooping pass even for looping clips.
Each sample records milliseconds, zero-based frame index, source file, all nine
interpolated pose values and cues fired since the previous sample (including
zero-time cues). Sampling uses playback's `SampleInterval`; terminal samples
hold the final frame. Cues include index, cycle, firing time and payload.

Sheets are uncompressed 32-bit BGRA BMPs with a BITMAPV4HEADER, explicit alpha
mask and sRGB color space. The JSON sidecar gives clip/actor identity, frame
count, cell and sheet sizes, grid rows/columns, and each source frame's top-left
pixel rectangle and duration in milliseconds. All artifacts use format version 1.

Press **E** to export the selected clip (including unsaved in-memory edits) to
`exports/<actor>_<clip>/trace.json`, `sheet.bmp`, and `sheet.json` beside the
executable. Folders are created as needed; repeated exports replace these files.
The notice lists all three relative paths for ten seconds. IDs with punctuation
are percent-encoded in directory names. During an X exchange, export uses the
primary actor's attack clip, independently of the target's reaction. During Sao
Roi it exports the primary actor's currently sampled sprite clip (including the
idle fallback), rather than the shared scene. E leaves playback running.
Shift+E or the Reduced button toggles reduced effects.

The app decodes source PNGs through DxLib's soft-image API, then calls the
portable sheet writer. BMP is always produced; no optional PNG export is added.
Decode/write failures display a persistent error; an interrupted or failed export
may leave partial output, so check the success notice before using the files.
The in-app export path and DxLib adapter are statically verified only on this
host; Windows runtime verification is still required.

Round 5 structural editing: in the clip panel, Ins or N duplicates the selected
frame immediately after itself, copying its file and duration. Del removes it;
the last frame cannot be removed. Ctrl+D copies the current clip in the same
actor as `<id>_copy`, then `_copy2`, `_copy3`, etc. and selects the copy.
Structural edits pause playback and reset time to zero. Like duration edits,
they proportionally retime tracks, preserve loop endpoints, and clamp cue start
times to the new clip end without changing cue duration or tie order. Ctrl+S
uses the same transactional catalog validation; failed saves leave disk untouched.

Ctrl+Z undoes and Ctrl+Y redoes clip cue nudges, frame durations, structural
changes and cinematic step nudges across both panels in session order. History
holds the latest 50 changed operations; a new edit clears redo, and successful
R reload clears both stacks. Saving retains history. Each operation restores the
exact affected file document; the other file is preserved. Undo/redo pauses,
exits exchange mode, resets selection and playhead to zero, restores the selected
clip's loop default and falls back to idle if the selected copy no longer exists.
There is no dirty indicator; undo after saving can leave unsaved changes, so use
Ctrl+S to persist the desired state before exiting.
