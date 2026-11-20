#!/usr/bin/env python3
"""Apply a deterministic visual-only MK-106 Classic v2 refinement."""
from pathlib import Path
import base64
import lzma

repo_root = Path(__file__).resolve().parents[2]
compressed = Path(__file__).with_name("refine_v2.py.xz.b64").read_text(encoding="ascii")
script = lzma.decompress(base64.b64decode(compressed)).decode("utf-8")
editor_script_path = repo_root / "Source" / "_mk106_refinement_v2.py"
# Setting __file__ to Source locates the editor without any other source edits.
exec(compile(script, str(editor_script_path), "exec"),
     {"__file__": str(editor_script_path), "__name__": "__main__"})
