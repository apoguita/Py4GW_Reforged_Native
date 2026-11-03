"""Offline emulator for the PY4GW::Patterns resolver tables.

The injected DLL resolves native pointers at startup by running the declarative
`resolvers` programs in `offsets/*.json` through `FileScanner`. When a client
build changes, some of those programs stop resolving and the client log only
says "scan returned null". This script replays the same tables against a
`Gw.exe` image on disk so a build can be audited before injection.

Fidelity notes (see src/base/file_scanner.cpp, src/base/scanner.cpp and
src/base/patterns.cpp):

* Runtime scans run against a SEC_IMAGE view of the client mapped at its
  preferred base, so absolute virtual addresses inside a pattern (the assert
  macro's `mov edx,<file>` / `mov ecx,<message>` operands) are the preferred
  base values. Emulating against the file image with `ImageBase + RVA`
  addresses therefore matches runtime behavior for byte patterns.
* `FindInRange` subtracts the pattern length from the end bound, scans forward
  with an exclusive upper bound and backward with an inclusive one, and starts a
  backward scan at `start` itself. Those quirks are reproduced exactly because
  several resolvers depend on them.
* `FunctionFromNearCall` follows nested `E8`/`E9`/`EB` thunks until something
  else appears; `FunctionFromNearCall` recursion is capped here instead of
  overflowing the stack.
* Section ranges use `VirtualSize`, matching `FileScanner`.

Usage:
    python resolve_offsets.py --exe F:\\GW\\GW1\\Gw.exe --offsets offsets
    python resolve_offsets.py --exe Gw.exe --offsets offsets --only ui
    python resolve_offsets.py --exe Gw.exe --offsets offsets --json ui.json --verbose
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

SECTION_NAMES = ("text", "rdata", "data")
SECTION_MAP = {".text": "text", ".rdata": "rdata", ".data": "data"}
MAX_THUNK_DEPTH = 32


def _parse_int(text: object) -> int:
    if isinstance(text, bool):
        return int(text)
    if isinstance(text, int):
        return text
    if isinstance(text, str):
        stripped = text.strip()
        return int(stripped, 0)
    raise ValueError(f"not an integer: {text!r}")


def _decode_literal(literal: str) -> bytes:
    """Decode the JSON `pattern` field exactly like DecodePatternLiteral()."""
    out = bytearray()
    index = 0
    while index < len(literal):
        char = literal[index]
        if char != "\\":
            out.append(ord(char) & 0xFF)
            index += 1
            continue
        index += 1
        if index >= len(literal):
            raise ValueError("dangling escape in pattern literal")
        escape = literal[index]
        index += 1
        if escape == "\\":
            out.append(ord("\\"))
        elif escape == "0":
            out.append(0)
        elif escape == "n":
            out.append(0x0A)
        elif escape == "r":
            out.append(0x0D)
        elif escape == "t":
            out.append(0x09)
        elif escape == "x":
            hex_pair = literal[index:index + 2]
            if len(hex_pair) != 2:
                raise ValueError("truncated \\xNN escape")
            out.append(int(hex_pair, 16))
            index += 2
        else:
            out.append(ord(escape) & 0xFF)
    return bytes(out)


@dataclass
class Section:
    name: str
    va: int          # preferred-base virtual address of section start
    vsize: int       # VirtualSize
    raw: int         # file offset
    rsize: int       # SizeOfRawData

    @property
    def start(self) -> int:
        return self.va

    @property
    def end(self) -> int:
        return self.va + self.vsize


class Image:
    """A PE image addressable by preferred-base virtual address."""

    def __init__(self, path: Path) -> None:
        self.path = path
        self.data = path.read_bytes()
        self.image_base = self._read_image_base()
        self.sections: Dict[str, Section] = self._read_sections()

    # ---- PE parsing -----------------------------------------------------
    def _read_image_base(self) -> int:
        pe_off = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[pe_off:pe_off + 4] != b"PE\0\0":
            raise ValueError(f"not a PE file: {self.path}")
        return struct.unpack_from("<I", self.data, pe_off + 24 + 28)[0]

    def _read_sections(self) -> Dict[str, Section]:
        pe_off = struct.unpack_from("<I", self.data, 0x3C)[0]
        count = struct.unpack_from("<H", self.data, pe_off + 6)[0]
        opt_size = struct.unpack_from("<H", self.data, pe_off + 20)[0]
        table = pe_off + 24 + opt_size
        sections: Dict[str, Section] = {}
        for index in range(count):
            base = table + index * 40
            raw_name = self.data[base:base + 8].rstrip(b"\0").decode("ascii", "replace")
            vsize, va, rsize, raw = struct.unpack_from("<IIII", self.data, base + 8)
            key = SECTION_MAP.get(raw_name)
            if key is None:
                continue
            sections[key] = Section(
                name=raw_name,
                va=self.image_base + va,
                vsize=vsize,
                raw=raw,
                rsize=rsize,
            )
        missing = [name for name in SECTION_NAMES if name not in sections]
        if missing:
            raise ValueError(f"missing sections {missing} in {self.path}")
        return sections

    # ---- address translation -------------------------------------------
    def section_of(self, va: int) -> Optional[Section]:
        for section in self.sections.values():
            if section.start <= va < section.end:
                return section
        return None

    def read(self, va: int, size: int) -> Optional[bytes]:
        section = self.section_of(va)
        if section is None:
            return None
        delta = va - section.start
        file_off = section.raw + delta
        if delta < 0 or file_off + size > len(self.data):
            return None
        return self.data[file_off:file_off + size]

    def read_u8(self, va: int) -> Optional[int]:
        raw = self.read(va, 1)
        return None if raw is None else raw[0]

    def read_u32(self, va: int) -> Optional[int]:
        raw = self.read(va, 4)
        return None if raw is None else struct.unpack("<I", raw)[0]

    def window(self, start: int, end: int) -> Optional[Tuple[int, bytes]]:
        """Contiguous file bytes for [start, end), clipped to the section."""
        if end <= start:
            return None
        section = self.section_of(start)
        if section is None:
            return None
        if end > section.end:
            end = section.end
        delta = start - section.start
        file_off = section.raw + delta
        length = min(end - start, len(self.data) - file_off, section.rsize - delta)
        if length <= 0:
            return None
        return start, self.data[file_off:file_off + length]

    def in_section(self, va: int, name: str) -> bool:
        section = self.sections.get(name)
        if section is None:
            return False
        return section.start < va < section.end


def _anchor_run(mask: str, length: int) -> Tuple[int, int]:
    """Longest run of compared bytes, used as the literal search needle."""
    best_start, best_len = 0, 0
    cur_start, cur_len = 0, 0
    for index in range(length):
        if mask[index] == "x":
            if cur_len == 0:
                cur_start = index
            cur_len += 1
        else:
            if cur_len > best_len:
                best_start, best_len = cur_start, cur_len
            cur_len = 0
    if cur_len > best_len:
        best_start, best_len = cur_start, cur_len
    return best_start, best_len


def _matches(window: bytes, base: int, pos: int, pattern: bytes, mask: str) -> bool:
    rel = pos - base
    if rel < 0 or rel + len(pattern) > len(window):
        return False
    if len(mask) > len(pattern):
        return False
    for index, mask_char in enumerate(mask):
        if mask_char == "x" and window[rel + index] != pattern[index]:
            return False
    return True


def find_in_range(image: Image, pattern: bytes, mask: str, offset: int,
                  start: int, end: int) -> int:
    """Replay FileScanner::FindInRange over preferred-base virtual addresses."""
    if not pattern:
        return 0
    length = len(mask) if mask else len(pattern)
    end_bound = end - length
    if start > end_bound:
        lo, hi = end_bound, start + length
        reverse = True
    else:
        lo, hi = start, end_bound + length
        reverse = False

    window = image.window(lo, hi)
    if window is None:
        return 0
    base, buf = window
    if len(buf) < length:
        return 0

    anchor_start, anchor_len = _anchor_run(mask if mask else "x" * length, length)
    if anchor_len == 0:
        return 0
    needle = pattern[anchor_start:anchor_start + anchor_len]

    first = pattern[0]
    if reverse:
        pos = buf.rfind(needle, 0, min(len(buf), start - base + length))
        while pos >= 0:
            candidate = base + pos - anchor_start
            if end_bound <= candidate <= start and _matches(buf, base, candidate, pattern, mask):
                return candidate + offset
            pos = buf.rfind(needle, 0, pos)
    else:
        pos = buf.find(needle)
        while pos >= 0:
            candidate = base + pos - anchor_start
            if start <= candidate < end_bound:
                if image.read_u8(candidate) == first and _matches(buf, base, candidate, pattern, mask):
                    return candidate + offset
            pos = buf.find(needle, pos + 1)
    return 0


def find(image: Image, pattern: bytes, mask: str, offset: int, section: str) -> int:
    sec = image.sections[section]
    return find_in_range(image, pattern, mask, offset, sec.start, sec.end)


def find_assertion(image: Image, assertion_file: str, assertion_message: str,
                   line_number: int, offset: int) -> int:
    """Replay FileScanner::FindAssertion for the installed assert macro."""
    if not assertion_message:
        return 0

    # FindAssertion builds its mask one byte longer than the literal, so the
    # comparison always includes the NUL terminator stored in .rdata.
    msg_bytes = assertion_message.encode("ascii") + b"\0"
    msg_mask = "x" * len(msg_bytes)

    file_bytes = assertion_file.encode("ascii") + b"\0"
    file_mask = "x" * len(file_bytes)
    file_variants = [file_bytes]
    lowered = assertion_file.lower().encode("ascii") + b"\0"
    ucwords = _ucwords(assertion_file).encode("ascii") + b"\0"
    if lowered != file_bytes:
        file_variants.append(lowered)
    if ucwords != file_bytes and ucwords != lowered:
        file_variants.append(ucwords)

    rdata = image.sections["rdata"]
    msg_cursor = rdata.start
    while True:
        msg_va = find_in_range(image, msg_bytes, msg_mask, 0, msg_cursor, rdata.end)
        if not msg_va:
            return 0
        msg_cursor = msg_va + 1

        for variant in file_variants:
            file_cursor = rdata.start
            while True:
                file_va = find_in_range(image, variant, file_mask, 0, file_cursor, rdata.end)
                if not file_va:
                    break
                file_cursor = file_va + 1

                # The stored literal carries the drive letter ("F:\\Code\\...");
                # walk back over it so the encoded pointer matches the real literal.
                raw = image.read(file_va, 2)
                if raw is None or len(raw) < 2 or raw[1] != ord(":"):
                    colon = find_in_range(image, b":", "x", -1, file_va, file_va - 128)
                    if not colon:
                        break
                    file_va = colon

                if line_number:
                    if (line_number & 0xFF) == line_number:
                        built = bytes([0x6A, line_number]) + b"\xBA" + struct.pack("<I", file_va) \
                            + b"\xB9" + struct.pack("<I", msg_va)
                        built_mask = "x" * len(built)
                        found = find(image, built, built_mask, offset, "text")
                        if found:
                            return found
                    built = b"\x68" + struct.pack("<I", line_number) + b"\xBA" \
                        + struct.pack("<I", file_va) + b"\xB9" + struct.pack("<I", msg_va)
                    built_mask = "x" * len(built)
                    found = find(image, built, built_mask, offset, "text")
                    if found:
                        return found
                else:
                    built = b"\xBA" + struct.pack("<I", file_va) + b"\xB9" + struct.pack("<I", msg_va)
                    built_mask = "x" * len(built)
                    found = find(image, built, built_mask, offset, "text")
                    if found:
                        return found
    return 0


def _ucwords(text: str) -> str:
    chars = list(text)
    for index, char in enumerate(chars):
        if char.isalpha() and (index == 0 or not chars[index - 1].isalpha()):
            chars[index] = char.upper()
    return "".join(chars)


def to_function_start(image: Image, address: int, scan_range: int) -> int:
    if not address:
        return 0
    return find_in_range(image, b"\x55\x8B\xEC", "xxx", 0, address, address - scan_range)


def function_from_near_call(image: Image, address: int, check_valid: bool = True) -> int:
    if not image.in_section(address, "text"):
        return 0
    for _ in range(MAX_THUNK_DEPTH):
        raw = image.read_u8(address)
        if raw is None:
            return 0
        if raw in (0xE8, 0xE9):
            rel = image.read_u32(address + 1)
            if rel is None:
                return 0
            target = (rel + address + 5) & 0xFFFFFFFF
        elif raw == 0xEB:
            delta = image.read_u8(address + 1)
            if delta is None:
                return 0
            delta = delta - 256 if delta > 127 else delta
            target = (address + 2 + delta) & 0xFFFFFFFF
        else:
            return 0
        if check_valid and not image.in_section(target, "text"):
            return 0
        if image.read_u8(target) in (0xE8, 0xE9, 0xEB):
            address = target
            continue
        return target
    return 0


def find_nth_use_of_address(image: Image, address: int, nth: int, offset: int,
                            section: str) -> int:
    if not address:
        return 0
    needle = struct.pack("<I", address)
    sec = image.sections[section]
    found = find_in_range(image, needle, "xxxx", 0, sec.start, sec.end)
    if not found:
        return 0
    for _ in range(nth):
        found = find_in_range(image, needle, "xxxx", 0, found + 1, sec.end)
        if not found:
            return 0
    return found + offset


def find_nth_use_of_string(image: Image, literal: str, nth: int, offset: int,
                           section: str) -> int:
    raw = literal.encode("ascii") + b"\0"
    mask = "x" * len(raw)
    found_str = find(image, raw, mask, 0, "rdata")
    if not found_str:
        return 0
    first_null = find_in_range(image, b"\0", "x", 1, found_str, found_str - 0x64)
    if not first_null:
        return 0
    return find_nth_use_of_address(image, first_null, nth, offset, section)


# ---- pattern table + resolver interpreter ------------------------------

@dataclass
class Pattern:
    name: str
    pattern: bytes = b""
    mask: str = ""
    assertion_file: str = ""
    assertion_message: str = ""
    offset: int = 0
    line_number: int = 0
    range: int = 0
    section: str = "text"


@dataclass
class Step:
    name: str
    op: str
    output: str
    input: str = ""
    pattern_name: str = ""
    literal: str = ""
    start: str = ""
    end: str = ""
    start_add: int = 0
    end_add: int = 0
    value: int = 0
    nth: int = 0
    scan_range: int = 0xFF
    check_valid_ptr: bool = True
    wide: bool = False
    section: Optional[str] = None


@dataclass
class Resolver:
    name: str
    attempts: List[Tuple[str, List[Step]]] = field(default_factory=list)
    log_level: str = "error"
    on_fail: str = "halt"


def _qualify(namespace: str, name: str) -> str:
    if not namespace or "." in name:
        return name
    return f"{namespace}.{name}"


def _load_step(namespace: str, value: dict) -> Step:
    raw_pattern = value.get("pattern", "")
    section = value.get("section")
    return Step(
        name=value.get("name", value.get("op", "")),
        op=value.get("op", "").lower(),
        output=value.get("out", "value") or "value",
        input=value.get("in", ""),
        pattern_name=_qualify(namespace, raw_pattern) if raw_pattern else "",
        literal=value.get("literal", ""),
        start=value.get("start", ""),
        end=value.get("end", ""),
        start_add=_parse_int(value.get("start_add", 0)),
        end_add=_parse_int(value.get("end_add", 0)),
        value=_parse_int(value.get("value", 0)),
        nth=_parse_int(value.get("nth", 0)),
        scan_range=_parse_int(value.get("scan_range", "0xFF")),
        check_valid_ptr=bool(value.get("check_valid_ptr", True)),
        wide=bool(value.get("wide", False)),
        section=section.lower() if section else None,
    )


@dataclass
class TraceStep:
    name: str
    op: str
    input: int
    output: int
    ok: bool
    detail: str = ""


@dataclass
class Outcome:
    ok: bool
    value: int = 0
    attempt: str = ""
    failed_step: str = ""
    message: str = ""
    trace: List[TraceStep] = field(default_factory=list)


def run_attempt(image: Image, patterns: Dict[str, Pattern], steps: List[Step],
                name: str) -> Outcome:
    values: Dict[str, int] = {}
    trace: List[TraceStep] = []
    for step in steps:
        op = step.op
        output = 0
        ok = False
        detail = ""
        step_input = 0

        def lookup(key: str) -> Optional[int]:
            return values.get(key)

        if op in ("scan", "pattern_scan", "pattern"):
            pattern = patterns.get(step.pattern_name)
            if pattern is None:
                detail = f"pattern not found: {step.pattern_name}"
            elif pattern.assertion_file or pattern.assertion_message:
                output = find_assertion(image, pattern.assertion_file,
                                        pattern.assertion_message,
                                        pattern.line_number, pattern.offset)
                ok = output != 0
                detail = "" if ok else "assertion scan returned null"
            else:
                output = find(image, pattern.pattern, pattern.mask, pattern.offset,
                              pattern.section)
                ok = output != 0
                detail = "" if ok else "scan returned null"
        elif op in ("scan_in_range", "pattern_scan_in_range", "find_in_range"):
            pattern = patterns.get(step.pattern_name)
            start_val = lookup(step.start)
            end_val = lookup(step.end)
            if pattern is None:
                detail = f"pattern not found: {step.pattern_name}"
            elif start_val is None:
                detail = f"start reference not found: {step.start}"
            elif end_val is None:
                detail = f"end reference not found: {step.end}"
            else:
                start_addr = (start_val + step.start_add) & 0xFFFFFFFF
                end_addr = (end_val + step.end_add) & 0xFFFFFFFF
                step_input = start_addr
                output = find_in_range(image, pattern.pattern, pattern.mask,
                                       pattern.offset, start_addr, end_addr)
                ok = output != 0
                detail = "" if ok else "range scan returned null"
        elif op == "to_function_start":
            src = lookup(step.input)
            if src is None:
                detail = f"input reference not found: {step.input}"
            else:
                step_input = src
                output = to_function_start(image, src, step.scan_range)
                ok = output != 0
                detail = "" if ok else "resolved function start is null"
        elif op == "function_from_near_call":
            src = lookup(step.input)
            if src is None:
                detail = f"input reference not found: {step.input}"
            else:
                step_input = src
                output = function_from_near_call(image, src, step.check_valid_ptr)
                ok = output != 0
                detail = "" if ok else "near-call target is null"
        elif op in ("find_use_of_string", "use_of_string", "scan_use_of_string"):
            literal = step.literal
            offset = 0
            section = "text"
            if step.pattern_name:
                pattern = patterns.get(step.pattern_name)
                if pattern is None:
                    detail = f"pattern not found: {step.pattern_name}"
                else:
                    try:
                        literal = pattern.pattern.decode("ascii")
                    except UnicodeDecodeError:
                        literal = ""
                    offset = pattern.offset
                    section = pattern.section
            if not detail:
                if not literal:
                    detail = "string literal is empty"
                else:
                    output = find_nth_use_of_string(image, literal, step.nth, offset, section)
                    ok = output != 0
                    detail = "" if ok else "string use scan returned null"
        elif op in ("dereference", "deref"):
            src = lookup(step.input)
            if src is None:
                detail = f"input reference not found: {step.input}"
            else:
                step_input = src
                read = image.read_u32(src) if src else None
                if not src:
                    detail = "cannot dereference null input"
                elif read is None:
                    detail = "cannot dereference unreadable input"
                else:
                    output = read
                    ok = output != 0
                    detail = "" if ok else "dereferenced pointer is null"
        elif op in ("read_u32", "read_uint32"):
            src = lookup(step.input)
            if src is None:
                detail = f"input reference not found: {step.input}"
            else:
                step_input = src
                read = image.read_u32(src) if src else None
                if not src:
                    detail = "cannot read from null input"
                elif read is None:
                    detail = "cannot read from unreadable input"
                else:
                    output = read
                    ok = True
        elif op == "add":
            src = lookup(step.input)
            if src is None:
                detail = f"input reference not found: {step.input}"
            else:
                step_input = src
                output = (src + step.value) & 0xFFFFFFFF
                ok = output != 0
                detail = "" if ok else "arithmetic result is null"
        elif op in ("divide", "div"):
            src = lookup(step.input)
            if src is None:
                detail = f"input reference not found: {step.input}"
            elif step.value == 0:
                detail = "cannot divide by zero"
            else:
                step_input = src
                output = src // step.value
                ok = True
        elif op == "validate_section":
            src = lookup(step.input)
            if src is None:
                detail = f"input reference not found: {step.input}"
            else:
                step_input = src
                output = src
                ok = image.in_section(src, step.section or "text")
                detail = "" if ok else "pointer is outside expected section"
        else:
            detail = f"unsupported op: {step.op}"

        trace.append(TraceStep(step.name, op, step_input, output, ok, detail))
        if not ok:
            return Outcome(False, attempt=name, failed_step=step.name,
                           message=detail, trace=trace)
        values[step.output] = output

    if "final" not in values:
        return Outcome(False, attempt=name, failed_step="final",
                       message="resolver did not populate final output", trace=trace)
    return Outcome(True, value=values["final"], attempt=name, trace=trace)


def resolve(image: Image, patterns: Dict[str, Pattern], resolver: Resolver) -> Outcome:
    last = Outcome(False, attempt="", message="no attempts")
    for attempt_name, steps in resolver.attempts:
        outcome = run_attempt(image, patterns, steps, attempt_name)
        if outcome.ok:
            return outcome
        last = outcome
    if len(resolver.attempts) > 1 and last.message:
        last.message = f"all attempts failed; last_error={last.message}"
    return last


def format_trace(trace: Iterable[TraceStep]) -> str:
    parts = []
    for step in trace:
        piece = (f"{step.name}:{step.op}(in=0x{step.input:X}, out=0x{step.output:X}, "
                 f"ok={'true' if step.ok else 'false'}")
        if step.detail:
            piece += f", detail={step.detail}"
        parts.append(piece + ")")
    return " trace=[" + "; ".join(parts) + "]"


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", required=True, help="path to Gw.exe to audit")
    parser.add_argument("--offsets", required=True, help="directory holding offsets/*.json")
    parser.add_argument("--only", default="", help="regex filter on resolver name")
    parser.add_argument("--json", default="", help="only load this offsets json file")
    parser.add_argument("--verbose", action="store_true", help="print every resolver result")
    parser.add_argument("--show-pass", action="store_true", help="also print resolved values")
    args = parser.parse_args(argv)

    exe_path = Path(args.exe)
    offsets_dir = Path(args.offsets)
    if not exe_path.is_file():
        print(f"exe not found: {exe_path}", file=sys.stderr)
        return 2
    if not offsets_dir.is_dir():
        print(f"offsets dir not found: {offsets_dir}", file=sys.stderr)
        return 2

    image = Image(exe_path)

    if args.json:
        pattern_sources = [offsets_dir / args.json]
    else:
        pattern_sources = sorted(offsets_dir.glob("*.json"))

    patterns: Dict[str, Pattern] = {}
    resolvers: Dict[str, Resolver] = {}
    for path in pattern_sources:
        loaded_patterns, loaded_resolvers = load_tables_from(path)
        patterns.update(loaded_patterns)
        resolvers.update(loaded_resolvers)

    import re
    matcher = re.compile(args.only) if args.only else None

    selected = [res for name, res in sorted(resolvers.items())
                if not matcher or matcher.search(name)]

    failures = 0
    print(f"image    : {exe_path}")
    print(f"base     : 0x{image.image_base:X}")
    for key in SECTION_NAMES:
        sec = image.sections[key]
        print(f"section  : {key:<5} va=0x{sec.start:X} vsize=0x{sec.vsize:X}")
    print(f"resolvers: {len(selected)} (patterns loaded: {len(patterns)})")
    print("")

    for resolver in selected:
        outcome = resolve(image, patterns, resolver)
        if outcome.ok:
            if args.show_pass or args.verbose:
                print(f"  OK      {resolver.name:<48} 0x{outcome.value:X}")
            continue
        failures += 1
        print(f"  FAIL    {resolver.name:<48} step={outcome.failed_step} "
              f"reason={outcome.message} policy={resolver.on_fail}")
        if args.verbose:
            print(f"          {format_trace(outcome.trace)}")

    print("")
    print(f"totals: resolved={len(selected) - failures} failed={failures}")
    return 0 if failures == 0 else 1


def load_tables_from(path: Path) -> Tuple[Dict[str, Pattern], Dict[str, Resolver]]:
    patterns: Dict[str, Pattern] = {}
    resolvers: Dict[str, Resolver] = {}
    root = json.loads(path.read_text(encoding="utf-8-sig"))
    namespace = root.get("namespace", "")
    for raw_name, value in root.get("patterns", {}).items():
        name = _qualify(namespace, raw_name)
        literal = value.get("pattern", "")
        patterns[name] = Pattern(
            name=name,
            pattern=_decode_literal(literal) if literal else b"",
            mask=value.get("mask", ""),
            assertion_file=value.get("assertion_file", ""),
            assertion_message=value.get("assertion_message", ""),
            offset=_parse_int(value.get("offset", "0")),
            line_number=_parse_int(value.get("line_number", "0")),
            range=_parse_int(value.get("range", "0")),
            section=value.get("section", "text").lower(),
        )
    for raw_name, value in root.get("resolvers", {}).items():
        name = _qualify(namespace, raw_name)
        resolver = Resolver(
            name=name,
            log_level=value.get("log_level", "error"),
            on_fail=value.get("on_fail", "halt"),
        )
        raw_attempts = value.get("attempts")
        if raw_attempts:
            for attempt in raw_attempts:
                resolver.attempts.append((attempt.get("name", ""),
                                          [_load_step(namespace, step) for step in attempt["steps"]]))
        else:
            resolver.attempts.append(("default",
                                      [_load_step(namespace, step) for step in value["steps"]]))
        resolvers[name] = resolver
    return patterns, resolvers


if __name__ == "__main__":
    raise SystemExit(main())
