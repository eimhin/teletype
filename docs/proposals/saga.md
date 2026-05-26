# Saga stage sequencer (v1)

## Context

Saga (Oxi One) is a duration-based stage sequencer: each stage holds the
playhead for N steps before advancing, instead of marching one cell per
tick. The user wants a "simplified" teletype port — v1 scope is
**per-stage duration only** (no offset, probability, repeat, accumulator,
velocity, gate). Notes still come from existing pattern data; Saga just
adds a dwell layer on top.

Decisions already locked in:

- **Surface**: new ops family `SAGA.*` (script-driven). A pattern-mode UI
  extension is a possible follow-up but **not in v1**.
- **Storage**: persisted in scene state (round-trips through flash + the
  text preset format).
- **Clocking**: external. The user calls an advance op from a script on
  each tick they want Saga to consider.

## Design

### Composition with existing patterns

Teletype already has `scene_pattern_t patterns[8]` (`src/state.h:122–129`)
with `val[64]`, `idx`, `len`, `start`, `end`, `wrap`. That's a perfectly
serviceable note + length + bounds store. Saga **binds to one of the
existing patterns** (selectable) and adds a parallel per-cell duration
array plus a runtime dwell counter.

This avoids duplicating note/length/bounds state and lets the user keep
editing notes with the familiar `P.*` ops and PATTERN mode.

**Key relationship to `P.NEXT`**: Saga does **not** own a separate stage
index. `SAGA.STEP` is a duration-aware `P.NEXT` on the bound pattern —
it bumps an internal `dwell` counter, and only when `dwell` reaches the
current cell's duration does it advance `patterns[saga.pattern_idx].idx`
using the same start/end/wrap logic `P.NEXT` already implements (so the
existing code path can be factored / called). Consequences:

- The "current stage" is just `patterns[saga.pattern_idx].idx`.
- `P.HERE` / `P.I` / `PN.HERE` keep working unchanged and reflect
  Saga's position. There is no `SAGA.HERE` or `SAGA.I` op — they'd be
  aliases.
- Users should call **either** `SAGA.STEP` **or** `P.NEXT` on a given
  clock — not both, or the pattern advances twice. (This is a
  documentation point, not enforced.)
- `SAGA.NEW` becomes the standard way to fire a trigger only on actual
  stage changes.

### New scene state

In `src/state.h`, alongside the existing pattern/turtle/delay state, add:

```c
typedef struct {
    uint8_t  pattern_idx;        // which pattern (0..PATTERN_COUNT-1) drives Saga
    int16_t  dur[PATTERN_LENGTH]; // per-cell dwell in ticks (clamped >=1)
    // runtime-only, not persisted:
    uint16_t dwell;               // ticks elapsed on current cell
    uint8_t  just_advanced;       // 1 on the tick patterns[i].idx just moved, else 0
} scene_saga_t;
```

Note: the current stage index is **not** stored here — it lives in
`patterns[saga.pattern_idx].idx`, the existing pattern playhead.

Add one field to `scene_state_t`:

```c
scene_saga_t saga;
```

Persist the **`pattern_idx` + `dur[]`** portion only. `dwell` and
`just_advanced` are transient like `p_acc_offset` (`src/state.h:277–280`).

A clean way to do this without changing the wire format of
`nvram_scene_t` ad hoc is to introduce a small `scene_saga_persist_t`
(just `pattern_idx + dur[]`) and embed it in `nvram_scene_t`. ~130 bytes
per scene × 12 slots ≈ 1.5 KB total — negligible against the ~176 KB
nvram reservation (CLAUDE.md "Flash budget").

### Op semantics

A single saga per scene. All ops live in `src/ops/saga.c` (new file).

| Op | Stack | Behavior |
|---|---|---|
| `SAGA.P` | get/set | Which pattern (0..7) Saga drives. |
| `SAGA.D` | get/set | Duration at the bound pattern's current `idx`. Clamped ≥ 1. |
| `SAGA.DN` | get/set, takes index | Duration at a given cell index. |
| `SAGA.STEP` | get | Advance Saga by one clock tick. See semantics below. Returns the value at the **current** `patterns[p].idx` after the step. |
| `SAGA.NEW` | get | Returns `just_advanced` — `1` exactly on the tick the index moved. Used to gate triggers. |
| `SAGA.RST` | get | Reset bound pattern's `idx` to its `start` and clear `dwell`. |

