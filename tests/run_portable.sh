#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$(mktemp -d "${TMPDIR:-/tmp}/undergod-studio-portable.XXXXXX")"
trap 'rm -rf "$out"' EXIT
flags=(-O2 -std=c++17 -Wall -Wextra -Wpedantic -I"$root/include")
g++ "${flags[@]}" "$root/tests/portable_checks.cpp" "$root/src/animator.cpp" "$root/src/cinematic_data.cpp" -o "$out/portable_checks"
cd "$root"
"$out/portable_checks"
