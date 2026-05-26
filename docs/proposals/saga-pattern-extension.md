# Saga-style stage sequencing via PATTERN extension (v1 alt)

## Context

Alternative surface for the same "Saga" idea (per-stage hold duration on
a step sequence): instead of a new `SAGA.*` ops family bound to one
pattern, **extend the existing pattern data model itself** so *every*
pattern gains a parallel per-cell `dur[]` array and a duration-aware
advance op. Trades a larger blast radius (core struct, flash format,
preset file format, PATTERN mode UI) for a more native feel — durations
are first-class pattern data, editable in PATTERN mode, and any of the 8
patterns can be Saga-style independently.

Locked-in scope for v1 (carried over):
- **Per-stage duration only** — no offset, probability, repeat,
  accumulator, velocity, gate.
- **External clocking** — user calls the advance op from a script on
  each tick.
- **Persisted** in scene state, round-trips through flash + the text
  preset format.

## Design

### Data model

Add `int16_t dur[PATTERN_LENGTH]` to `scene_pattern_t` in `src/state.h`
(currently around line 122). Keep the **runtime** dwell/advance state
out of the persisted struct — mirror the existing precedent at
`src/state.h:277–280` where `p_acc_offset` / `p_acc_count` live on
`scene_state_t` directly, not inside `scene_pattern_t`:

```c
// src/state.h — extends scene_pattern_t (PERSISTED)
typedef struct {
    int16_t  idx;
    uint16_t len;
    uint16_t wrap;
    int16_t  start;
    int16_t  end;
    int16_t  val[PATTERN_LENGTH];
    int16_t  dur[PATTERN_LENGTH];   // NEW: per-cell dwell in ticks (>=1)
} scene_pattern_t;

// src/state.h — extends scene_state_t (RUNTIME-ONLY, not persisted)
uint16_t p_dwell[PATTERN_COUNT];
uint8_t  p_just_advanced[PATTERN_COUNT];
```

Size impact: `scene_pattern_t` grows by 128 bytes (64 × `int16_t`).
Per scene: 8 patterns × 128 = +1 KB. All 12 scene slots: +12 KB.
Comfortably within the ~176 KB `__flash_nvram_size__` reservation
(CLAUDE.md "Flash budget"). If the link fails with `.flash_nvram will
not fit`, bump `module/config.mk:261`.

### Op additions

All in `src/ops/patterns.c`. Names mirror existing pattern ops; all
checked free against the existing `tele_op_t` declarations.

| Op | Stack | Behavior |
|---|---|---|
| `P.D` / `PN.D` | get/set, takes index | Duration at cell index. `P.D 0` reads, `P.D 0 4` writes. Clamped ≥ 1 on write. Mirrors `P` / `PN`. |
| `P.D.HERE` / `PN.D.HERE` | get/set | Duration at current `idx`. Mirrors `P.HERE` / `PN.HERE`. |
| `P.STEP` / `PN.STEP` | get | Duration-aware advance. See semantics below. Returns the value at the **current** `idx` after the step. |
| `P.STEP.NEW` / `PN.STEP.NEW` | get | `1` exactly on the tick `idx` just moved (or first tick after a hard reset). Used to gate triggers. |
| `P.D.RND` / `PN.D.RND` | get, takes `lo hi` | Randomize the duration of every cell in `[start..end]` (honoring the pattern's `start`/`end` markers, same range semantics as `P.SHUF`) to a uniform random value in `[lo..hi]`. `lo` clamped ≥ 1; if `hi < lo`, the args are swapped. Returns nothing meaningful (or `lo` for stack hygiene — match `P.SHUF`'s convention). |

`P.NEXT` is left unchanged — users can still get a one-tick-per-cell
playhead via the existing op, or opt into duration-aware playback with
`P.STEP`. (Calling both on the same clock would double-advance; this is
a documentation point, not enforced.)

**`P.STEP` semantics** — using `dwell` from `p_dwell[pn]`, `idx` from
the bound pattern:

