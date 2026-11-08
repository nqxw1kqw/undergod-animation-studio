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
The cinematic always features chick as player and frog as enemy, regardless of
which actor picker was selected; it is not a frog ult_throw clip.

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
1440×360 arena, 0.5 actor scale, foot pivot, and mirrored frog.

| Input | Action |
| --- | --- |
| 1 / 2 | Select chick / frog, retaining the clip if available, otherwise idle |
| A | Cycle actors (also includes actors added to the catalog) |
| Tab | Cycle all clips and sao_roi |
| Space | Play / pause; playing again from a finished nonlooping clip restarts it |
| Home | Restart at zero, retaining play/pause state |
| L | Toggle looping, retaining local time |
| Page Down / Page Up | Decrease / increase speed by 0.25, clamped to 0.25×–3× |
| Left / Right | Pause and step to previous / next frame boundary |
| Shift + Left / Right | Pause and scrub by 50 ms |
| Drag timeline | Pause and scrub to an absolute local time |
| End | Pause at final pose (just before wrap when looping) |
| K | Skip a skippable cinematic to its clean end |
| S / F / P / D | Toggle shake / flash / particles / dim |
| E | Toggle reduced effects master |
| R | Reload both clip and cinematic data and images; restart time at zero on success |
| Up / Down | Select a frame or cue row in the timeline panel (scrolls with selection) |
| [ / ] | Nudge selected cue −/+10 ms; Shift uses 50 ms |
| , / . | Shorten / lengthen selected frame by 10 ms (minimum 20 ms) |
| Ctrl+S | Validate and save all edited clips to runtime data/clips.json |
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
switching clips itself retains edits. Cinematic rows are read-only and Ctrl+S
there does nothing. `cinematic.json` is never written. Copy saved runtime
`clips.json` back to repo `data/` before rebuilding if you want it in source control.

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
   newly added actors, and Tab/the clip buttons enumerate their clips. Additional
   actors use the player-side pivot/scale; frog alone uses enemy mirroring. The
   imported sao_roi scene remains specifically chick/frog.
4. Run portable checks. Their delivered-inventory assertions intentionally pin
   the 15 v1 clips, 44 unique character frames and four-frame naming/order;
   update those assertions and the smoke image count when deliberately extending
   the inventory. Adjust picker layout if adding more than 18 clips.
