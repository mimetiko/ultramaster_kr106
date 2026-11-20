#!/usr/bin/env python3
"""Materialise the KR-106 Studio Edition GUI on the Windows build runner.

The source payload holds the two JUCE editor source files. The audio engine,
parameter identifiers, preset format and plugin identifiers are left unchanged.
"""
from __future__ import annotations

import base64
import io
import lzma
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[2]
PAYLOAD = Path(__file__).with_name("studio_source.xz.b64")
REQUIRED = {"Source/PluginEditor.cpp", "Source/PluginEditor.h"}

archive_bytes = lzma.decompress(base64.b64decode(PAYLOAD.read_text(encoding="ascii")))
with tarfile.open(fileobj=io.BytesIO(archive_bytes), mode="r:") as archive:
    names = set(archive.getnames())
    if names != REQUIRED:
        raise RuntimeError(f"Unexpected Studio Edition source files: {names}")
    for member_name in sorted(REQUIRED):
        member = archive.getmember(member_name)
        contents = archive.extractfile(member)
        if contents is None or not member.isfile():
            raise RuntimeError(f"Missing Studio Edition file: {member_name}")
        target = ROOT / member_name
        target.write_bytes(contents.read())
        print(f"Prepared {target.relative_to(ROOT)}")

# Recolour only the existing scope. The synthesizer engine stays unmodified.
scope = ROOT / "Source/Controls/KR106Scope.h"
text = scope.read_text(encoding="utf-8")
colours = {
    "juce::Colour(0, 0, 0)": "juce::Colour(13, 19, 28)",
    "juce::Colour(0, 64, 0)": "juce::Colour(27, 60, 69)",
    "juce::Colour(0, 128, 0)": "juce::Colour(55, 121, 123)",
    "juce::Colour(0, 192, 0)": "juce::Colour(79, 173, 160)",
    "juce::Colour(0, 255, 0)": "juce::Colour(103, 226, 195)",
}
for before, after in colours.items():
    if before not in text:
        raise RuntimeError(f"Scope colour missing: {before}")
    text = text.replace(before, after, 1)
scope.write_text(text, encoding="utf-8")
print("Prepared Source/Controls/KR106Scope.h")