```
if dwell == 0:                   // entering a stage (fresh / after reset)
    dwell = 1
    just_advanced = 1
else if dwell + 1 > dur[idx]:    // current stage is over → advance
    p_next_inc_i(ss, pn)         // reuse existing helper, patterns.c:386
    dwell = 1
    just_advanced = 1
else:                            // still holding on current stage
    dwell = dwell + 1
    just_advanced = 0
return val[idx]
```

**Reused helper**: `p_next_inc_i()` at `src/ops/patterns.c:386` already
handles start/end/wrap correctly for `P.NEXT`. Call it from `P.STEP`
unchanged — no refactor needed beyond exposing it (it's already a
static in the same file).

**Reset semantics**: extend the existing `P.I` / `PN.I` *setters* in
`src/ops/patterns.c` so that writing `idx` also clears `p_dwell[pn]`
(and `p_just_advanced[pn]`) for the same pattern. This gives one clean
rule — *"writing idx invalidates dwell"* — that covers both:

- **Scene start** (`#I` script): `P.I 0` (or `P.I P.START`) is enough.
  Scene state is zero-initialized so dwell starts at 0; the first
  `P.STEP` enters stage 0 with `STEP.NEW=1`.
- **Mid-run reset** (e.g. from a reset jack): `P.I 0` clears dwell, so
  the very next `P.STEP` correctly enters stage 0 again with
  `STEP.NEW=1`, regardless of where the playhead was before.

`P.I -1` is **not** a meaningful reset — `idx` is signed but `val[-1]`
is undefined, and `p_next_inc_i()` (patterns.c:386) clamps out-of-range
indices to 0. Use `P.I 0` (or `P.I P.START`) — always.

No dedicated `P.RST` op needed. No new "current value" op needed —
existing `P.HERE` / `PN.HERE` work as before, and `P.STEP` itself
returns the current value.

### PATTERN mode UI: duration view toggle

`module/pattern_mode.c` currently has no view-mode flag — all key
handlers and the renderer (`screen_refresh_pattern`, ~lines 543–627)
operate directly on `ss_get_pattern_val()` / `ss_set_pattern_val()`.
Add a view-mode toggle so the same column UI shows and edits `dur[]`
instead of `val[]` when active:

```c
static uint8_t pattern_view = 0;  // 0 = values, 1 = durations
```

Implementation:

1. Bind `HID_TILDE` (no modifier) in `process_pattern_keys()` to toggle
   `pattern_view`. This mirrors LIVE mode's existing tilde idiom
   (`module/live_mode.c:570–575`), where `~` toggles `SUB_MODE_VARS`
   ⇄ `SUB_MODE_OFF` — so users get a consistent "tilde = toggle this
   view" feel across modes. `~` is currently unbound in PATTERN mode.
2. Replace direct `ss_get_pattern_val` / `ss_set_pattern_val` calls in
   key handlers and the renderer with a small accessor pair that
   dispatches on `pattern_view` — e.g.
   `pm_get_cell(ss, pn, i)` / `pm_set_cell(ss, pn, i, v)`. About 5–10
   touch points.
3. **No separate header indicator needed.** Pattern values default to
   `0` and durations default to `1`, so the cell contents themselves
   visibly change on toggle — the data *is* the indicator. (Once cells
   are populated, the user toggled the view themselves on the previous
   keypress, so they know which mode they're in.)
4. Clamp duration writes to ≥ 1 in the setter when `pattern_view == 1`.

Min / max / arithmetic transformations (`P.SHUF`, `P.REV`, `P.+`, etc.)
remain val-only for v1 — they don't get duration counterparts. (Open
question worth flagging during review.)

### Persistence — flash format change

`scene_pattern_t` is embedded directly in `nvram_scene_t`
(`module/flash.h:19–24`), so adding `dur[]` to the struct propagates
automatically. **This is a breaking flash-format change** — existing
saved scenes in flash would be reinterpreted as garbage after upgrade.

