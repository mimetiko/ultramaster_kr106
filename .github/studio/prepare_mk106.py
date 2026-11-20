#!/usr/bin/env python3
"""Apply the mimetiko MK-106 classic layout while keeping the KR106 DSP intact."""
from __future__ import annotations
import base64
import io
import lzma
import tarfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
parts = [Path(__file__).with_name(f"mk106_part{i}.txt").read_text(encoding="ascii") for i in (1,2,3)]
archive = lzma.decompress(base64.b64decode("".join(parts)))
expected = {"Source/PluginEditor.cpp", "Source/PluginEditor.h"}
with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as tf:
    if set(tf.getnames()) != expected:
        raise RuntimeError("Unexpected MK-106 editor source archive contents")
    for name in sorted(expected):
        fp = tf.extractfile(name)
        if fp is None: raise RuntimeError(f"Missing source: {name}")
        (root / name).write_bytes(fp.read())
        print("MK-106 editor:", name)

scope = root / "Source/Controls/KR106Scope.h"
src = scope.read_text(encoding="utf-8")
for before, after in [("traceStr(\"ULTRAMASTER\",","traceStr(\"MIMETIKO\","),
                      ("traceStr(\"KR-106\",","traceStr(\"MK-106\",")]:
    if before not in src: raise RuntimeError(f"Missing scope label: {before}")
    src = src.replace(before, after)
scope.write_text(src, encoding="utf-8")

cmake = root / "CMakeLists.txt"
src = cmake.read_text(encoding="utf-8")
for before, after in [
    ('COMPANY_NAME "Kayrock"', 'COMPANY_NAME "mimetiko"'),
    ('PLUGIN_DESCRIPTION "Juno-6/60/106 emulation v${CMAKE_PROJECT_VERSION}"',
     'PLUGIN_DESCRIPTION "MK-106 classic synthesizer by mimetiko v${CMAKE_PROJECT_VERSION}"'),
    ('"Ultramaster KR-106 (SysEx)"', '"MK-106 (SysEx)"'),
    ('"Ultramaster KR-106"', '"MK-106"')]:
    if before not in src: raise RuntimeError(f"Missing CMake name: {before}")
    src = src.replace(before, after)
cmake.write_text(src, encoding="utf-8")

header = root / "Source/PluginProcessor.h"
src = header.read_text(encoding="utf-8")
if 'return "KR-106";' not in src: raise RuntimeError("Missing original processor name")
header.write_text(src.replace('return "KR-106";','return "MK-106";'), encoding="utf-8")
print("MK-106 brand, plugin metadata, and processor name prepared")
