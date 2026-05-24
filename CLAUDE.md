# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Firmware for the monome teletype eurorack module. Implements a small stack-oriented scripting language whose programs are evaluated on an AVR32 microcontroller in response to triggers, grid input, etc.

## Build / Test

The submodule `libavr32` must be checked out (`git submodule update --init --recursive`).

**All `make` commands run inside the `dewb/monome-build` Docker container**, which ships the AVR32 cross-toolchain, `ragel` 6.9, and pinned host libs. The container's entrypoint is `/bin/bash -c` and `WorkingDir` is `/target`. Two invocation forms:

- **Interactive** (drop into a shell, run commands by hand):
  ```bash
  docker run --rm -it --platform linux/amd64 -v "$(pwd)":/target dewb/monome-build bash
  # then inside, paths are relative to /target:
  cd module && make clean && make
  ```
- **Non-interactive** (for hooks, agents, CI — pass the command as one string):
  ```bash
  docker run --rm --platform linux/amd64 -v "$(pwd)":/target dewb/monome-build 'cd module && make clean && make'
  ```

`--platform linux/amd64` matters on Apple Silicon (the image is x86_64).

Targets (run from inside the container):

- Firmware (AVR32 cross-compile, produces `module/teletype.hex`): `cd module && make clean && make`
- Host tests (greatest framework): `cd tests && make clean && make test`
  - If `make test` has line-ending trouble, `make tests && ./tests` works. The tests Makefile builds host-native objects from `src/` — if you've previously built the module, run `make clean` first to avoid mixing AVR32 and host objects.
- Simulator (REPL): `cd simulator && make && ./tt`
- Release zip (`teletype.zip`): `make release` at repo root.
- Format: `make format` (clang-format on uncommitted changes only) or `make format-all`.

There is no per-test target in `tests/Makefile`; the test binary runs all suites. Greatest accepts `-t <pattern>` and `-s <suite>` at runtime to filter, e.g. `./tests -s parser_suite` or `./tests -s op_mod_suite`.

`python3 utils/op_enums.py` (regen of `src/ops/op_enum.h`) has no native deps and can run on the host directly — no container needed.

## Architecture

Three layers, separated so the language core is host-testable independently of hardware:

1. **`src/` — language core (portable C).** Pipeline is:
   - `scanner.rl` → tokens
   - `match_token.rl` → maps token text to op/mod structs
   - `command.c` / `teletype.c` → `parse` → `validate` → `process_command`
   - `state.c` holds `scene_state_t` (regular scripts + metro + init script, plus internal delay/live slots; variables, patterns, stack, delay queue, grid state) and `exec_state_t` (per-call evaluation state, including the value stack). Script slot counts and indices are defined in `src/script.h` — refer to those constants rather than hardcoding numbers.
   - `scene_serialization.c` reads/writes scenes as text (the same format used in `presets/`).
   Evaluation is post-order: each op pops its declared parameters from the stack and may push a return value. Ops can have GET and SET behavior (e.g. `X` reads/writes variable X), controlled by whether they appear before/after `:` in a command.

2. **`src/ops/` — op and mod definitions.** Each file groups related ops; every op is a `tele_op_t` (params in, optional return, get/set funcs) and every mod (a.k.a. PRE — `IF`, `EVERY`, `PROB`, `L`, etc.) is a `tele_mod_t`. The master tables `tele_ops[]` and `tele_mods[]` live in `src/ops/op.c`. `op_enum.h` is **generated** — never hand-edit; run `python3 utils/op_enums.py`.

   To add an op or mod (see README "Adding a new OP or MOD" for the canonical list), edit:
   1. The op file itself (`src/ops/*.c`) defining the `tele_op_t` / `tele_mod_t`.
   2. The master table in `src/ops/op.c` (`tele_ops[]` or `tele_mods[]`).
   3. Regenerate `op_enum.h` via `python3 utils/op_enums.py` — never hand-edit.
   4. Add the token in `src/match_token.rl` and rebuild (ragel regenerates `match_token.c`).
   5. Add the op's `.c`/`.o` to the file lists in `module/config.mk`, `tests/Makefile`, and `simulator/Makefile`.

   The op-mod test verifies this wiring is consistent — if it fails after adding an op, one of these is missing.

3. **`module/` — AVR32 firmware glue.** `main.c` is the hardware event loop. `*_mode.c` files implement the UI modes (LIVE / EDIT / PATTERN / PRESET_R / PRESET_W / HELP / USB_DISK), switched via key handlers. `line_editor.c` is the shared text-input widget used by the modes. `grid.c` handles the optional monome grid; `flash.c` persists scenes to flash. The module depends heavily on `libavr32/` submodule (drivers, USB, font, screen, event queue).

Other notable `src/` subsystems beyond the parse/eval pipeline: `turtle.c` (grid-walk state), `scale.c` (quantization tables), `chaos.c` (drunk walks), `ops/delay.c` (delayed-command queue), `ops/metronome.c` (metro tick), `drum_helpers.c`. Most have matching test suites in `tests/`.

### Key cross-cutting points

- **Hardware abstraction:** ops that touch hardware (TR pulses, CV outputs, I2C peers like Ansible/ER-301/Just Friends/W/, MIDI, etc.) call out through `teletype_io.h` — implemented by `module/` on hardware and by stubs in `tests/` / `simulator/`. Don't `#include` AVR32 headers from `src/`.
- **Scene format:** the text serialization in `scene_serialization.c` is also the on-disk preset format (`presets/tt*.txt`); changes here are a compatibility surface for users' saved scenes. The `serialize_scene` test suite covers round-trips.
- **Ragel files:** edit `*.rl`, then rebuild — the Makefiles run `ragel` to regenerate the `.c`. Don't edit the generated `.c` directly.
- **`script.h` vs `state.h`:** scripts are stored inside `scene_state_t` as arrays of `tele_command_t`; runtime execution does not re-parse.

## Conventions

- C99, 4-space indent, clang-format config at repo root (`.clang-format`). Run `make format` before committing.
- `src/` and `src/ops/` must remain free of AVR32-specific code so host tests and the simulator keep building.