Mitigation uses existing machinery: bump `FIRSTRUN_KEY` in
`module/flash.c:17` (currently `0x24` → `0x25`). On boot
`is_flash_fresh()` (flash.c:30–32) detects the mismatch and
`flash_prepare()` (flash.c:34–79) wipes + reinitializes all SCENE_SLOTS
with blank scenes. **Users lose their saved scenes on upgrade** — note
in CHANGELOG and the release notes. There's no in-place migration path
worth building for a v1 of an experimental feature.

`flash_write()` / `flash_read()` (flash.c:82–121) need no changes — the
existing `flashc_memcpy` / `memcpy` calls already use `ss_patterns_ptr`
+ `ss_patterns_size()`, which automatically pick up the larger
`scene_pattern_t`.

### Scene serialization (preset text format)

Existing `#P` section in `src/scene_serialization.c:88–141` writes
length / wrap / start / end as four header lines (one tab-separated row
of 8 patterns each), then 64 rows of 8 tab-separated values. The
deserializer state machine tracks `l` (line within `#P`) and `b`
(pattern index).

Add a parallel `#D` section directly after `#P`: 64 rows of 8
tab-separated durations. In `deserialize_scene`, recognize `#D` as a
new section state (`STATE_PATTERN_DURATIONS`) routed from
`STATE_POUND` like every other section marker, mirroring the
value-row portion of the `#P` parser but writing into `dur[]` via a new
`ss_set_pattern_dur()` accessor.

**Backward compatibility**: a missing `#D` section means existing
preset files load with `dur[]` left at the defaults set by
`ss_init()` (durations all 1). The serializer also skips emitting `#D`
when every cell holds the default — so factory presets in
`presets/tt*.txt` round-trip byte-identical without modification.

Defaults on init: `dur[i] = 1` for every cell of every pattern, set in
`ss_init` / `ss_clear_scripts_and_patterns` in `src/state.c`.

### Files to add / change

No new files. All changes land in existing files.

**Core**
- `src/state.h` — `dur[]` on `scene_pattern_t`; `p_dwell` /
  `p_just_advanced` arrays on `scene_state_t`.
- `src/state.c` — init `dur[i] = 1` and runtime arrays to 0 on scene
  clear. Add accessors `ss_get_pattern_dur` / `ss_set_pattern_dur`
  alongside the existing val accessors.

**Ops**
- `src/ops/patterns.c` — handler functions and `tele_op_t` entries for
  `P.D`, `PN.D`, `P.D.HERE`, `PN.D.HERE`, `P.STEP`, `PN.STEP`,
  `P.STEP.NEW`, `PN.STEP.NEW`, `P.D.RND`, `PN.D.RND`. Reuse
  `p_next_inc_i()` at line 386 from `P.STEP`. `P.D.RND` can reuse
  whatever RNG `RAND` / `RRAND` already use (avoids introducing a
  second RNG).

**Five wiring touchpoints** (CLAUDE.md "Adding a new OP") — note that
because we're editing an *existing* op file (`patterns.c`), the three
Makefiles need **no changes**:
1. `src/ops/op.c` — append the new `&op_P_*` / `&op_PN_*` entries to
   `tele_ops[]` in positional order.
2. `python3 utils/op_enums.py` — regenerate `src/ops/op_enum.h`.
3. `src/match_token.rl` — add tokens (e.g. `"P.D.HERE" => { MATCH_OP(E_OP_P_D_HERE); };`).
4. `module/config.mk` — **no change** (patterns.c already in file list).
5. `tests/Makefile` / `simulator/Makefile` — **no change** (same reason).

**PATTERN mode UI**
- `module/pattern_mode.c` — `pattern_view` flag, `HID_TILDE` toggle,
  accessor dispatch (val vs dur).

**Persistence**
- `module/flash.c` — bump `FIRSTRUN_KEY` from `0x24` to `0x25`.
- `module/flash.h` — no change (`scene_pattern_t` grows automatically).

**Scene serialization**
- `src/scene_serialization.c` — emit `#D` section in `serialize_scene`
  (only when any cell holds a non-default duration); parse it in
  `deserialize_scene` with a new `STATE_PATTERN_DURATIONS` routed from
  `STATE_POUND` alongside the existing single-char section letters.

### Tests

