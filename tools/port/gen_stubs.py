#!/usr/bin/env python3
"""
Generate compilable stubs for a list of SDK symbols.

Signatures are lifted from the project's own headers rather than written out by
hand, so a stub cannot silently disagree with the declaration its callers see.
Anything whose declaration cannot be found is reported instead of guessed at.

The point of these is to reach a linking, running binary before the real
implementations exist: a stub that returns zero is obviously wrong at runtime,
whereas a missing symbol tells you nothing until every last one is written.

Usage:
    gen_stubs.py --prefix GX   --out port/platform/gx_stub.c   --symbols missing.txt
"""

import argparse
import os
import re
import sys

HEADER_DIRS = ["include"]

# `ret name(params);` with the return type possibly carrying * and qualifiers.
DECL = re.compile(
    r'(?P<ret>[A-Za-z_][A-Za-z0-9_ \t]*[ \t*]+)'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)'
    r'[ \t]*\((?P<params>[^;{)]*)\)[ \t]*;',
    re.S)


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    return re.sub(r'//[^\n]*', ' ', text)


def collect_decls():
    """symbol -> (return type, parameter list) from every header."""
    found = {}
    for root_dir in HEADER_DIRS:
        for root, _dirs, files in os.walk(root_dir):
            for fn in files:
                if not fn.endswith((".h", ".hpp")):
                    continue
                path = os.path.join(root, fn)
                try:
                    text = strip_comments(open(path, errors="replace").read())
                except OSError:
                    continue
                for m in DECL.finditer(text):
                    ret = " ".join(m.group("ret").split())
                    name = m.group("name")
                    params = " ".join(m.group("params").split())
                    # skip obvious non-declarations
                    if ret.split()[0] in ("return", "typedef", "else", "if", "while"):
                        continue
                    found.setdefault(name, (ret, params))
    return found


def normalise_ret(ret):
    """Drop storage-class and cv qualifiers so 'extern void' reads as 'void'."""
    r = ret
    for q in ("extern", "static", "inline", "const", "volatile", "__inline"):
        r = re.sub(rf'\b{q}\b', ' ', r)
    return " ".join(r.split())


def zero_for(ret):
    r = normalise_ret(ret)
    if r == "void":
        return None
    if "*" in r:
        return "NULL"
    if r in ("f32", "float", "f64", "double"):
        return "0.0f" if r in ("f32", "float") else "0.0"
    return "0"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--prefix", required=True, help="symbol prefix, e.g. GX or Jac_")
    ap.add_argument("--symbols", required=True, help="file listing missing symbols")
    ap.add_argument("--out", required=True)
    ap.add_argument("--include", action="append", default=[],
                    help="header to #include in the generated file")
    args = ap.parse_args()

    wanted = [s.strip() for s in open(args.symbols) if s.strip().startswith(args.prefix)]
    decls = collect_decls()

    bodies, missing, names = [], [], []
    for sym in sorted(set(wanted)):
        if sym not in decls:
            missing.append(sym)
            continue
        ret, params = decls[sym]
        zero = zero_for(ret)
        # name the parameters so we can void-cast them; declarations often omit names
        plist = []
        if params and params != "void":
            for i, p in enumerate(params.split(",")):
                p = p.strip()
                if not p:
                    continue
                # An array parameter ends in brackets, so look for the name
                # before them rather than at the very end.
                arrays = ""
                core = p
                m2 = re.search(r'((?:\s*\[[^\]]*\])+)$', core)
                if m2:
                    arrays = m2.group(1)
                    core = core[:m2.start()].rstrip()

                if re.search(r'\b[A-Za-z_][A-Za-z0-9_]*$', core) and not core.endswith(("*", "&")):
                    plist.append(p)               # already named
                else:
                    plist.append(f"{core} a{i}{arrays}")
        sig_params = ", ".join(plist) if plist else "void"
        idx = len(bodies)
        lines = [f"\tSTUB_HIT({idx});"]
        if zero:
            lines.append(f"\treturn {zero};")
        bodies.append(f"{normalise_ret(ret)} {sym}({sig_params})\n{{\n" + "\n".join(lines) + "\n}\n")
        names.append(sym)

    with open(args.out, "w") as f:
        f.write(f"""/*
 * Generated stubs for {args.prefix}* -- DO NOT EDIT BY HAND.
 *
 * Produced by tools/port/gen_stubs.py from the declarations in include/.
 * Every one of these does nothing and returns zero. They exist so the program
 * can link and run, which surfaces real problems in the code that is already
 * written; they are not an implementation and nothing here is correct.
 *
 * Replace them a subsystem at a time by deleting the entries from this file as
 * real versions appear.
 */

#include <stddef.h>

""")
        for inc in args.include:
            f.write(f'#include "{inc}"\n')
        f.write(f"""
/* Call counting. Which of these the game actually uses, and how often, decides
 * what a renderer has to implement first -- measured rather than assumed. Set
 * PIKMIN_TRACE to have the totals printed at exit. */
#define STUB_COUNT {len(bodies)}
static unsigned long g_stub_hits[STUB_COUNT];
static const char* const g_stub_names[STUB_COUNT] = {{
{chr(10).join('    "' + n + '",' for n in names)}
}};
#define STUB_HIT(i) (g_stub_hits[(i)]++)

__attribute__((constructor)) static void stub_register(void)
{{
    traceRegisterTable("{args.prefix}", g_stub_hits, g_stub_names, STUB_COUNT);
}}

""")
        f.write("\n".join(bodies))

    print(f"{args.out}: {len(bodies)} stubs")
    if missing:
        print(f"  no declaration found for {len(missing)}:", file=sys.stderr)
        for s in missing:
            print("   ", s, file=sys.stderr)


if __name__ == "__main__":
    main()
