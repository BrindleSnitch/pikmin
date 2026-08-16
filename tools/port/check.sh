#!/usr/bin/env bash
#
# Syntax-check the portable game sources with a modern compiler.
#
# This does not link or run anything -- it answers one question: does the
# GameCube source still parse as valid C++ for a little-endian 64-bit target?
# That is the tripwire that catches an edit breaking one of the other 396 files.
#
# baseline.txt lists files known to fail; the check passes as long as no NEW
# file joins them. It is currently empty -- every source in the portable set
# compiles -- so any failure at all is a regression. excluded.txt lists sources
# deliberately left out of the set, with reasons.

set -uo pipefail
cd "$(dirname "$0")/../.."

CXX="${CXX:-clang++}"
BASELINE="tools/port/baseline.txt"
EXCLUDED="tools/port/excluded.txt"
# Which disc version the sources are compiled for. 116 files change behaviour on
# these macros, and with none defined the version-conditional branches simply
# vanish -- GameFlow's language paths, for one, are left null, which surfaces
# much later as a crash in unrelated code. types.h derives the broader
# VERSION_GPIE01 from this. Mirrors configure.py's -DVERSION_{version}.
VERSION_DEFS="-DVERSION_GPIE01_01 -DBUILD_VERSION=5"

FLAGS=(-fsyntax-only -std=c++98 $VERSION_DEFS -I include -I include/stl
       -include port/compat/ppc_compat.h -w)

failing="$(mktemp)"
skiplist="$(mktemp)"
trap 'rm -f "$failing" "$skiplist"' EXIT

# strip comments and blanks from the exclusion list
sed -e 's/#.*//' -e '/^[[:space:]]*$/d' -e 's/[[:space:]]*$//' \
	"$EXCLUDED" | sort >"$skiplist"

total=0
skipped=0
while IFS= read -r f; do
	if grep -qxF "$f" "$skiplist"; then
		skipped=$((skipped + 1))
		continue
	fi
	total=$((total + 1))
	if ! "$CXX" "${FLAGS[@]}" "$f" 2>/dev/null; then
		printf '%s\n' "$f" >>"$failing"
	fi
done < <(find src/plugPiki* src/sysCommon src/sysCore src/sysDolphin \
	-name '*.cpp' | sort)

sort -o "$failing" "$failing"
n_fail=$(wc -l <"$failing")
echo "checked $total files -- $((total - n_fail)) clean, $n_fail failing"
echo "excluded $skipped files (see $EXCLUDED)"

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
