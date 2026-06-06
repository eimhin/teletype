#!/usr/bin/env python3
"""Generate a manual containing ONLY the ops added on this branch, with their
full descriptions -- the long-form companion to cheatsheet_new.py.

It shares cheatsheet_new.py's FEATURE_GROUPS allow-list (single source of truth,
so the cheatsheet and this manual never drift) and renders with the same Jinja
templates docs.py uses, so the output matches the main manual's house style.
Where cheatsheet_new.py emits only each op's one-line `short`, this emits the
full `description` (argument options, tables, examples).

Each command-line argument is an output file; the format is chosen from its
extension: .pdf / .tex / .html / .md. Run it from the utils directory, e.g.

    python3 docs_new.py ../docs/teletype_new.pdf ../docs/teletype_new.html
"""

import sys
from pathlib import Path

import jinja2
import pypandoc

from common import get_tt_version
from cheatsheet_new import FEATURE_GROUPS, load_ops

if (sys.version_info.major, sys.version_info.minor) < (3, 6):
    raise Exception("need Python 3.6 or later")

THIS_FILE = Path(__file__).resolve()
ROOT_DIR = THIS_FILE.parent.parent
TEMPLATE_DIR = ROOT_DIR / "utils" / "templates"
FONTS_DIR = ROOT_DIR / "utils" / "fonts"
TT_VERSION = get_tt_version()
VERSION_STR = " ".join(["Teletype", TT_VERSION["tag"], "New Ops Documentation"])

env = jinja2.Environment(
    autoescape=False,
    loader=jinja2.FileSystemLoader(str(TEMPLATE_DIR)),
    trim_blocks=True,
    lstrip_blocks=True,
    cache_size=0,
    auto_reload=True
)

INTRO = (
    "These are the operators added on this branch, grouped by feature, with "
    "full descriptions and argument detail. For a one-line-per-op summary see "
    "the new-ops cheatsheet; the complete Teletype manual documents every "
    "operator.\n\n"
)


def build_sections(html):
    """Render every FEATURE_GROUP as markdown. PDF uses a compact table (short
    line) followed by the extended descriptions; HTML uses the op-list template,
    which already embeds the full description inline -- mirroring docs.py."""
    table_template = env.get_template(
        "html_op_list.jinja2.md" if html else "pdf_op_table.jinja2.md")
    extended_template = env.get_template("pdf_op_extended.jinja2.md")
    ops = load_ops()

    out = ""
    missing = []
    for title, names in FEATURE_GROUPS:
        group_ops = []
        extended = []
        for name in names:
            op = ops.get(name)
            if op is None:
                missing.append(name)
                continue
            group_ops.append(op)
            if "description" in op:
                extended.append(extended_template.render(name=name, **op))
        if not group_ops:
            continue
        out += "\\newpage\n\n"
        out += f"# {title}\n\n"
        out += table_template.render(ops=group_ops) + "\n\n"
        if not html and extended:
            out += "\n".join(extended) + "\n\n"

    if missing:
        sys.stderr.write(
            "WARNING: no toml entry found for: " + ", ".join(missing) + "\n")
    return out


def main():
    if len(sys.argv) <= 1:
        sys.exit("Please supply a filename")

    pdf_md = INTRO + build_sections(html=False)
    html_md = INTRO + build_sections(html=True)

    for arg in sys.argv[1:]:
        p = Path(arg).resolve()
        print(f"Generating: {p}")
        ext = p.suffix

        if ext == ".md":
            p.write_text(pdf_md)
        elif ext == ".html":
            html_output = "# " + VERSION_STR + "\n\n" + html_md
            pypandoc.convert_text(
                html_output,
                format="markdown",
                to="html5",
                outputfile=str(p),
                extra_args=["--standalone",
                            "--self-contained",
                            "--toc",
                            "--toc-depth=2",
                            "--css=" + str(TEMPLATE_DIR / "docs.css"),
                            "--template=" + str(TEMPLATE_DIR /
                                                "template.html")])
        elif ext == ".pdf" or ext == ".tex":
            latex_preamble = env.get_template("latex_preamble.jinja2.md")
            latex = latex_preamble \
                .render(title=VERSION_STR, fonts_dir=FONTS_DIR) + "\n\n"
            latex += pdf_md
            pandoc_version = int(pypandoc.get_pandoc_version()[0])
            engine = ("--pdf-engine=xelatex"
                      if pandoc_version >= 2
                      else "--latex-engine=xelatex")
            pypandoc.convert_text(
                latex,
                format="markdown",
                to=ext[1:],
                outputfile=str(p),
                extra_args=["--standalone",
                            "--column=80",
                            "--toc",
                            "--toc-depth=2",
                            engine,
                            "--variable=papersize:A4"])


if __name__ == "__main__":
    main()
