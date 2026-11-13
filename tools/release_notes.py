#!/usr/bin/env python3
"""Print one version's section of CHANGELOG.md (the text under "## <version>").

    python3 tools/release_notes.py 1.41 > notes.md

Exits 1 when the CHANGELOG has no section for that version.
"""
import io
import re
import sys


def notes(changelog, version):
    out, on = [], False
    for line in changelog.splitlines():
        if line.startswith("## "):
            if on:
                break
            on = re.match(r"## %s(\s|$)" % re.escape(version), line) is not None
            continue
        if on:
            out.append(line)
    return "\n".join(out).strip() if on else None


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    text = notes(io.open("CHANGELOG.md", encoding="utf-8").read(), sys.argv[1])
    if text is None:
        sys.exit("CHANGELOG.md has no section for %s" % sys.argv[1])
    print(text)
