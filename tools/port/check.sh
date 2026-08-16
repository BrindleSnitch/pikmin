#!/usr/bin/env bash
#
# Syntax-check the portable game sources with a modern compiler.
#
# This does not link or run anything -- it answers one question: does the
# GameCube source still parse as valid C++ for a little-endian 64-bit target?
# That is the tripwire that catches an edit breaking one of the other 396 files.
#
# 45 files are known to fail (see baseline.txt). The check passes as long as no
# NEW file joins them, so the build stays green while those are worked through.

set -uo pipefail
cd "$(dirname "$0")/../.."

CXX="${CXX:-clang++}"
BASELINE="tools/port/baseline.txt"
FLAGS=(-fsyntax-only -std=c++98 -I include -I include/stl
       -include port/compat/ppc_compat.h -w)

failing="$(mktemp)"
trap 'rm -f "$failing"' EXIT

total=0
while IFS= read -r f; do
	total=$((total + 1))
	if ! "$CXX" "${FLAGS[@]}" "$f" 2>/dev/null; then
		printf '%s\n' "$f" >>"$failing"
	fi
done < <(find src/plugPiki* src/sysCommon src/sysCore src/sysDolphin \
	-name '*.cpp' | sort)

sort -o "$failing" "$failing"
n_fail=$(wc -l <"$failing")
echo "checked $total files -- $((total - n_fail)) clean, $n_fail failing"

regressed="$(comm -23 "$failing" "$BASELINE")"
fixed="$(comm -13 "$failing" "$BASELINE")"

if [ -n "$fixed" ]; then
	echo
	echo "these files now compile and should be removed from $BASELINE:"
	printf '  %s\n' $fixed
fi

if [ -n "$regressed" ]; then
	echo
	echo "REGRESSION -- these files compiled before and no longer do:"
	printf '  %s\n' $regressed
	echo
	echo "first error from each:"
	for f in $regressed; do
		echo "--- $f"
		"$CXX" "${FLAGS[@]}" "$f" 2>&1 | grep 'error:' | head -3
	done
	exit 1
fi

echo "no regressions"
