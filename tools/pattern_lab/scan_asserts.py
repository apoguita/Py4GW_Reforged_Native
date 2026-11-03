"""Inventory the Guild Wars assert-macro call sites in a Gw.exe image.

The client's `Assert(line, file, message)` lowers to a very regular code shape:

    push <line32>        ; 68 imm32
    mov  edx, <file>     ; BA imm32
    mov  ecx, <message>  ; B9 imm32

(or `push <line8>` / 6A imm8 for small line numbers). Both `file` and `message`
are pointers into .rdata, so a build can be inventoried by locating that shape,
resolving the two pointers to strings, and pairing the site with the line
number.

That inventory is what lets a broken assertion-anchored pattern be re-derived
after a client update: the (file, message) literals are source-stable even when
line numbers, .rdata layout and .text addresses all shift.

Usage:
    python scan_asserts.py --exe F:\\GW\\GW1\\Gw.exe
    python scan_asserts.py --exe Gw.exe --msg 'frame|size' --file 'Fr'
    python scan_asserts.py --exe Gw.exe --after 0x631000 --before 0x636000
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path
from typing import List, Optional, Tuple

from resolve_offsets import Image

ASSERT_SHAPE = re.compile(rb"\xBA(....)\xB9(....)", re.DOTALL)


def _cstring(image: Image, va: int, limit: int = 96) -> Optional[str]:
    if not image.in_section(va, "rdata"):
        return None
    raw = image.read(va, limit)
    if raw is None:
        return None
    end = raw.find(b"\0")
    if end <= 0:
        return None
    text = raw[:end]
    if any(byte < 0x20 or byte > 0x7E for byte in text):
        return None
    return text.decode("ascii")


def _line_at(image: Image, site: int) -> Tuple[Optional[int], int]:
    """Return (line_number, push_width) for the push feeding this assert."""
    if image.read_u8(site - 2) == 0x6A:
        value = image.read_u8(site - 1)
        return (value if value is not None else None), 1
    if image.read_u8(site - 5) == 0x68:
        raw = image.read(site - 4, 4)
        if raw is not None:
            return struct.unpack("<I", raw)[0], 4
    return None, 0


def _function_entry(image: Image, site: int, window: int = 0x60) -> Tuple[int, int]:
    """Nearest `push ebp; mov ebp, esp` prologue at or before the assert site."""
    for delta in range(0, window):
        candidate = site - delta
        if image.read(candidate, 3) == b"\x55\x8B\xEC":
            return candidate, delta
    return 0, 0


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", required=True, help="path to Gw.exe to inventory")
    parser.add_argument("--msg", default="", help="regex filter on the assert message")
    parser.add_argument("--file", default="", help="regex filter on the assert file")
    parser.add_argument("--after", default="", help="only sites at or above this VA")
    parser.add_argument("--before", default="", help="only sites below this VA")
    parser.add_argument("--limit", type=int, default=0, help="stop after N printed rows")
    parser.add_argument("--func-entry", action="store_true", help="report the enclosing function prologue")
    args = parser.parse_args(argv)

    exe_path = Path(args.exe)
    if not exe_path.is_file():
        print(f"exe not found: {exe_path}", file=sys.stderr)
        return 2

    image = Image(exe_path)
    text = image.sections["text"]
    window = image.window(text.start, text.end)
    if window is None:
        print("unable to read .text", file=sys.stderr)
        return 2
    base, buf = window

    msg_filter = re.compile(args.msg) if args.msg else None
    file_filter = re.compile(args.file) if args.file else None
    after = int(args.after, 0) if args.after else 0
    before = int(args.before, 0) if args.before else 0xFFFFFFFF

    rows = 0
    print(f"image: {exe_path}  base=0x{image.image_base:X}  .text=0x{text.start:X}")
    for match in ASSERT_SHAPE.finditer(buf):
        file_va = struct.unpack("<I", match.group(1))[0]
        msg_va = struct.unpack("<I", match.group(2))[0]
        message = _cstring(image, msg_va)
        if message is None:
            continue
        assertion_file = _cstring(image, file_va, 160)
        if assertion_file is None:
            continue
        site = base + match.start()
        if not (after <= site <= before):
            continue
        if msg_filter and not msg_filter.search(message):
            continue
        if file_filter and not file_filter.search(assertion_file):
            continue
        line, width = _line_at(image, site)
        line_text = f"0x{line:X}" if line is not None else "?"
        entry_text = ""
        if args.func_entry:
            func_va, delta = _function_entry(image, site)
            entry_text = (f"  entry=0x{func_va:08X} delta=0x{delta:X}"
                          if func_va else "  entry=?")
        print(f"0x{site:08X}  line={line_text:<8} push{width * 8:<3} "
              f"file={assertion_file}{entry_text}")
        print(f"{'':12}  msg={message!r}  va(file=0x{file_va:08X}, msg=0x{msg_va:08X})")
        rows += 1
        if args.limit and rows >= args.limit:
            break
    print("")
    print(f"assert sites: {rows}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
