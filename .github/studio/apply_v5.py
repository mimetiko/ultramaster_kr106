#!/usr/bin/env python3
"""Apply physical-material v5 rendering to the generated JUCE editor."""
import base64, lzma
from pathlib import Path
root = Path(__file__).resolve().parents[2]
script = lzma.decompress(base64.b64decode(
    Path(__file__).with_name("used_hardware_v5.py.xz.b64").read_text(encoding="ascii")
)).decode("utf-8")
exec(compile(script, str(root / "Source" / "_v5_patch.py"), "exec"),
     {"__file__": str(Path(__file__).resolve()), "__name__": "__main__"})
