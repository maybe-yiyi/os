#!/bin/sh
set -e

if ! command -v clang-format >/dev/null 2>&1; then
	echo "clang-format not found; install it (e.g. apt install clang-format)" >&2
	exit 1
fi

ROOT="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
cd "$ROOT"

find kernel libc \( -name '*.c' -o -name '*.h' \) -print0 |
	xargs -0 clang-format -i
