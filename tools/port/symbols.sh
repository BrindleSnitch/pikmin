#!/usr/bin/env bash
#
# Report what the compiled objects still need from outside themselves.
#
# Every symbol referenced but not defined anywhere in the object set is
# something the platform layer must eventually supply -- a GameCube SDK entry
# point, or a C/C++ runtime function. Grouped by SDK prefix, this is the
# platform layer's to-do list, measured rather than guessed at.
#
# Run tools/port/build.sh first.

set -uo pipefail
cd "$(dirname "$0")/../.."

BUILD_DIR="${BUILD_DIR:-/root/pikmin-build}"
NM="${NM:-llvm-nm}"
command -v "$NM" >/dev/null || NM=nm

objs=("$BUILD_DIR"/*.o)
if [ ! -e "${objs[0]}" ]; then
	echo "no objects in $BUILD_DIR -- run tools/port/build.sh first" >&2
	exit 1
fi

defined="$(mktemp)"
undef="$(mktemp)"
trap 'rm -f "$defined" "$undef"' EXIT

"$NM" --defined-only "${objs[@]}" 2>/dev/null | awk '{print $NF}' | sort -u >"$defined"
"$NM" --undefined-only "${objs[@]}" 2>/dev/null | awk '{print $NF}' | sort -u >"$undef"

missing="$(comm -23 "$undef" "$defined")"
n_missing=$(printf '%s\n' "$missing" | grep -c . || true)

echo "objects        ${#objs[@]}"
echo "defined        $(wc -l <"$defined")"
echo "undefined      $(wc -l <"$undef")"
echo "still missing  $n_missing"
echo

# Group by the SDK's naming convention: a run of capitals starting the symbol.
echo "missing symbols by subsystem:"
printf '%s\n' "$missing" | sed 's/^_*//' \
	| awk '
	/^(GX|OS|DVD|PAD|VI|SI|EXI|CARD|AR|AI|DSP|THP|HVQ)[A-Z_a-z0-9]*/ {
		match($0, /^(GX|OS|DVD|VI|SI|EXI|CARD|AR|AI|DSP|THP|HVQ|PAD)/)
		print substr($0, 1, RLENGTH); next
	}
	/^PS[A-Z]/  { print "PS (mtx)"; next }
	/^J[A-Z]/   { print "J (jaudio/jsystem)"; next }
	/^Z2/       { print "Z2 (audio)"; next }
	/^__/       { print "compiler/runtime"; next }
	           { print "other" }
	' | sort | uniq -c | sort -rn

echo
echo "sample of 'other' (application-level or C library):"
printf '%s\n' "$missing" | sed 's/^_*//' \
	| grep -vE '^(GX|OS|DVD|PAD|VI|SI|EXI|CARD|AR|AI|DSP|THP|HVQ|PS[A-Z]|J[A-Z]|Z2|__)' \
	| head -25
