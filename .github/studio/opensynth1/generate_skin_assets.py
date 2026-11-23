#!/usr/bin/env python3
"""Generate reproducible physically shaded Synth 1 skin textures (stdlib only)."""
from __future__ import annotations
import math
import struct
import sys
import zlib
from pathlib import Path

def png(path: Path, w: int, h: int, fn) -> None:
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.extend(fn(x, y))
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack('>I',len(data)) + tag + data + struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'
        +chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))
        +chunk(b'IDAT',zlib.compress(bytes(raw),9))
        +chunk(b'IEND',b''))
    print(path, path.stat().st_size, 'bytes')

def clamp(v: float) -> int:
    return int(max(0,min(255,round(v))))

def hash2(x: int,y: int,salt: int=0) -> float:
    n=(x*374761393+y*668265263+salt*1442695041)&0xffffffff
    n=(n^(n>>13))*1274126177&0xffffffff
    return ((n^(n>>16))&65535)/32767.5-1

def knob(x: int,y: int) -> tuple[int,int,int,int]:
    # Machined graphite-anodised aluminium. JUCE draws the moving pointer
    # separately so this raster body stays perfectly stable while turning.
    u,v=x-128.0,y-123.0
    r=math.hypot(u,v)
    a=math.atan2(v,u)
    if r>=126: return (0,0,0,0)
    if r>=112:
        shadow_r=math.hypot(u-3,v-5)
        return (0,0,0,clamp(165*max(0,1-(shadow_r-112)/14)))
    light=max(-1,min(1,(-.63*u-.77*v)/(r+.001)))
    fleck=hash2(x,y,31)*2.3+hash2(x//2,y,9)*1.5
    spec=51*math.exp(-((a+2.22)/.17)**2)
    if r>=108:
        n=18+76*max(0,(112-r)/4)+spec+fleck
    elif r>=99:
        ribs=10*math.sin(a*100)+6*math.cos(a*165)
        n=20+44*(.52+.48*light)+ribs+fleck
    elif r>=91:
        n=17+52*(.5+.5*light)+30*math.exp(-((r-96)/1.8)**2)+fleck
    elif r>=84:
        n=34+63*(.5+.5*light)+47*math.exp(-((r-86.1)/2.0)**2)+fleck
    else:
        broad=80*math.exp(-((u+34)/27)**2)*math.exp(-((v+16)/65)**2)
        top=35*math.exp(-((v+49)/18)**2)
        right=16*math.exp(-((u-33)/16)**2)*math.exp(-((v+33)/25)**2)
        turn=3.9*math.sin(r*2.4)+1.8*math.cos(r*6.3)
        n=34+27*(.5+.5*light)+broad+top+right+turn+fleck
    n=clamp(n)
    return (n,clamp(n+2),clamp(n+5),255)

def fader(x: int,y: int) -> tuple[int,int,int,int]:
    # A three-dimensional opaque aluminium fader handle. Only the external
    # cast shadow has alpha; the entire physical body is RGBA alpha=255.
    u,v=x-100.0,y-52.0
    ax,ay=abs(u),abs(v)
    if ax>=100 or ay>=52:return (0,0,0,0)
    if ax>94 or ay>43:
        t=max((ax-94)/6,(ay-43)/9)
        return (0,0,0,clamp(200*(1-t)))
    d=min(94-ax,43-ay)
    top=50*math.exp(-((v+35)/8)**2)
    bottom=-36*math.exp(-((v-33)/8)**2)
    metal=73+42*(-v/43)+30*math.exp(-d/3.5)+top+bottom+10*hash2(x,y,34)
    if abs(v)<3: metal=48+4*hash2(x,y)
    if 10<abs(v)<32 and int(ax)%10<1: metal+=12
    n=clamp(metal)
    return (n,clamp(n+1),clamp(n+4),255)

def dark_panel(x: int,y: int) -> tuple[int,int,int,int]:
    # Fine-grain charcoal powder coat, with no embossed panel-edge lighting.
    fine=hash2(x,y,71)
    brushed=hash2(x//5,y,31)
    broad=hash2(x//17,y//2,123)
    n=34+4.2*fine+1.7*brushed+1.2*broad
    return (clamp(n),clamp(n+1),clamp(n+2),255)

def grain(x: int,y: int) -> tuple[int,int,int,int]:
    noise=hash2(x,y,5)
    elongated=hash2(x//7,y,13)
    fine=hash2(x//51,y//2,26)
    n=.52*noise+.28*elongated+.20*fine
    a=clamp(abs(n)*47)
    return (255,245,234,a) if n>=0 else (6,4,10,a)

def main(root: Path) -> None:
    png(root/'synth1_knob.png',256,256,knob)
    png(root/'synth1_fader.png',200,108,fader)
    png(root/'synth1_grain.png',256,256,grain)
    png(root/'synth1_dark_panel.png',256,256,dark_panel)

if __name__=='__main__':
    main(Path(sys.argv[1]) if len(sys.argv)>1 else Path('resources'))
