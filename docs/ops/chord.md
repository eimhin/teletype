## Chord / Key

The `KEY` family bakes a small library of musically coherent **banks** ("keys") into the firmware for use with i2c2midi. Each bank bundles, all rooted on C (semitone 0): a scale, a palette of C-rooted chord shapes that fit that scale, and an ordered list of voice-led voicings over a held C pedal. All three ops return the reverse-binary decimals consumed by i2c2midi's `I2M.C.B` (the value is the sum of `2^semitone` for each note).

The op set holds one piece of state — the **active bank index**. `KEY n` selects bank `n` as the active context *and* returns its scale; `KEY.C` and `KEY.V` then read shapes / voicings from whichever bank is active. This mirrors the `P` / `P.N` / `P.L` relationship: a bare op carrying context plus dotted sub-ops that read it. Re-selecting the key live (`KEY 3`) re-points everything with a single op.

The active bank is **transient** — it defaults to `0` (Major) at init and on scene load, and is not persisted; re-select it in the init script (`#I`) on each scene.
