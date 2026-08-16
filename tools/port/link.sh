#!/usr/bin/env bash
#
# Build the platform layer and link the executable.
#
# Everything must come from one toolchain. This device carries both a Bionic
# clang and a glibc gcc, and mixing them produces link failures that read as
# missing library functions (__errno against __errno_location) rather than as
# the ABI mismatch they actually are. See PORTING.md.
#
# Run tools/port/build.sh first to produce the game objects.

set -uo pipefail
cd "$(dirname "$0")/../.."

CC="${CC:-gcc}"
CXX="${CXX:-g++}"
BUILD_DIR="${BUILD_DIR:-/root/pikmin-build}"
PLATFORM_DIR="$BUILD_DIR/platform"
OUT="${OUT:-$BUILD_DIR/pikmin}"

SDL_CFLAGS="$(pkg-config --cflags sdl2 2>/dev/null || true)"
SDL_LIBS="$(pkg-config --libs sdl2 2>/dev/null || echo -lSDL2)"

# The platform layer needs the real system headers, so it must NOT see
# include/stl -- those shims shadow <stdlib.h>, <stdio.h> and friends, and the
# libc declarations vanish. The game code needs the opposite.

# Which disc version the sources are compiled for. 116 files change behaviour on
# these macros, and with none defined the version-conditional branches simply
# vanish -- GameFlow's language paths, for one, are left null, which surfaces
# much later as a crash in unrelated code. types.h derives the broader
# VERSION_GPIE01 from this. Mirrors configure.py's -DVERSION_{version}.
VERSION_DEFS="-DVERSION_GPIE01_01 -DBUILD_VERSION=5"

CFLAGS_PLATFORM="-std=gnu99 -O0 -D_GNU_SOURCE -I include -w"
CXXFLAGS_PLATFORM="-std=c++98 -O0 -D_GNU_SOURCE $VERSION_DEFS -I include -I include/stl -include port/compat/ppc_compat.h -w"

mkdir -p "$PLATFORM_DIR"
rm -f "$PLATFORM_DIR"/*.o

fail=0
for f in port/platform/*.c; do
	extra=""
	case "$f" in *pad_host.c) extra="$SDL_CFLAGS" ;; esac
	# gx_stub and gx_fifo include SDK headers that pull in the stl shims
	case "$f" in
	*gx_stub.c | *gx_fifo.c | *jaudio_stub.c)
		flags="-std=gnu99 -O0 $VERSION_DEFS -I include -I include/stl -I port/platform -include port/compat/ppc_compat.h -w"
		;;
	*) flags="$CFLAGS_PLATFORM" ;;
	esac
	if ! $CC -c $flags $extra "$f" -o "$PLATFORM_DIR/$(basename "$f" .c).o" 2>&1; then
		echo "failed: $f"
		fail=1
	fi
done

# src/mtx is decompiled SDK source rather than part of the port, but it is pure
# computation with no hardware behind it, so it is used as-is instead of being
# reimplemented. build.sh only walks C++ sources, so it is compiled here.
for f in src/mtx/*.c; do
	# Must see exactly the same VERSION_DEFS as the game code. Dolphin/mtx.h
	# aliases matrix functions in whichever direction the configured SDK
	# revision needs -- in this one it renames C_MTXTrans to MTXTrans rather
	# than the reverse -- so a translation unit compiled with different macro
	# state defines its functions under different names, and the link fails on
	# symbols that plainly exist in the source.
	if ! $CC -c -std=gnu99 -O0 $VERSION_DEFS -I include -I include/stl \
		-include port/compat/ppc_compat.h -w "$f" \
		-o "$PLATFORM_DIR/mtx__$(basename "$f" .c).o" 2>&1; then
		echo "failed: $f"
		fail=1
	fi
done

for f in port/platform/*.cpp; do
	[ -e "$f" ] || continue
	if ! $CXX -c $CXXFLAGS_PLATFORM "$f" -o "$PLATFORM_DIR/$(basename "$f" .cpp).o" 2>&1; then
		echo "failed: $f"
		fail=1
	fi
done

# The entry point lives outside the directories build.sh walks.
$CXX -c $CXXFLAGS_PLATFORM src/sysBootup.cpp -o "$BUILD_DIR/sysBootup.o" 2>&1 || fail=1

[ "$fail" -eq 0 ] || { echo "platform layer did not build"; exit 1; }

echo "platform objects: $(ls "$PLATFORM_DIR"/*.o | wc -l)"

# -no-pie puts the executable at a low fixed load address instead of letting the
# loader place it above 4GB. The game round-trips pointers through u32 in a
# number of places -- System::copyRamToCache hands doneDMA a (u32)cache, ARAM
# addresses, cache handles -- which was lossless on a 32-bit console and
# truncating here. Mapping the arena low fixed heap pointers; this fixes the
# same problem for everything with static storage, which the arena trick cannot
# reach.
if $CXX -no-pie -o "$OUT" "$BUILD_DIR"/*.o "$PLATFORM_DIR"/*.o $SDL_LIBS -lpthread -lm 2>/tmp/link.err; then
	echo "linked: $OUT"
	ls -la "$OUT"
else
	echo "link failed; distinct undefined symbols:"
	grep -oE "undefined reference to \`[^']+'" /tmp/link.err |
		sed "s/undefined reference to \`//; s/'$//" | sort -u | head -40
	exit 1
fi
