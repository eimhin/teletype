---
name: add-op
description: Add a new OP or MOD to the teletype scripting language. Walks through the five wiring touchpoints (op struct, master table, generated enum, ragel token, three Makefiles) and runs the verification test. Invoke as /add-op <NAME> [in <file>].
disable-model-invocation: true
---

# Adding a new OP or MOD

Adding an op touches five places. Missing any one breaks the build or the `op_mod_tests` suite at runtime. This skill walks the user through them in order.

## Inputs to gather first

Before touching files, confirm with the user:

1. **Name** — e.g. `FOO`, `FOO.BAR`. Dots are allowed in op names; they map to `_` in the C symbol (`op_FOO_BAR`).
2. **Type** — OP or MOD? MODs are `IF`, `EVERY`, `PROB`, `L`, etc. — they wrap a following command. Most additions are OPs.
3. **Arity** — how many values it pops from the stack (0–3 typical).
4. **GET / GET+SET** — does it return a value (GET only) or also accept assignment via `X : op`? Use `MAKE_GET_OP` vs `MAKE_GET_SET_OP`.
5. **Returns a value?** — passed as the last arg to the macro (`true`/`false`).
6. **Home file** — which `src/ops/*.c` does it belong to? Group with siblings (e.g. arithmetic → `maths.c`, hardware I/O → `hardware.c`). If genuinely new domain, create a new pair `src/ops/<name>.{c,h}`.

## The five touchpoints

### 1. Op struct in `src/ops/<file>.c` (+ header decl)

In the `.c`:

```c
static void op_FOO_get(const void *data, scene_state_t *ss,
                       exec_state_t *es, command_state_t *cs);

const tele_op_t op_FOO = MAKE_GET_OP(FOO, op_FOO_get, /*params*/ 2, /*returns*/ true);

static void op_FOO_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t b = cs_pop(cs);  // values pop in reverse push order: top of stack first
    int16_t a = cs_pop(cs);
    cs_push(cs, /* result */);
}
```

Wrap any parameter the op body doesn't reference in `NOTUSED(...)` (from `src/helpers.h`) — it's the repo convention and silences `-Wunused-parameter`.

For GET+SET use `MAKE_GET_SET_OP(FOO, op_FOO_get, op_FOO_set, params, returns)`. For MODs use `MAKE_MOD(FOO, op_FOO_func, params)` and `tele_mod_t`.

In the `.h` add: `extern const tele_op_t op_FOO;`

**Reference examples in repo:**
- Simple arithmetic: `src/ops/maths.c` `op_ADD`
- GET+SET with sub-fields: `src/ops/maths.c` `op_R`, `op_R_MIN`
- MOD: `src/ops/controlflow.c` `mod_IF`

### 2. Master table in `src/ops/op.c`

Add `&op_FOO,` to `tele_ops[]` (or `tele_mods[]` for MODs), grouped with siblings from the same source file. The table's order doesn't matter for correctness but the convention is to keep file-local ops together.

### 3. Regenerate `src/ops/op_enum.h`

**Never hand-edit this file.** Run:

```bash
python3 utils/op_enums.py
```

This regenerates `E_OP_FOO` (or `E_MOD_FOO`) from the `tele_ops[]` / `tele_mods[]` tables.

### 4. Ragel token in `src/match_token.rl`

Add a line in the appropriate alphabetical-ish group:

```ragel
"FOO"   => { MATCH_OP(E_OP_FOO); };
```

For MODs use `MATCH_MOD(E_MOD_FOO)`. Note: if your op name contains a dot (`FOO.BAR`), match the literal token `"FOO.BAR"`.

After editing, the next build runs ragel and regenerates `src/match_token.c`. Do not hand-edit `match_token.c`.

### 5. Build file lists (only if a new `.c` was added)

If you created a new `src/ops/<newfile>.c`, add it to all three. **Paths are relative to each Makefile's own directory**, so they use the `../src/ops/` prefix:

- `module/config.mk` — append `../src/ops/<newfile>.c` to the CSRCS list
- `tests/Makefile` — append `../src/ops/<newfile>.o` to the `tests:` recipe object list
- `simulator/Makefile` — append `../src/ops/<newfile>.o` to `OBJ`

If you only added an op to an existing file, skip this step.

## Documentation (optional but expected for user-facing ops)

Add an entry under the appropriate section in `docs/ops/`. This is markdown the doc build consumes; CI builds the PDF but won't fail if it's missing.

## Verification

**All `make` commands run inside the `dewb/monome-build` Docker container**, not on the host. The image's entrypoint is `/bin/bash -c` and `WorkingDir` is `/target`, so two forms work:

```bash
# Interactive (recommended when working by hand):
docker run --rm -it --platform linux/amd64 -v "$(pwd)":/target dewb/monome-build bash
# then: cd tests && make clean && make test
```

```bash
# Non-interactive (single command, useful for automation):
docker run --rm --platform linux/amd64 -v "$(pwd)":/target dewb/monome-build 'cd tests && make clean && make test'
```

Either way, the `op_mod_tests` suite is the authoritative check for the five-touchpoint wiring.

If `make test` fails with line-ending errors (a known issue with the greatest test runner script), fall back to `make tests && ./tests`. Filter to one suite with `./tests -s op_mod_suite` or one test with `./tests -t <pattern>`.

`python3 utils/op_enums.py` (step 3) is pure Python with no native deps, so it can run on the host or in the container.

If you had previously built the module (`module/`), `make clean` here is required to drop AVR32 objects before rebuilding host-native.

For an end-to-end sanity check, the simulator gives a REPL on the parser/interpreter (also inside the container — interactive form needed since `./tt` is itself a REPL):

```bash
docker run --rm -it --platform linux/amd64 -v "$(pwd)":/target dewb/monome-build bash
# then inside:
cd simulator && make && ./tt
> FOO 2 3   # should evaluate
```

## Common failures

- **`op_mod_tests` fails** → one of the five touchpoints is missing. Use the `op-adder` subagent (`Task` with `subagent_type: op-adder`) to audit which one.
- **Token not recognized in simulator** → step 4 (ragel) was skipped, or ragel didn't re-run. Force a clean rebuild inside the container: `cd module && make clean && make` (paths are relative to `/target`, the container's working dir).
- **`E_OP_FOO` undefined** → step 3 (`op_enums.py`) wasn't run after editing step 2.
- **Module builds but tests don't link** → step 5: missing entry in `tests/Makefile`.

## Flow

1. Gather inputs (name, type, arity, get/set, file).
2. Edit step 1 (`.c` + `.h`).
3. Edit step 2 (`op.c` table).
4. Run step 3 (regen script).
5. Edit step 4 (`match_token.rl`).
6. If new file: do step 5 (three Makefiles).
7. Run `make format` (inside the container) to format C changes. If `git-clang-format` isn't present in the image, fall back to `clang-format -style=file -i <changed files>` on the host (mind version skew — match the version CI uses).
8. Run verification inside the container: `cd tests && make clean && make test` (paths are relative to `/target`).
9. If failing, invoke the `op-adder` subagent to audit wiring.