**`SAGA.STEP` semantics** (pseudocode — `idx` is `patterns[p].idx`):

```
if dwell == 0:                  // entering a stage (fresh after RST)
    dwell = 1
    just_advanced = 1
else if dwell + 1 > dur[idx]:   // current stage is over, advance
    idx = next_index(idx)       // same start/end/wrap as P.NEXT
    dwell = 1
    just_advanced = 1
else:                           // still holding on current stage
    dwell = dwell + 1
    just_advanced = 0
return val[idx]
```

Key property: `SAGA.NEW` returns `1` on **the first tick of every
stage**, including stage 0 on the very first tick after `SAGA.RST`.
That's what lets a single uniform script line (`IF SAGA.NEW: ...`) fire
the trigger for stage 0 too, without special-casing the start.

Read the current stage / current note with the existing pattern ops:
`P.I` (index), `P.HERE` (value at index), `PN.HERE` (value at index of
pattern N) — there are no `SAGA.HERE` / `SAGA.I` ops because the bound
pattern already exposes them.

This composes cleanly with existing patterns — note edits, pattern bounds,
and length are still managed via `P.*`. Internally, `op_SAGA_STEP_get`
should share the start/end/wrap advancement code with `op_P_NEXT_get`
(refactor it into a static helper in `src/ops/patterns.c` and call from
both) rather than duplicating it.

### Worked example

Goal: a 4-note pattern (C4, E4, G4, C5) where each stage has its own
duration in clock ticks. Saga drives pattern 0; an external clock fires
script 1 on each tick; script 1 advances Saga and sends out CV + a
trigger only on stage boundaries.

**Init script `#I`** (run once when the scene loads):

```
SAGA.P 0          # bind Saga to pattern 0
P.N 0             # make pattern 0 the active pattern for unqualified P ops
P.L 4             # length 4 stages
P.WRAP 1          # loop
P.START 0
P.END 3
P 0 60            # stage 0: C4
P 1 64            # stage 1: E4
P 2 67            # stage 2: G4
P 3 72            # stage 3: C5
SAGA.DN 0 2       # stage 0 holds 2 ticks
SAGA.DN 1 1       # stage 1 holds 1 tick
SAGA.DN 2 3       # stage 2 holds 3 ticks
SAGA.DN 3 1       # stage 3 holds 1 tick
SAGA.RST          # idx = 0, dwell = 0, primed
```

**Script 1** — driven by your external clock (a trigger jack into a
script trigger, or a metro firing `SCRIPT 1`):

```
A SAGA.STEP
IF SAGA.NEW: CV 1 N A; TR.P 1
```

`A SAGA.STEP` stores the current-stage note value into variable `A` and
advances Saga's internal counter. `SAGA.NEW` is `1` exactly on the tick
the stage changed (or the first tick after `RST`), so `CV` + `TR.P` only
fire on stage boundaries — the gate stays open for the full duration of
each stage because nothing turns CV 1 off in between.

**Tick-by-tick trace** of the first loop (durations `[2, 1, 3, 1]`,
notes `[60, 64, 67, 72]`):

