#!/usr/bin/env python3
"""
Extract the filesystem from a GameCube disc image.

Walks the FST (file system table) described in the disc header and writes every
file out to a directory tree, plus the main executable as boot.dol.

Works directly on NKit-shrunk images as well as plain ISO/GCM. NKit removes junk
padding but keeps the real disc header at offset 0 and leaves file data at its
original offsets, so nothing needs restoring first -- the only difference is
that the image is shorter than the 1,459,978,240 bytes of a retail disc. The
--check pass verifies that assumption before writing anything.

All disc structures are big-endian.

Usage:
    python3 extract_disc.py <image> <output-dir> [--check]
"""

import argparse
import os
import struct
import sys

GC_MAGIC = 0xC2339F3D
RETAIL_SIZE = 1_459_978_240

# Disc header field offsets
OFF_GAME_ID = 0x000
OFF_TITLE = 0x020
OFF_MAGIC = 0x01C
OFF_DOL = 0x420
OFF_FST = 0x424
OFF_FST_SIZE = 0x428


class Entry:
    __slots__ = ("path", "offset", "size")

    def __init__(self, path, offset, size):
        self.path = path
        self.offset = offset
        self.size = size


def read_header(fh):
    fh.seek(OFF_MAGIC)
    (magic,) = struct.unpack(">I", fh.read(4))
    if magic != GC_MAGIC:
        sys.exit(f"not a GameCube image: magic {magic:#010x}, expected {GC_MAGIC:#010x}")

    fh.seek(OFF_GAME_ID)
    game_id = fh.read(6).decode("ascii", "replace")
    fh.seek(OFF_TITLE)
    title = fh.read(0x60).split(b"\0")[0].decode("ascii", "replace").strip()
    fh.seek(OFF_DOL)
    dol_off, fst_off, fst_size = struct.unpack(">III", fh.read(12))
    return game_id, title, dol_off, fst_off, fst_size


def parse_fst(fh, fst_off, fst_size):
    """Return a flat list of Entry, with full paths resolved."""
    fh.seek(fst_off)
    fst = fh.read(fst_size)
    if len(fst) < 12:
        sys.exit("FST truncated -- image is damaged or offsets are wrong")

    n_entries = struct.unpack(">I", fst[8:12])[0]
    if fst[0] != 1 or not (0 < n_entries < 100_000):
        sys.exit("FST root entry invalid -- this image may need restoring first")

    strtab = fst[n_entries * 12:]

    def name_at(off):
        end = strtab.find(b"\0", off)
        return strtab[off:end].decode("ascii", "replace")

    entries = []
    # dir_end[depth] tracks the entry index a directory's contents run until,
    # which is how the FST encodes nesting -- there are no explicit closers.
    stack = [("", n_entries)]
    for i in range(1, n_entries):
        raw = fst[i * 12:(i + 1) * 12]
        is_dir = raw[0] == 1
        name = name_at(int.from_bytes(raw[1:4], "big"))
        a, b = struct.unpack(">II", raw[4:12])

        while len(stack) > 1 and i >= stack[-1][1]:
            stack.pop()

        parent = stack[-1][0]
        path = f"{parent}/{name}" if parent else name

        if is_dir:
            stack.append((path, b))
        else:
            entries.append(Entry(path, a, b))

    return entries


def dol_size(fh, dol_off):
    """DOL has no length field; derive it from its 18 section tables."""
    fh.seek(dol_off)
    hdr = fh.read(0x100)
    offsets = struct.unpack(">18I", hdr[0x00:0x48])
    sizes = struct.unpack(">18I", hdr[0x90:0xD8])
    return max((o + s) for o, s in zip(offsets, sizes) if o)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("output", nargs="?")
    ap.add_argument("--check", action="store_true",
                    help="validate the image and report, without extracting")
    args = ap.parse_args()

    img_size = os.path.getsize(args.image)
    fh = open(args.image, "rb")

    game_id, title, dol_off, fst_off, fst_size = read_header(fh)
    shrunk = img_size < RETAIL_SIZE

    print(f"image    {os.path.basename(args.image)}")
    print(f"game id  {game_id}  ({title})")
    print(f"size     {img_size:,} bytes"
          f"{'  [shrunk -- padding removed]' if shrunk else ''}")

    if fst_off + fst_size > img_size:
        sys.exit("FST lies beyond end of file -- this image must be restored first")

    entries = parse_fst(fh, fst_off, fst_size)
    total = sum(e.size for e in entries)
    beyond = [e for e in entries if e.offset + e.size > img_size]

    print(f"files    {len(entries):,}  ({total / 1048576:.1f} MB)")

    if beyond:
        print(f"\n{len(beyond)} file(s) lie beyond end of image -- cannot extract:")
        for e in beyond[:5]:
            print(f"  {e.path}")
        sys.exit("image is incomplete; restore it to a full ISO first")

    print("all file data present within image")

    if args.check:
        return
    if not args.output:
        sys.exit("output directory required (or pass --check)")

    written = 0
    for n, e in enumerate(entries, 1):
        dest = os.path.join(args.output, "files", e.path)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        fh.seek(e.offset)
        remaining = e.size
        with open(dest, "wb") as out:
            while remaining:
                chunk = fh.read(min(1 << 20, remaining))
                if not chunk:
                    sys.exit(f"unexpected EOF reading {e.path}")
                out.write(chunk)
                remaining -= len(chunk)
        written += e.size
        if n % 250 == 0 or n == len(entries):
            print(f"  {n:,}/{len(entries):,} files, {written / 1048576:.0f} MB",
                  flush=True)

    ds = dol_size(fh, dol_off)
    fh.seek(dol_off)
    with open(os.path.join(args.output, "boot.dol"), "wb") as out:
        out.write(fh.read(ds))
    print(f"boot.dol {ds:,} bytes")
    print(f"\nextracted to {args.output}")


if __name__ == "__main__":
    main()
