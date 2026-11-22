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
    u,v=x-128,y-125
    rr=math.hypot(u,v)
    angle=math.atan2(v,u)
    if rr>119: return (0,0,0,0)
    if rr>110:
        shadow_r=math.hypot(u-3,v-5)
        alpha=0 if shadow_r>=119 else int(135*max(0,(119-shadow_r)/10))
        return (0,0,0,alpha)
    noise=hash2(x,y)
    light=.53-.29*(u-v)/220+.07*math.cos(angle+2.25)
    softbox=35*math.exp(-((u+45)**2/2400+(v+57)**2/1100))
    if rr>103:
        z=(rr-103)/7
        glow=max(0,1-abs(z-.19)/.24)*64
        base=67+light*85+glow-z*100
        rgb=(base+4,base+5,base+7)
    elif rr>92:
        knurl=12*math.sin(angle*116)+7*math.sin(angle*238+rr*2)
        base=30+light*87+knurl+softbox*.3
        rgb=(base,base+2,base+5)
    elif rr>87:
        ring=(rr-87)/5
        base=13+light*55+(1-ring)*43
        rgb=(base,base+2,base+4)
    else:
        radial=rr/87
        brushed=hash2(x//2,y,112)*6.5+math.sin(rr*2.8)*1.5
        highlight=46*math.exp(-((u+22)**2/3600+(v+35)**2/1400))
        base=25+light*62+softbox*.38+highlight+brushed-radial*6
        rgb=(base,base+2,base+5)
    return (clamp(rgb[0]+noise*2),clamp(rgb[1]+noise*2),clamp(rgb[2]+noise*2),255)

def fader(x: int,y: int) -> tuple[int,int,int,int]:
    u,v=x-100,y-52
    ax,ay=abs(u),abs(v)
    if ax>90 or ay>39:
        if ax<99 and ay<47:
            return (0,0,0,clamp(80*(1-max((ax-90)/9,(ay-39)/8))))
        return (0,0,0,0)
    d=min(90-ax,39-ay)
    shine=30*math.exp(-((v+21)**2)/190)
    bevel=72*math.exp(-d/3.7)
    top=35*math.exp(-((v+36)**2)/32)
    brushed=5*hash2(x,y,2)+3*math.sin(y*1.5)
    g=63+30*(-v/39)+shine+bevel+top+brushed
    if abs(v)<2.5 and ax<78: g*=.39
    if 5<abs(v)<27 and int(ax)%11<2: g+=13
    return (clamp(g),clamp(g+2),clamp(g+3),255)

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

if __name__=='__main__':
    main(Path(sys.argv[1]) if len(sys.argv)>1 else Path('resources'))
