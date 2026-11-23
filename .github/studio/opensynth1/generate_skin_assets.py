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
    # Dark, lathe-machined aluminium with a stepped outer rim.
    # The white pointer is painted live in JUCE, never baked in.
    u,v=x-128.0,y-126.0
    r=math.hypot(u,v)
    a=math.atan2(v,u)
    if r>=125: return (0,0,0,0)
    if r>111:
        shadow_r=math.hypot(u-2.8,v-4.0)
        alpha=clamp(170*max(0.0,(125-shadow_r)/13.0))
        return (1,1,2,alpha)
    grain=hash2(x,y,11)
    lx=(-.50*u-.83*v)/(r+.001)
    light=.48+.28*lx
    brushed=2.1*hash2(x//2,y,17)+1.4*hash2(x,y,31)
    if r>=106:
        metal=31+58*light+48*math.exp(-((r-108.2)/1.2)**2)+brushed
    elif r>=95:
        grooves=6*math.cos(114*a)+3*math.cos(226*a+.8)
        metal=24+52*light+grooves+brushed
        if r<97: metal+=13
    elif r>=89:
        metal=18+32*light+39*math.exp(-((r-90.1)/1.6)**2)+brushed
    else:
        # Circular machining grooves and diffuse softbox reflections.
        turning=2.4*math.sin(r*2.7)+1.0*math.sin(r*8.7)
        softbox=33*math.exp(-((u+34)**2/2900+(v+40)**2/1600))
        secondary=13*math.exp(-((u-34)**2/900+(v+28)**2/650))
        chamfer=14*math.exp(-((r-84.5)/4.5)**2)
        metal=26+46*light+softbox+secondary+chamfer+turning+brushed
    n=clamp(metal+grain*1.2)
    return (n,clamp(n+1),clamp(n+3),255)

def fader(x: int,y: int) -> tuple[int,int,int,int]:
    # Solid machined metal cap, fully opaque apart from its outer shadow.
    u,v=x-100.0,y-52.0
    ax,ay=abs(u),abs(v)
    if ax>=98 or ay>=49: return (0,0,0,0)
    if ax>90 or ay>38:
        fade=max((ax-90)/8,(ay-38)/11)
        return (0,0,0,clamp(168*(1-fade)))
    d=min(90-ax,38-ay)
    light=.35+.4*(-v/38)+.09*(-u/90)
    edge=57*math.exp(-d/3.1)
    top=22*math.exp(-((v+30)/7)**2)
    softbox=30*math.exp(-((u+41)/36)**2-((v+18)/20)**2)
    brushed=4.3*hash2(x,y,10)+2.0*math.sin(y*1.5)
    metal=58+44*light+edge+top+softbox+brushed
    if abs(v)<3 and ax<78: metal*=.43
    if 7<abs(v)<28 and int(ax)%13<1: metal+=8
    n=clamp(metal)
    return (n,clamp(n+1),clamp(n+3),255)

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
