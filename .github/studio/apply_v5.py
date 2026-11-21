#!/usr/bin/env python3
"""Apply physical-material v5 rendering to the generated JUCE editor."""
import base64, lzma
from pathlib import Path
root = Path(__file__).resolve().parents[2]
script = lzma.decompress(base64.b64decode(
    Path(__file__).with_name("used_hardware_v5.py.xz.b64").read_text(encoding="ascii")
)).decode("utf-8")
# v4 CI source uses these stable helper names; normalize v5 anchors.
script = script.replace(
    "paintFaceplateGrain(g, { 10.f, 10.f, (float)kBaseWidth-20.f, (float)kBaseHeight-20.f },\n                       1064, 390, 0.095f);\n    paintSubtleEdgeWear(g, { 8.f, 8.f, (float)kBaseWidth-16.f, (float)kBaseHeight-16.f }, 106);",
    "patina(g,{10.f,10.f,float(kBaseWidth)-20.f,float(kBaseHeight)-20.f},1064,390,0.09f);\n    edgeWear(g,{8.f,8.f,float(kBaseWidth)-16.f,float(kBaseHeight)-16.f},106);")
script = script.replace(
    "paintFaceplateGrain(g, r.reduced(3.f), (int)(r.getX()*17.f + r.getY()), 74, 0.095f);\n    paintSubtleEdgeWear(g, r, (int)(r.getX()*13.f));",
    "patina(g,r.reduced(3.f),uint32_t(r.getX()*17.f+r.getY()),72,0.09f);\n    edgeWear(g,r,uint32_t(r.getX()*13.f));")
script = script.replace(
    "paintFaceplateGrain(g, {x+2.f,y+2.f,w-4.f,h-4.f}, (int)x*19, 90, 0.10f);",
    "patina(g,{x+2.f,y+2.f,w-4.f,h-4.f},uint32_t(x*19),84,0.09f);")
exec(compile(script, str(root / "Source" / "_v5_patch.py"), "exec"),
     {"__file__": str(Path(__file__).resolve()), "__name__": "__main__"})
