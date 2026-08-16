# Porting notes

Working notes for a native Android port, kept on the `port` branch. The `main`
branch stays as upstream `projectPiki/pikmin` left it.

Nothing here changes the matching PowerPC build. The upstream CodeWarrior
toolchain, the `.dol` it produces, and its CI are all untouched.

## Where things stand

The whole portable set compiles as C++ for a little-endian 64-bit target:

```
386 clean / 386 total, 11 excluded
```

`tools/port/check.sh` runs that pass. `baseline.txt` is empty, so any failure is
now a regression rather than a known gap. `excluded.txt` holds the 11 sources
deliberately left out, with reasons.

This is a syntax check. It does not link, and nothing has been run — the value
is purely that an edit breaking any of the 386 gets caught within a push.

### What the 140 original errors turned out to be

**116 × address of a temporary.** All from one macro. `include/sysNew.h` carried
a placeholder in its non-CodeWarrior branch, commented "This must be replaced
with something legal or be removed." The comment above it already described the
semantics — placement new on stack-allocated space — so `stack_new` now expands
to exactly that via `__builtin_alloca`. The `__MWERKS__` branch is untouched.

Two behavioural differences from the original, neither currently reachable:
destructors do not run (the file notes Pikmin 1 barely uses them), and alloca is
reclaimed at function exit rather than scope exit, so a `stack_new` in a loop
accumulates. Seven call sites are genuinely inside loop bodies, all with small
fixed bounds (`numDigits`, `i < 8`, a debug piki count) allocating objects of a
few bytes — no growth concern, but worth remembering if new call sites appear.

**11 × pointer cast to `int`.** Two were real 64-bit defects:

- `objectMgr.cpp` computed pool element addresses through `(int)`, truncating
  the pointer before the arithmetic. `mObjectPool` is `u8*`, so plain pointer
  arithmetic is both correct and simpler.
- `MenuItem::mData` stored a `StageInfo*` in an `int` that `mapSelect.cpp` casts
  straight back to a pointer. `mData` and the `Menu` API now use `sptr`
  (types.h), which is 32-bit on PowerPC — layout and the matching build are
  unaffected.

The other nine are AgeServer debug telemetry and one debug `PRINT`, sending
pointers as opaque handles over a 32-bit wire protocol that nothing reads back
as an address. Truncation there is now explicit and commented rather than
incidental.

**2 × misc.** `ogMenu.cpp` has a `bool` function that returns nothing — faithful
to an original that left whatever was in r3, so the original is preserved under
`#if defined(__MWERKS__)` and other compilers get a value. `node.cpp` repeated a
default argument on a constructor definition; defaults resolve at the call site,
so removing it does not affect codegen.

**11 × missing `windows.h` / `gl/gl.h`.** Excluded, not shimmed — see below.

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

## The platform layer

`tools/port/build.sh` compiles the portable set to aarch64 objects — 386 of 386,
no failures. `tools/port/symbols.sh` then reports every symbol those objects
reference but do not define, which is the platform layer's surface measured
rather than guessed.

```
objects        386
defined      16283
undefined     3522
still missing  263
```

| Subsystem | Missing | Notes |
| --- | ---: | --- |
| `GX*` | 78 | Only 43 are referenced by game code; the other 35 exist solely because `src/sysDolphin`, the backend being replaced, is in the compile set. |
| `Jac_*` | 44 | The game's JAudio interface. |
| `OS*` | 35 | Threads, alarms, heaps, time. |
| `CARD*` | 23 | Memory card. Stubbable for a long time. |
| Matrix math | 9 | `C_MTX*` / `PSMTX*`. |
| `VI*` | 9 | Video interface. |
| `PAD*` | 7 | Controller. |
| `DVD*` | 6 | File I/O. |
| `HIO*` | 5 | Host debug I/O to a PC. Stub. |
| `AR*` | 3 | Audio RAM. |
| Cache ops | 3 | `DCFlushRange` and friends — no-ops off-GameCube. |
| C library | ~10 | Free once linked against libc. |

### What that number does and does not mean

Of the 263, **234 already have decompiled implementations in this repository** —
`src/gx`, `src/os`, `src/dvd`, `src/vi`, `src/pad`, `src/card`, `src/mtx`,
`src/jaudio` and so on, none of which were in the compile set. Only 29 are
genuinely absent, and every one is C library (`sprintf`, `strstr`, `atoi`,
`log`, `atan2f`, …) plus `__gxx_personality_v0` from libc++abi.

