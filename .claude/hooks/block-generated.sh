#!/usr/bin/env bash
# PreToolUse hook: refuse edits to ragel/python-generated files.
# Exit 2 blocks the tool call and feeds stderr back to Claude.

set -eu

# Hook silently no-ops if jq isn't installed (don't block legitimate edits
# because of a missing dependency).
command -v jq >/dev/null 2>&1 || exit 0

f=$(jq -r '.tool_input.file_path // empty')

# Patterns anchor on a path separator (or start-of-string) before `src/`
# so we don't false-positive on paths like `othersrc/match_token.c`.
case "$f" in
    src/match_token.c|*/src/match_token.c|src/scanner.c|*/src/scanner.c|src/ops/op_enum.h|*/src/ops/op_enum.h)
        cat >&2 <<EOF
Refusing to edit generated file: $f

Edit the source instead:
  src/match_token.c  <- src/match_token.rl (ragel regenerates on next make)
  src/scanner.c      <- src/scanner.rl     (ragel regenerates on next make)
  src/ops/op_enum.h  <- python3 utils/op_enums.py
EOF
        exit 2
        ;;
esac