| Tick | Before STEP (idx, dwell) | Action inside STEP | After STEP | `SAGA.NEW` | `A` | What the script does |
|---|---|---|---|---|---|---|
| 1 | 0, 0 | enter stage (dwell 0→1) | 0, 1 | 1 | 60 | `CV 1 N 60`, `TR.P 1` |
| 2 | 0, 1 | hold (2 ≤ dur 2) | 0, 2 | 0 | 60 | — |
| 3 | 0, 2 | over (3 > dur 2) → advance | 1, 1 | 1 | 64 | `CV 1 N 64`, `TR.P 1` |
| 4 | 1, 1 | over (2 > dur 1) → advance | 2, 1 | 1 | 67 | `CV 1 N 67`, `TR.P 1` |
| 5 | 2, 1 | hold (2 ≤ dur 3) | 2, 2 | 0 | 67 | — |
| 6 | 2, 2 | hold (3 ≤ dur 3) | 2, 3 | 0 | 67 | — |
| 7 | 2, 3 | over (4 > dur 3) → advance | 3, 1 | 1 | 72 | `CV 1 N 72`, `TR.P 1` |
| 8 | 3, 1 | over (2 > dur 1) → advance (wrap) | 0, 1 | 1 | 60 | `CV 1 N 60`, `TR.P 1` |

Loop length per cycle = `sum(dur) = 7` ticks. Triggers fire on ticks
1, 3, 4, 7, 8, 10, 11, 14, … — exactly at stage onsets.

**Live tweaking**: while running, the user can change durations and
notes from any script (or LIVE mode) — e.g. `SAGA.DN 2 5` to lengthen
stage 2, or `P 1 65` to change stage 1's note. Changes take effect
immediately since Saga reads `dur[idx]` fresh on every `STEP`.

### Editing durations outside the init script

Since v1 has no PATTERN-mode UI for editing `dur[]`, there are three
practical workflows:

1. **LIVE mode** — type `SAGA.DN 0 2` etc. at the LIVE prompt. Instant
   but doesn't survive a scene reload; best for performance tweaks.

2. **Dedicated setup script** — e.g. put the durations in `#8`, then
   run with `SCRIPT 8` from LIVE. Persists with the scene and is
   re-runnable.

3. **Drive durations from a pattern via the `L` iterator** (recommended
   workflow). Store the durations *in another pattern* (say pattern 1),
   then sync them into Saga's `dur[]` with one line:

   ```
   L 0 3: SAGA.DN I PN 1 I
   ```

   `L 0 3` iterates `I` over 0..3, `PN 1 I` reads pattern 1 at index
   `I`, and `SAGA.DN I …` writes that into Saga's duration at the same
   index. Now **pattern 1 *is* the duration editor** — edit values in
   the existing PATTERN mode UI, re-sync with one line. Put that line
   in `#I` so durations refresh automatically when the scene loads.

### Files to add / change

**New files**

- `src/ops/saga.c` — handler functions and `tele_op_t` definitions, in the
  style of `src/ops/queue.c`. Use `MAKE_GET_SET_OP` / `MAKE_GET_OP`
  macros for the table entries.
- `src/ops/saga.h` — forward declarations of the `op_SAGA_*` symbols.

**Wiring (five touchpoints — CLAUDE.md "Adding a new OP")**

1. `src/ops/op.c` — `#include "ops/saga.h"`; append `&op_SAGA_*` entries
   to `tele_ops[]` after the queue family. Order in the master table must
   match the order written by `op_enums.py`.
2. `python3 utils/op_enums.py` — regenerate `src/ops/op_enum.h`. Do not
   hand-edit.
3. `src/match_token.rl` — add token rules in the same style as `Q.*`:
   `"SAGA.STEP" => { MATCH_OP(E_OP_SAGA_STEP); };` etc. Rebuild lets
   ragel regenerate `match_token.c`.
4. Add `../src/ops/saga.c` / `.o` to all three Makefiles:
   - `module/config.mk` (file list near `../src/ops/queue.c`)
   - `tests/Makefile` (object list near `../src/ops/queue.o`)
   - `simulator/Makefile` (object list near `../src/ops/queue.o`)

**Scene state + persistence**

- `src/state.h` — add `scene_saga_t` and `saga` field on `scene_state_t`.
  Add accessors `ss_saga_ptr()` / `ss_saga_size()` mirroring the
  `ss_patterns_ptr` pattern used by flash.
- `src/state.c` — init `saga` to defaults: `pattern_idx = 0`,
  `dur[i] = 1`, `dwell = 0`, `just_advanced = 0`. Wire into the
  existing `ss_init` / `ss_clear_scripts_and_patterns` path so loading a
  fresh slot zeroes Saga state.
