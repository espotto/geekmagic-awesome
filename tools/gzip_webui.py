#!/usr/bin/env python3
"""Regenerate src/webui.h from src/webui.html.

The web UI ships as a gzip-compressed byte array in PROGMEM and is served with
`Content-Encoding: gzip` (see handleRoot in src/WebPortal.cpp). src/webui.html
is the source of truth — edit it, then rerun:

    python tools/gzip_webui.py

Before compressing, the page is minified conservatively (minify() below):
HTML comments, indentation and blank lines go, CSS loses comments and the
spaces around braces, semicolons and commas, and JavaScript loses its
comment-only lines and indentation. Every line break in a script stays, so
automatic semicolon insertion reads the code as before, and nothing inside a
quoted string is touched. It saves flash on the device; webui.html stays
readable.

Python 3 stdlib only. Output is deterministic (gzip header mtime=0), so
re-running without changes leaves webui.h untouched on the same machine.
Another zlib build (zlib-ng, say) can compress to other bytes, so CI runs
`python tools/gzip_webui.py --check`, which compares what webui.h
decompresses to with the minified webui.html instead.
"""
import gzip
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "webui.html"
OUT = ROOT / "src" / "webui.h"

HEADER = """\
// GENERATED FILE -- do not edit by hand.
// Produced by tools/gzip_webui.py from src/webui.html (the source of truth).
// Regenerate after editing the HTML:  python tools/gzip_webui.py
//
// Gzip-compressed single-page config UI, served from PROGMEM with
// `Content-Encoding: gzip` by handleRoot in WebPortal.cpp.
#pragma once
#include <Arduino.h>

static const uint8_t WEBUI_HTML_GZ[] PROGMEM = {
"""


def html() -> bytes:
    # The repo stores LF; a Windows checkout with core.autocrlf has CRLF.
    return SRC.read_bytes().replace(b"\r\n", b"\n")


def _css(text: str) -> str:
    """Comments out, whitespace collapsed, no space around { } ; , -- outside quotes."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":                       # a quoted string, kept as it is
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif c.isspace():
            while i < n and text[i].isspace():
                i += 1
            prev = out[-1][-1:] if out else ""
            nxt = text[i:i + 1]
            if prev and prev not in "{};," and nxt not in "{};,":
                out.append(" ")
        else:
            out.append(c)
            i += 1
    return "".join(out).strip()


def _js(text: str) -> str:
    """Indentation and comment-only lines out; every remaining line break kept."""
    lines = []
    for line in text.split("\n"):
        t = line.strip()
        if t and not t.startswith("//"):
            lines.append(t)
    return "\n".join(lines)


def _html(text: str) -> str:
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    return "\n".join(t for t in (l.strip() for l in text.split("\n")) if t)


def minify(page: bytes) -> bytes:
    src = page.decode("utf-8")
    parts, pos = [], 0
    for m in re.finditer(r"(<style[^>]*>)(.*?)(</style>)|(<script[^>]*>)(.*?)(</script>)", src, flags=re.S):
        parts.append(_html(src[pos:m.start()]))
        if m.group(1):
            parts.append(m.group(1) + _css(m.group(2)) + m.group(3))
        else:
            parts.append(m.group(4) + _js(m.group(5)) + m.group(6))
        pos = m.end()
    parts.append(_html(src[pos:]))
    return "\n".join(p for p in parts if p).encode("utf-8")


def check() -> int:
    body = OUT.read_text(encoding="utf-8").split("PROGMEM = {", 1)[1]
    blob = bytes(int(b, 16) for b in re.findall(r"0x([0-9a-f]{2})", body))
    if gzip.decompress(blob) == minify(html()):
        print(f"{OUT.name} matches {SRC.name}")
        return 0
    print(f"{OUT.name} is out of date: run python tools/gzip_webui.py")
    return 1


def main() -> None:
    raw = minify(html())
    blob = gzip.compress(raw, compresslevel=9, mtime=0)

    lines = [HEADER]
    for i in range(0, len(blob), 16):
        chunk = blob[i:i + 16]
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
    lines.append("};\n")
    lines.append("static const size_t WEBUI_HTML_GZ_LEN = sizeof(WEBUI_HTML_GZ);\n")

    with open(OUT, "w", encoding="utf-8", newline="\n") as f:   # Python 3.9: no write_text(newline=)
        f.write("".join(lines))
    print(f"{SRC.name}: {len(html())} B, minified {len(raw)} B -> gzip {len(blob)} B "
          f"({100 * len(blob) / len(raw):.1f}%) -> {OUT.name}")


if __name__ == "__main__":
    if sys.argv[1:] == ["--check"]:
        sys.exit(check())
    main()
