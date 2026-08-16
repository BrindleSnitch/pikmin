# Porting notes

Working notes for a native Android port, kept on the `port` branch. The `main`
branch stays as upstream `projectPiki/pikmin` left it.

Nothing here changes the matching PowerPC build. The upstream CodeWarrior
toolchain, the `.dol` it produces, and its CI are all untouched.

## Where things stand

A syntax-only pass over the 397 portable game sources with clang 21 on aarch64,
after force-including `port/compat/ppc_compat.h`:

```
352 clean / 397 total
```

The remaining 45 files produce 140 errors in four repetitive categories.

| Category | Count | Notes |
| --- | ---: | --- |
| Address of a temporary | 116 | CodeWarrior permitted `&Foo(x)`. Hoist into a named local. Almost all in `plugPikiNakata` (enemy AI). |
| `windows.h` / `gl/gl.h` missing | 12 | Nintendo's internal PC tooling — see below. Stub or exclude; do not port. |
| Pointer cast to `int` | 11 | The 32→64-bit problem. `int` no longer holds an address. Use `intptr_t`. Expect far more of these at runtime than a syntax check reveals. |
| Misc | 2 | One missing return, one duplicated default argument. |

`tools/port/baseline.txt` lists those 45 files. `tools/port/check.sh` fails only
when a file *outside* that list breaks, so CI stays green and useful while the
backlog is worked through.

## Two findings worth knowing

**The renderer already has a seam.** `Graphics` (include/Graphics.h) is an
abstract base class, and the tree ships two implementations: the GameCube
backend under `src/sysDolphin`, and `src/sysCore/oglGraphics.cpp` — 1,287 lines
of OpenGL, left over from an internal Windows build of the game. That code is
2001-era immediate-mode desktop GL and cannot be used as-is on GLES3, but it
proves the abstraction survives a second backend and shows which calls each
method has to satisfy.

Direct `GX*` calls split as:

| Location | Calls | Fate |
| --- | ---: | --- |
| `src/sysDolphin` | 510 | Inside the backend. Replaced wholesale. |
| `plugPikiYamashita` | 259 | The P2D 2D pane library — menus and HUD. |
| `plugPikiColin` / `Ogawa` / `Kando` | 124 | Scattered, mostly UI-adjacent. |
| `plugPikiNakata` / `Nishimura` / `sysCommon` | 0 | Gameplay and AI touch no graphics API at all. |

So the leakage past the abstraction is concentrated in 2D UI rather than spread
through gameplay.

**`src/sysCore` is dev tooling, not game code.** The files needing `windows.h`
are `wSocket`, `tcpStream`, `moduleMgr`, `attachModule`, `atxDirectRouter`, and
the `ui*` set — a debug console, a socket layer, and an in-house window toolkit
that Nintendo's developers ran on PC. None of it is needed to play the game.

## Assets

The repository contains no game assets and never will; they come from a disc
image the user supplies.

Retail discs are 1,459,978,240 bytes. An NKit-shrunk image is smaller but keeps
the standard disc header (magic `c2339f3d` at `0x1c`) and, for the GPIE01 image
tested, leaves every file at its native offset with the FST intact at the offset
named in the header — 3,495 files, 214 directories, 630.7 MB. So the filesystem
can be walked directly out of an NKit image without restoring it first, which
avoids needing NKit's Windows tooling.

Every multi-byte value on the disc is big-endian, including inside the asset
formats. Byte-swapping on load is the bulk of the asset work.

`tools/port/extract_disc.py` walks the FST and writes the tree out. Run it with
`--check` first; it refuses to extract from an image whose file data is
incomplete.

### What GPIE01 contains

3,495 files, 631 MB. By volume it is mostly video:

| Format | Files | Size | What it is |
| --- | ---: | ---: | --- |
| `.h4m` | 6 | 471.5 MB | HVQM4 full-motion video. Decoder already decompiled in `src/hvqm4dec`. |
| `.mod` | 194 | 50.2 MB | Models. |
| `.aw` | 33 | 21.5 MB | JAudio wave archives. |
| `.stx` | 13 | 17.6 MB | Streamed audio. |
| `.anm` | 186 | 9.0 MB | Animations. |
| `.bti` | 1002 | 8.6 MB | Textures (standard Nintendo BTI). |
| `.blo` | 868 | — | 2D layouts, consumed by the P2D pane library. |
| `.pcr` / `.gen` / `.dsk` / `.cin` | 810 | — | Routes, generators, disk/level config, cutscene scripts. |

Cutscenes are three quarters of the disc and none of the first playable
milestone depends on them, so they can be deferred wholesale.

The 868 `.blo` layouts line up with the 259 GX calls in `plugPikiYamashita`:
the 2D UI is a large, self-contained slice of both the code and the data.

### The disc ships Nintendo's PC build

The root of the retail disc contains `sysBootup.exe`, `sysCore.dll`, and several
`.ilk` files — Windows PE binaries and Microsoft incremental-linker artifacts,
left on the shipping disc. `sysCore.dll` is the Windows build of the same
`src/sysCore` that needs `windows.h`, and by extension of `oglGraphics.cpp`.

We can't use those binaries, but they confirm the PC/OpenGL configuration was a
real, complete build rather than abandoned scaffolding — which is the strongest
evidence yet that the `Graphics` abstraction genuinely supports a second
backend.

## Layout

```
port/compat/     shims that let original sources build on a modern toolchain
tools/port/      the compile check and its baseline
```