- `module/flash.h` — add a `scene_saga_persist_t saga;` field to
  `nvram_scene_t`.
- `module/flash.c` — extend `flash_write()` / `flash_read()` with one
  more `flashc_memcpy` / `memcpy` pair, guarded by `init_pattern`
  (semantically Saga is pattern-adjacent).
- `module/config.mk:261` — only if a build fails with `.flash_nvram
  will not fit`. Expected size delta ≈ 1.5 KB → very unlikely to need a
  bump from 176 KB.

**Scene serialization**

- `src/scene_serialization.c` — add a `#SAGA` section after the pattern
  section. Format: one header line `pattern_idx`, then 64 ints for
  `dur[]` (one per line or space-separated, matching the style already
  used for pattern values). Add the symmetric parse branch in
  `deserialize_scene`. Backwards-compat: deserializer must treat a
  missing `#SAGA` section as "leave defaults", so existing
  `presets/tt*.txt` still load.

### Tests

- New `tests/op_saga_test.c` (greatest suite) covering:
  - `SAGA.STEP` dwells correctly when `dur[idx] > 1` (idx unchanged,
    `SAGA.NEW` is 0).
  - `SAGA.STEP` advances when dwell reaches `dur[idx]` (idx advances,
    `SAGA.NEW` is 1, dwell resets).
  - Advance honors the bound pattern's `start` / `end` / `wrap`.
  - `SAGA.D` clamps `< 1` to `1` (a zero-duration stage would freeze
    the sequencer).
  - `SAGA.RST` returns `patterns[p].idx` to `start` and clears
    `dwell` so the next `SAGA.STEP` after RST does not immediately fire
    a boundary.
  - Writing `P.I` directly (bypassing Saga) does **not** clear `dwell`
    — this is intentional / documented, not a bug; users who want a
    hard reset should call `SAGA.RST`.
- Add the suite to `tests/main.c` alongside the existing suites.
- `tests/serialize_scene` style round-trip: write a scene with a
  populated `dur[]`, read it back, confirm equality. Also confirm
  pre-Saga preset text still deserializes (defaults apply).
- After wiring, run `op_mod_test` — its consistency check
  (`tele_ops[]` vs enum) catches the most common mis-wire.

### Verification

1. `python3 utils/op_enums.py` — regenerate enums.
2. Host tests (CLAUDE.md "Build / Test"): inside `dewb/monome-build`,
   `cd tests && make clean && make test`. Confirm new saga suite passes
   and `op_mod_test` is green.
3. Simulator REPL: `cd simulator && make && ./tt`. Type a small Saga
   program by hand — e.g. set `P 0 60`, `P 1 64`, `P 2 67`, set
   `SAGA.DN 0 2`, `SAGA.DN 1 1`, `SAGA.DN 2 4`, then call
   `SAGA.STEP` repeatedly and watch idx + return values. This is the
   fastest way to confirm `match_token.rl` recognizes every new token
   (CLAUDE.md "Op table gotchas").
4. Firmware build: `cd module && make clean && make`. Check the link
   doesn't blow `.flash_nvram`; spot-check
   `avr32-objdump -h module/teletype.elf | grep flash_nvram`.
5. Round-trip a preset: in the simulator, populate a Saga, save to a
   `.txt` via the existing serialization path, reload it, and confirm
   `SAGA.*` reads back the same values.

## Out of scope (explicit non-goals for v1)

- Per-stage offset, probability, repeat, accumulator, velocity, gate,
  glide, range — the "simplified" framing.
- Polyphony / chords per stage — would require a multi-note-per-stage
  data model that doesn't fit pattern-bound notes.
- PATTERN mode UI for editing `dur[]`. v1 is script-only. A follow-up
  could add an alt-view to `module/pattern_mode.c` showing duration as a
  second value per cell, toggled with a modifier key — but that's a
  separate, larger change touching screen layout and key routing.
- Internal clocking. v1 advances only when the script calls
  `SAGA.STEP`.
