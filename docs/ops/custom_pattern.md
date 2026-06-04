## Custom Pattern (XP)

The `XP` family is a single, scene-global **custom pattern**: a grid **8 columns wide by 16 steps deep**. Unlike the regular `P` / `PN` patterns, there is only one `XP` pattern in the firmware, and it is two-dimensional — every step holds 8 values (one per column) rather than one.

Each of the **8 columns is its own independent track**, with its own playhead index, length, start, end, and wrap — exactly like the eight `P` patterns, but sharing one 16-step depth and one editing screen. Every `XP` op therefore takes a **column** `c` (0..7) as its first argument, mirroring the `PN <pattern> <index>` ordering: `XP c i` reads/writes the value at column `c`, index `i`. The dotted sub-ops (`XP.I`, `XP.L`, `XP.START`, `XP.END`, `XP.WRAP`, `XP.HERE`, `XP.NEXT`) all operate on the single column you name.

`XP.NEXT c` advances one column's playhead (honouring that column's start/end/wrap, just like `P.NEXT`). `XP.NEXT.ALL` advances **all eight columns at once**, each by its own range — handy for stepping a chord or a bank of parallel sequences from a single trigger.

This makes `XP` well suited to chord and multi-voice work: read a column per voice (`XP.NEXT.ALL` then `XP.HERE 0`, `XP.HERE 1`, …), or treat each column as an independent parallel sequence.

To edit the custom pattern, switch to the **custom tracker** view with `<scroll lock>` (it has its own dedicated shortcut and is not part of the `<tab>` mode cycle). All 8 columns are shown at once and the 16 steps scroll vertically (8 visible at a time) with the same key bindings as pattern mode; each column draws its own playhead and start/end markers. The custom pattern is saved with the scene.
