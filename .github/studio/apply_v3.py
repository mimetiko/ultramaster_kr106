#!/usr/bin/env python3
"""Apply the compact two-page MK-106 v3 patch to the prepared v2 editor."""
import base64
import lzma
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = root / "Source"
payload = Path(__file__).with_name("mk106_v3_patch.py.xz.b64").read_text(encoding="ascii")
script = lzma.decompress(base64.b64decode(payload)).decode("utf-8")
exec(compile(script, str(source / "_mk106_v3_patch.py"), "exec"),
     {"__file__": str(source / "_mk106_v3_patch.py"), "__name__": "__main__"})
