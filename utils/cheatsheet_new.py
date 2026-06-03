#!/usr/bin/env python3
"""Generate a cheatsheet include.tex containing ONLY the ops added on this
branch, grouped by FEATURE rather than by the toml section the op happens to
live in. Rendering mirrors cheatsheet.py so the output is identical in style to
the normal docs build.

The ops are defined by FEATURE_GROUPS below: an ordered list of
(group title, [op names in display order]). The union of all names is the
allow-list. The script warns on stderr if any listed name has no toml entry,
so drift is flagged loudly.

Runs strictly serially (no multiprocessing pool) by design.
"""

import sys
from pathlib import Path

import pypandoc
import pytoml as toml

from common import validate_toml

THIS_FILE = Path(__file__).resolve()
ROOT_DIR = THIS_FILE.parent.parent
OP_DOCS_DIR = ROOT_DIR / "docs" / "ops"

# toml section files the new ops live in (searched for each op name).
SECTION_FILES = ["patterns", "custom_pattern", "maths", "seed", "chord"]

# Ops added on the `eimhin` branch (vs main), grouped by feature. Order within
# each group is the display order on the sheet.
FEATURE_GROUPS = [
    ("Seeded random", [
        "SRND", "SRND.R"]),
    ("Marbles", [
        "MRBL.V", "MRBL.D", "MRBL.T", "MRBL.SD"]),
    ("Cellular-automaton rhythm", [
        "CA", "CA.X", "CA.SEED"]),
    ("Interference / phasing rhythm", [
        "POLY", "POLY.X", "POLY.A"]),
    ("Breathing Euclidean rhythm", [
        "ERD", "ERD.W"]),
    ("Bytebeat rhythm", [
        "BB"]),
    ("Pattern accumulators", [
        "P.A", "PN.A", "P.A.W", "PN.A.W", "ACC.CLR"]),
    ("Diatonic generators", [
        "P.MOTIF", "PN.MOTIF", "P.CP", "PN.CP",
        "P.FUGUE", "PN.FUGUE", "P.ORN", "PN.ORN"]),
    ("Pattern playback engine", [
        "P.MODE", "PN.MODE", "P.DIR", "PN.DIR", "P.STRIDE", "PN.STRIDE"]),
    ("Stages", [
        "P.STEP", "PN.STEP", "P.STEP.NEW", "PN.STEP.NEW",
        "P.STEP?", "PN.STEP?",
        "P.D", "PN.D", "P.D.HERE", "PN.D.HERE", "P.D.RND", "PN.D.RND"]),
    ("Custom pattern", [
        "XP", "XP.HERE", "XP.I", "XP.L", "XP.WRAP", "XP.START", "XP.END",
        "XP.NEXT", "XP.NEXT.ALL"]),
    ("Chord / Key", [
        "KEY", "KEY.C", "KEY.V"]),
]


def latex_safe(s):
    # backslash must be first, otherwise it will duplicate itself
    unsafe = ["\\", "&", "%", "$", "#", "_", "{", "}", "^"]
    for u in unsafe:
        s = s.replace(u, "\\" + u)
    s = s.replace("~", "\\~{}")
    return s


def load_ops():
    """name -> op dict, merged across the section files (first wins)."""
    ops = {}
    for section in SECTION_FILES:
        toml_file = Path(OP_DOCS_DIR, section + ".toml")
        if not (toml_file.exists() and toml_file.is_file()):
            continue
        section_ops = toml.loads(toml_file.read_text())
        validate_toml(section_ops)
        for name, op in section_ops.items():
            ops.setdefault(name, op)
    return ops


def render_op(op):
    short_latex = pypandoc.convert_text(op["short"], format="markdown", to="tex")
    prototype = latex_safe(op["prototype"])
    if "prototype_set" in op:
        prototype += " / " + latex_safe(op["prototype_set"])
    out = "\\begin{op}"
    if op.get("aliases"):
        out += "[" + latex_safe(" ".join(op["aliases"])) + "]"
    out += "{" + prototype + "}\n"
    out += short_latex
    out += "\\end{op}\n\n"
    return out


def cheatsheet_tex():
    ops = load_ops()
    output = "Teletype — new ops cheatsheet\n\n"
    missing = []
    for title, names in FEATURE_GROUPS:
        output += f"\\group{{{ latex_safe(title) }}}\n\n"
        for name in names:  # strictly serial, in declared order
            op = ops.get(name)
            if op is None:
                missing.append(name)
                continue
            output += render_op(op)
    if missing:
        sys.stderr.write(
            "WARNING: no toml entry found for: " + ", ".join(missing) + "\n")
    return output


def main():
    if len(sys.argv) != 2:
        sys.exit("Please supply a filename")
    Path(sys.argv[1]).resolve().write_text(cheatsheet_tex())


if __name__ == "__main__":
    main()
