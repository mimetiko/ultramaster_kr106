#!/usr/bin/env python3
"""MK-106 v4: deterministic satin patina and tactile faders, visual-only."""
from pathlib import Path
p=Path(__file__).resolve().parents[2]/"Source"/"PluginEditor.cpp"
s=p.read_text(encoding="utf-8")
def change(old,new):
    global s
    if s.count(old)!=1:
        raise RuntimeError(f"v4 anchor mismatch: {old[:90]!r} (found {s.count(old)})")
    s=s.replace(old,new,1)
change('} // namespace\n\nclass KR106Editor::ModernLookAndFeel',"""
// The grain uses a fixed pseudo-random sequence: no jitter or shimmer on repaint.
static void patina(juce::Graphics& g, juce::Rectangle<float> r, uint32_t seed,
                   int count, float opacity)
{
    uint32_t n = seed ^ 0x9e3779b9u;
    for (int i=0; i<count; ++i)
    {
        n ^= n << 13; n ^= n >> 17; n ^= n << 5;
        const float x = r.getX() + 2.f + float(n % 10000u) * (r.getWidth()-4.f)/10000.f;
        n ^= n << 13; n ^= n >> 17; n ^= n << 5;
        const float y = r.getY() + 2.f + float(n % 10000u) * (r.getHeight()-4.f)/10000.f;
        const bool bright = (n & 3u)==0u;
        g.setColour((bright ? C::fromRGB(219,214,204) : C::fromRGB(3,4,6))
                       .withAlpha(opacity*(bright ? 0.65f : 1.f)));
        g.drawLine(x,y,x+0.6f+float((n>>8)%8u)*0.13f,y,0.55f);
    }
}
static void edgeWear(juce::Graphics& g, juce::Rectangle<float> r, uint32_t seed)
{
    uint32_t n = seed ^ 0xa53c9e11u;
    for(int i=0;i<16;++i)
    {
        n = n*1664525u+1013904223u;
        const float x = r.getX()+5.f+float(n%1000u)/1000.f*(r.getWidth()-10.f);
        const float y = (n&1u) ? r.getY()+1.f : r.getBottom()-1.f;
        g.setColour(C::fromRGB(212,203,184).withAlpha(0.055f));
        g.drawLine(x,y,x+1.1f,y,0.55f);
    }
}
} // namespace

class KR106Editor::ModernLookAndFeel""")
change('''        g.setColour(C::fromRGB(10, 10, 12));
        g.drawLine(cx, trackTop + 4.f, cx, trackBottom - 4.f, 2.f);

        // 1980s-style''','''        g.setColour(C::fromRGB(10, 10, 12));
        g.drawLine(cx, trackTop + 4.f, cx, trackBottom - 4.f, 2.f);
        // Inner shadow and a glint on the left-hand machined rail.
        g.setColour(C::fromRGB(169,166,154).withAlpha(0.20f));
        g.drawLine(cx-3.2f,trackTop+3.f,cx-3.2f,trackBottom-3.f,0.65f);
        g.setColour(C::fromRGB(3,4,5).withAlpha(0.50f));
        g.drawLine(cx+3.2f,trackTop+3.f,cx+3.2f,trackBottom-3.f,0.85f);

        // 1980s-style''')
change('''        g.setColour(C::fromRGB(18, 19, 20));
        g.drawRoundedRectangle(cap, 3.3f, 1.1f);
        // Satin-metal edge, restrained face ribbing and top highlight.''','''        g.setColour(C::fromRGB(18, 19, 20));
        g.drawRoundedRectangle(cap, 3.3f, 1.1f);
        // Gently sculpted lip and sidewalls: physical relief without glare.
        g.setGradientFill(juce::ColourGradient(
             C::fromRGB(180,175,160).withAlpha(0.35f),cap.getX()+2.f,cap.getY()+2.f,
             C::fromRGB(45,46,49).withAlpha(0.08f),cap.getX()+2.f,cap.getY()+9.f,false));
        g.fillRoundedRectangle(cap.reduced(2.3f,2.f).withHeight(6.5f),1.3f);
        g.setColour(C::fromRGB(234,228,209).withAlpha(0.18f));
        g.drawLine(cap.getX()+2.f,cap.getY()+3.f,
                   cap.getX()+2.f,cap.getBottom()-3.f,0.65f);
        g.setColour(C::fromRGB(6,7,9).withAlpha(0.62f));
        g.drawLine(cap.getRight()-2.f,cap.getY()+3.f,
                   cap.getRight()-2.f,cap.getBottom()-3.f,0.9f);
        // Satin-metal edge, restrained face ribbing and top highlight.''')
change('''        for (int rib = -7; rib <= 8; rib += 5)
            g.drawLine(cap.getX() + 6.f, knobY + (float)rib,
                       cap.getRight() - 6.f, knobY + (float)rib, 0.6f);''','''        for (int rib = -7; rib <= 8; rib += 5)
        {
            g.setColour(C::fromRGB(11,12,13).withAlpha(0.42f));
            g.drawLine(cap.getX()+6.f,knobY+(float)rib,
                       cap.getRight()-6.f,knobY+(float)rib,0.75f);
            g.setColour(C::fromRGB(210,206,196).withAlpha(0.12f));
            g.drawLine(cap.getX()+6.f,knobY+(float)rib-0.9f,
                       cap.getRight()-6.f,knobY+(float)rib-0.9f,0.55f);
        }''')
change('''    g.fillRect(r);
    g.setColour(C::fromRGB(123, 120, 114).withAlpha(0.72f));''','''    g.fillRect(r);
    patina(g,r.reduced(3.f),uint32_t(r.getX()*17.f+r.getY()),72,0.09f);
    edgeWear(g,r,uint32_t(r.getX()*13.f));
    g.setColour(C::fromRGB(123, 120, 114).withAlpha(0.72f));''')
change('''    g.fillRect(7,8,kBaseWidth-14,kBaseHeight-15);
    g.setColour(C::fromRGB(17,17,18));''','''    g.fillRect(7,8,kBaseWidth-14,kBaseHeight-15);
    patina(g,{10.f,10.f,float(kBaseWidth)-20.f,float(kBaseHeight)-20.f},1064,390,0.09f);
    edgeWear(g,{8.f,8.f,float(kBaseWidth)-16.f,float(kBaseHeight)-16.f},106);
    g.setColour(C::fromRGB(17,17,18));''')
change('''        g.fillRect(juce::Rectangle<float>(x,y,w,h));
        g.setColour(C::fromRGB(115,116,116));''','''        g.fillRect(juce::Rectangle<float>(x,y,w,h));
        patina(g,{x+2.f,y+2.f,w-4.f,h-4.f},uint32_t(x*19),84,0.09f);
        g.setColour(C::fromRGB(115,116,116));''')
p.write_text(s,encoding="utf-8")
print("Applied v4 satin faceplate + three-dimensional slider details")