- New `tests/op_pattern_step_test.c` covering:
  - `P.STEP` dwells when `dur[idx] > 1` (idx unchanged, `STEP.NEW`=0).
  - `P.STEP` advances when dwell reaches `dur[idx]` (`STEP.NEW`=1, dwell resets).
  - Advance honors `start` / `end` / `wrap` (shares the `p_next_inc_i`
    path with `P.NEXT`, so a quick parity test is enough).
  - `P.D` / `P.D.HERE` clamp `< 1` to `1`.
  - First `P.STEP` after init returns `val[start]` with `STEP.NEW`=1.
  - Writing `P.I` mid-run clears `dwell` for that pattern, so the next
    `P.STEP` enters the new stage with `STEP.NEW`=1 regardless of prior
    dwell state. Verify by running a few `P.STEP`s partway into a
    stage, then `P.I 0`, then one `P.STEP` and assert `STEP.NEW`==1.
  - `P.D.RND lo hi` writes only into cells `[start..end]`; cells
    outside the range are untouched. All written values fall in
    `[lo..hi]` (and ≥ 1). `lo > hi` is handled (swap or treat as
    `[hi..lo]`).
- `tests/serialize_scene_test.c` (or similar) — round-trip a scene
  with populated `dur[]`. Also confirm a pre-existing preset text
  (no `#D`) loads with default durations.
- After wiring, `op_mod_test` is the consistency check on
  `tele_ops[]` vs the regenerated enum (CLAUDE.md "Op table gotchas").

### Verification

1. `python3 utils/op_enums.py` — regenerate enums.
2. Host tests inside `dewb/monome-build`:
   `cd tests && make clean && make test`. New step suite + `op_mod`
   green.
3. Simulator REPL: `cd simulator && make && ./tt`. Set a pattern, set
   durations via `P.D`, call `P.STEP` repeatedly, watch `P.I` and the
   returned value. Confirms `match_token.rl` recognizes every new
   token.
4. Firmware build: `cd module && make clean && make`. Check
   `avr32-objdump -h module/teletype.elf | grep flash_nvram` against
   the reservation in `module/config.mk:261`.
5. **Migration smoke test on hardware**: flash the new firmware to a
   teletype that has saved presets, confirm `flash_prepare()` wipes
   them cleanly (no garbage scenes, no crashes), and the user gets a
   fresh slate. Reload a known-good preset text file via USB disk →
   confirm `dur[]` deserializes correctly (or defaults if `#D`
   absent).
6. PATTERN mode: enter mode, press `~` to switch to duration view, edit
   some durations, press `~` again to confirm it toggles back, exit
   to LIVE, run `P.STEP` in a script, confirm timing matches.

## Tradeoffs vs the SAGA.* ops approach

This plan is bigger and more invasive than the `SAGA.*` ops approach
on three axes:

- **Blast radius**: touches `scene_pattern_t` (the core sequencer
  struct), all 12 saved scenes on every user's hardware (forced wipe
  via `FIRSTRUN_KEY`), the preset file format, and PATTERN mode UI.
  The `SAGA.*` approach was strictly additive.
- **Storage**: +12 KB across all scenes vs ~1.5 KB for the bound-saga
  approach.
- **Per-user disruption**: existing presets in flash are wiped on
  upgrade. User-saved preset text files are forward-compatible (load
  with default durations) but not backward-compatible (a v1-saved
  preset won't load on old firmware).

In return:
- Durations are first-class data, editable in the existing PATTERN
  mode UI.
- All 8 patterns can have independent durations — no "which pattern is
  Saga" binding.
- One mental model — patterns gain a duration column, period.
- 5 op stems × 2 P/PN variants = 10 new ops, all in the existing
  `P.*` namespace, vs 6 ops under a new `SAGA.*` namespace.

## Out of scope (same as the SAGA.* plan)

- Per-stage offset, probability, repeat, accumulator, velocity, gate,
  glide, range.
- Polyphony / chords per stage.
- Internal clocking. `P.STEP` only advances when a script calls it.
- In-place flash migration. `FIRSTRUN_KEY` bump wipes existing scenes.
