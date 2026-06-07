## Custom Pattern (XP)

The `XP` family is a single, scene-global **custom pattern**: a grid **7 columns wide by 16 steps deep**. Unlike the regular `P` / `PN` patterns, there is only one `XP` pattern in the firmware, and it is two-dimensional — every step holds 7 values (one per column) rather than one.

Each of the **7 columns is its own independent track**, with its own playhead index, length, start, end, and wrap — just like the `P` patterns, but sharing one 16-step depth and one editing screen. Every `XP` op therefore takes a **column** `c` (0..6) as its first argument, mirroring the `PN <pattern> <index>` ordering: `XP c i` reads/writes the value at column `c`, index `i`. The dotted sub-ops (`XP.I`, `XP.L`, `XP.START`, `XP.END`, `XP.WRAP`, `XP.HERE`, `XP.NEXT`) all operate on the single column you name.

`XP.NEXT c` advances one column's playhead (honouring that column's start/end/wrap, just like `P.NEXT`). `XP.NEXT.ALL` advances **all seven columns at once**, each by its own range — handy for stepping a chord or a bank of parallel sequences from a single trigger.

This makes `XP` well suited to chord and multi-voice work: read a column per voice (`XP.NEXT.ALL` then `XP.HERE 0`, `XP.HERE 1`, …), or treat each column as an independent parallel sequence.

Each column also has a per-cell **duration** grid, exactly like the regular patterns' `P.D` / `P.STEP`. `XP.D c i` gets/sets the dwell duration (in ticks, minimum 1) of column `c`'s cell `i`, and `XP.STEP c` advances that column only once the current cell's duration has elapsed — returning the same value while it dwells, and the next cell's value when it moves on. `XP.STEP.NEW c` reports whether the last step just changed cells, and `XP.STEP? c` advances and returns that flag in one op. `XP.STEP.ALL` dwell-steps every column at once (the duration-aware twin of `XP.NEXT.ALL`). The `XP.D.RND` / `XP.D.RND.M` / `XP.D.RND.N` ops randomise a column's durations.

To edit the custom pattern, switch to the **custom tracker** view with `<scroll lock>` (it has its own dedicated shortcut and is not part of the `<tab>` mode cycle). All 7 columns are shown at once and the 16 steps scroll vertically (8 visible at a time) with the same key bindings as pattern mode; each column draws its own playhead and start/end markers. Press `~` to toggle the grid between **values** and **durations** (the same tilde toggle as the regular pattern tracker). The custom pattern is saved with the scene.
