#!/usr/bin/env bash
#
# Compile the portable source set to object files.
#
# Unlike check.sh this generates real code, which is the point: the undefined
# symbols left over across the resulting objects are an exact, enumerated
# specification of the platform layer still to be written. Nothing is linked --
# there is no entry point yet and the GameCube SDK is not implemented.
#
# Objects land outside the repository by default; override with BUILD_DIR.

set -uo pipefail
cd "$(dirname "$0")/../.."

# Default to the host's native toolchain. On a device carrying both a Bionic
# and a glibc compiler these are not interchangeable -- objects built against
# one cannot link against libraries built for the other -- so the whole tree
# must use a single one. See PORTING.md.
CXX="${CXX:-g++}"
BUILD_DIR="${BUILD_DIR:-/root/pikmin-build}"
EXCLUDED="tools/port/excluded.txt"
# Which disc version the sources are compiled for. 116 files change behaviour on
# these macros, and with none defined the version-conditional branches simply
# vanish -- GameFlow's language paths, for one, are left null, which surfaces
# much later as a crash in unrelated code. types.h derives the broader
# VERSION_GPIE01 from this. Mirrors configure.py's -DVERSION_{version}.
VERSION_DEFS="-DVERSION_GPIE01_01 -DBUILD_VERSION=5"

FLAGS=(-c -std=c++98 -O0 -fno-strict-aliasing $VERSION_DEFS -I include -I include/stl
       -include port/compat/ppc_compat.h -w)

skiplist="$(mktemp)"
trap 'rm -f "$skiplist"' EXIT
sed -e 's/#.*//' -e '/^[[:space:]]*$/d' -e 's/[[:space:]]*$//' \
	"$EXCLUDED" | sort >"$skiplist"

mkdir -p "$BUILD_DIR"
ok=0
fail=0
failed_list=""

while IFS= read -r f; do
	grep -qxF "$f" "$skiplist" && continue
	# flatten src/a/b.cpp -> a__b.o so basenames can't collide
	obj="$BUILD_DIR/$(echo "${f#src/}" | sed 's|/|__|g; s|\.cpp$|.o|')"
	if "$CXX" "${FLAGS[@]}" "$f" -o "$obj" 2>/dev/null; then
		ok=$((ok + 1))
	else
		fail=$((fail + 1))
		failed_list="$failed_list $f"
	fi
done < <(find src/plugPiki* src/sysCommon src/sysCore src/sysDolphin \
	-name '*.cpp' | sort)

echo "compiled $ok objects, $fail failed"
if [ "$fail" -ne 0 ]; then
	echo "failed:"
	printf '  %s\n' $failed_list
	exit 1
fi
echo "objects in $BUILD_DIR"
