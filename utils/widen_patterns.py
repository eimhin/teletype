#!/usr/bin/env python3
"""One-shot script: widen the #P block of teletype preset scenes from 4 to 8
pattern columns. Inside the #P block, every line is a tab-separated row of
pattern values; we append four extra '\t0' columns. Blank lines pass through.

Run once after bumping PATTERN_COUNT from 4 to 8:
    python3 utils/widen_patterns.py presets/tt*.txt
"""
import sys


OLD_COLS = 4
NEW_COLS = 8


def widen(text: str) -> str:
    out = []
    in_pattern = False
    for line in text.split("\n"):
        if line == "#P":
            in_pattern = True
            out.append(line)
            continue
        if in_pattern and line.startswith("#"):
            in_pattern = False
        if in_pattern and line and "\t" in line:
            cols = line.split("\t")
            if len(cols) == OLD_COLS:
                cols.extend(["0"] * (NEW_COLS - OLD_COLS))
                line = "\t".join(cols)
        out.append(line)
    return "\n".join(out)


def main(paths: list[str]) -> None:
    for path in paths:
        with open(path, "r") as fh:
            original = fh.read()
        widened = widen(original)
        if widened == original:
            print(f"{path}: unchanged")
            continue
        with open(path, "w") as fh:
            fh.write(widened)
        print(f"{path}: widened")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(f"usage: {sys.argv[0]} <preset.txt> [<preset.txt> ...]")
    main(sys.argv[1:])