So the **link** problem is nearly closed and a linking binary is much closer
than expected. The **functional** problem is untouched, and the difference
matters: those SDK sources target GameCube hardware directly. Across
`src/{gx,os,vi,pad,si,exi,dvd,ar,dsp,ai}`, roughly a third of the files write to
hardware registers at fixed physical addresses, and nine use PowerPC inline
assembly. Compiled for ARM they would link and then write into memory that does
not exist.

The real gain is that the interface is now **fully specified and stable**. Each
subsystem can be swapped for a host implementation one at a time, behind
unchanged headers, with the decompiled original serving as an exact behavioural
reference for what the replacement has to do.

### Progress

`port/platform/` now supplies OS, DVD, VI, PAD, CARD, AR, HIO and the cache ops,
and `src/mtx` supplies the matrix maths. That takes the missing SDK surface from
263 to **122** — GX 78 (only 43 of them game-facing) and `Jac_*` 44. Everything
else is done. (A raw symbol count also lists ~79 libc and pthread names; those
resolve at link time and are not work.)

| File | Supplies | Real or stub |
| --- | --- | --- |
| `os_host.c` | 35 `OS*` | Real: pthreads, POSIX clocks, a 24MB arena. |
| `dvd_host.c` | 6 `DVD*` | Real: `pread` against the extracted tree. |
| `vi_host.c` | 9 `VI*` | Real: frame pacing at 60000/1001 Hz. No display yet. |
| `pad_host.c` | 7 `PAD*` | Real: SDL game controllers, incl. rumble. |
| `card.c` | 23 `CARD*` | Stub: reports no card, a state the game already handles. |
| `stubs.c` | cache, `HIO*`, `AR*` | No-op by nature, or deferred with audio. |

VI matters more than "no display yet" suggests: the game's main loop is driven
by `VIWaitForRetrace`, so that function's rate is the rate the whole simulation
advances at. NTSC is 59.94 fields per second rather than 60, and the game was
tuned against the real figure, so the pacing uses it. A missed field
resynchronises to the next boundary instead of spinning through frames already
lost.

### Toolchain

This device has two, and they are not interchangeable:

| Command | Target | libc |
| --- | --- | --- |
| `clang` (Termux) | `aarch64-unknown-linux-android24` | Bionic |
| `gcc` (Ubuntu) | `aarch64-linux-gnu` | glibc |

`apt install libsdl2-dev` produces a **glibc** SDL2, which cannot be linked
against Bionic objects. Local development therefore uses Ubuntu's gcc; both
compilers build the tree cleanly, and CI independently checks a third
(clang 18, x86_64 glibc).

Worth knowing for later: an Android APK targets Bionic, so it needs a fourth
toolchain — the NDK, on an x86_64 runner, with an SDL2 built for Android. That
is a packaging concern rather than a porting one, but it is the reason to keep
the platform layer free of glibc-specific assumptions.

**None of this has been run.** It compiles; that is the entire claim. Threads in
particular are the likeliest source of trouble: GameCube threads are
cooperatively scheduled and start suspended, and while the suspend-on-create
behaviour is reproduced, any code that relied on cooperative scheduling for
mutual exclusion will race here where it did not on hardware.

#### A trap worth remembering

`src/mtx` defines each paired-single routine as

```c
void PSMTXConcat(...) { #ifdef __MWERKS__ asm { ... } #endif }
```

so on any other compiler those bodies are **empty**. Adding `src/mtx` to a build
therefore links cleanly and silently turns every matrix operation into a no-op —
no error, no warning, just a game that renders nonsense. All 36 now delegate to
their exact `C_` twins under `#else`, verified by checking compiled symbol sizes
rather than trusting the edit. Expect the same shape elsewhere in the SDK: a
`#ifdef __MWERKS__` around an asm body is a silent hole, not a compile error.

### Suggested order

1. **Cache ops, `HIO*`, `CARD*`** — no-op or stub. Removes 31 symbols for almost
   no work and no behavioural risk.
2. **Matrix math** — pure computation. The `src/mtx` C paths port directly;
   only the paired-single assembly variants need attention.
3. **`OS*`** — heaps, time, threads onto host equivalents. Everything else
   depends on this, so it comes before the interesting work.
4. **`DVD*`** — retarget onto the extracted asset tree. Small surface, 6 symbols,
   and it is what makes real data reachable.
5. **`PAD*`** — SDL gamepad, plus touch later. Small and immediately testable.
6. **`VI*` + `GX*`** — the renderer. 43 game-facing GX entry points, with
   `src/sysCore/oglGraphics.cpp` as a reference for what a second backend has to
   satisfy.
7. **`Jac_*`** — audio, last. The game is playable without it.

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
