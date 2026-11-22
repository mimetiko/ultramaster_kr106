#!/usr/bin/env python3
"""Apply our OpenSynth1 UI-only sources over a pinned MIT-licensed upstream checkout.

Keep audio/DSP and parameter identifiers from upstream unchanged.
Only files placed in overlay/Source are copied; until that folder exists,
this script leaves upstream OpenSynth1 untouched.
"""
from pathlib import Path
import shutil
import sys

if len(sys.argv) != 2:
    raise SystemExit("Usage: apply_editor_overlay.py <upstream-source-root>")

root = Path(sys.argv[1]).resolve()
overlay = Path(__file__).resolve().parent / "overlay" / "Source"
allowed = {"PluginEditor.cpp", "PluginEditor.h", "QuickOptionPicker.cpp", "QuickOptionPicker.h"}

if not (root / "Source" / "PluginProcessor.cpp").is_file():
    raise RuntimeError("Upstream OpenSynth1 source tree not found")

if not overlay.exists():
    print("OpenSynth1 UI lab: no custom editor yet; compiling unchanged upstream")
else:
    for source in sorted(overlay.iterdir()):
        if not source.is_file() or source.name not in allowed:
            raise RuntimeError(f"Unexpected editor overlay file: {source.name}")
        shutil.copy2(source, root / "Source" / source.name)
        print(f"OpenSynth1 UI lab: replaced Source/{source.name}")
