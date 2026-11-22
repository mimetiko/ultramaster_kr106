#include "PluginEditor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

// Mimetiko Synth 1 / Concept A
// The red anodised hardware faceplate is rendered as a cached raster skin.
// All moving controls are JUCE components attached to the original APVTS
// parameter IDs; no synthesis, preset format or DSP code is modified.
namespace {
constexpr int W=1640, H=920;
using Colour=juce::Colour;
const Colour kRed(0xffab2531), kRedBright(0xffec4550), kDark(0xff14171b);
const Colour kPale(0xffe9e5dd), kMuted(0xffacadae), kPanel(0xff292a2d);
using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
using ChoiceAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;

enum class Kind { Knob, Fader, Choice, Toggle };
enum class Page { Main, Chorus, Delay, Multi, Eq, Advanced };
struct Spec {
    const char* id;
    const char* label;
    Kind kind;
    Page page;
    int x,y,w,h;
};
const std::vector<Spec> specs = {
  {"0", "WAVE", Kind::Choice, Page::Main, 68,234,112,28},
  {"45", "FM", Kind::Knob, Page::Main, 215,212,60,60},
  {"76", "DETUNE", Kind::Knob, Page::Main, 322,212,60,60},
  {"95", "SUB LEVEL", Kind::Knob, Page::Main, 212,292,64,64},
  {"96", "SUB WAVE", Kind::Choice, Page::Main, 68,319,112,28},
  {"97", "SUB OCT", Kind::Choice, Page::Main, 313,319,110,28},
  {"1", "WAVE", Kind::Choice, Page::Main, 68,449,112,28},
  {"2", "PITCH", Kind::Knob, Page::Main, 215,430,60,60},
  {"3", "FINE", Kind::Knob, Page::Main, 322,430,60,60},
  {"4", "TRACK", Kind::Toggle, Page::Main, 51,534,74,24},
  {"6", "SYNC", Kind::Toggle, Page::Main, 129,534,69,24},
  {"7", "RING", Kind::Toggle, Page::Main, 203,534,69,24},
  {"108", "PITCH ON", Kind::Toggle, Page::Main, 277,534,83,24},
  {"109", "FINE ON", Kind::Toggle, Page::Main, 364,534,85,24},
  {"5", "MIX", Kind::Knob, Page::Main, 512,214,60,60},
  {"8", "PULSE WIDTH", Kind::Knob, Page::Main, 512,302,60,60},
  {"91", "PHASE", Kind::Knob, Page::Main, 512,394,60,60},
  {"9", "KEY SHIFT", Kind::Knob, Page::Main, 512,484,60,60},
  {"14", "FILTER TYPE", Kind::Choice, Page::Main, 675,226,242,28},
  {"19", "CUTOFF", Kind::Knob, Page::Main, 660,286,120,120},
  {"20", "RESONANCE", Kind::Knob, Page::Main, 836,298,85,85},
  {"21", "ENV AMOUNT", Kind::Knob, Page::Main, 852,464,66,66},
  {"22", "KEY TRACK", Kind::Knob, Page::Main, 763,464,66,66},
  {"23", "DRIVE", Kind::Knob, Page::Main, 674,464,66,66},
  {"24", "VELOCITY", Kind::Toggle, Page::Main, 775,555,120,24},
  {"15", "A", Kind::Fader, Page::Main, 992,249,48,257},
  {"16", "D", Kind::Fader, Page::Main, 1067,249,48,257},
  {"17", "S", Kind::Fader, Page::Main, 1142,249,48,257},
  {"18", "R", Kind::Fader, Page::Main, 1217,249,48,257},
  {"25", "A", Kind::Fader, Page::Main, 1304,249,48,257},
  {"26", "D", Kind::Fader, Page::Main, 1378,249,48,257},
  {"27", "S", Kind::Fader, Page::Main, 1452,249,48,257},
  {"28", "R", Kind::Fader, Page::Main, 1526,249,48,257},
  {"57", "ON", Kind::Toggle, Page::Main, 47,659,61,24},
  {"42", "WAVE", Kind::Choice, Page::Main, 119,655,110,28},
  {"41", "DEST", Kind::Choice, Page::Main, 241,655,112,28},
  {"43", "RATE", Kind::Knob, Page::Main, 364,637,56,56},
  {"44", "DEPTH", Kind::Knob, Page::Main, 436,637,56,56},
  {"67", "TEMPO SYNC", Kind::Toggle, Page::Main, 126,697,111,20},
  {"68", "KEY SYNC", Kind::Toggle, Page::Main, 245,697,104,20},
  {"58", "ON", Kind::Toggle, Page::Main, 556,659,61,24},
  {"47", "WAVE", Kind::Choice, Page::Main, 628,655,110,28},
  {"46", "DEST", Kind::Choice, Page::Main, 750,655,112,28},
  {"48", "RATE", Kind::Knob, Page::Main, 873,637,56,56},
  {"49", "DEPTH", Kind::Knob, Page::Main, 945,637,56,56},
  {"69", "TEMPO SYNC", Kind::Toggle, Page::Main, 635,697,111,20},
  {"70", "KEY SYNC", Kind::Toggle, Page::Main, 754,697,104,20},
  {"38", "MODE", Kind::Choice, Page::Main, 1075,655,101,28},
  {"94", "POLY", Kind::Knob, Page::Main, 1187,640,56,56},
  {"73", "UNISON", Kind::Toggle, Page::Main, 1256,660,70,23},
  {"93", "VOICES", Kind::Knob, Page::Main, 1340,640,56,56},
  {"75", "DETUNE", Kind::Knob, Page::Main, 1413,640,56,56},
  {"84", "SPREAD", Kind::Knob, Page::Main, 1488,640,56,56},
  {"59", "ON", Kind::Toggle, Page::Main, 55,801,66,26},
  {"31", "MODE", Kind::Choice, Page::Main, 132,801,114,28},
  {"32", "RANGE", Kind::Choice, Page::Main, 257,801,90,28},
  {"33", "RATE", Kind::Knob, Page::Main, 356,784,64,64},
  {"34", "GATE", Kind::Knob, Page::Main, 431,784,64,64},
  {"29", "AMP GAIN", Kind::Knob, Page::Main, 1276,790,66,66},
  {"30", "AMP VEL", Kind::Knob, Page::Main, 1366,790,66,66},
  {"107", "MASTER VOLUME", Kind::Knob, Page::Main, 1477,767,113,113},
  {"66", "ON", Kind::Toggle, Page::Chorus, 548,838,72,25},
  {"64", "TYPE", Kind::Choice, Page::Chorus, 634,834,124,27},
  {"52", "TIME", Kind::Knob, Page::Chorus, 771,815,57,57},
  {"53", "DEPTH", Kind::Knob, Page::Chorus, 855,815,57,57},
  {"54", "RATE", Kind::Knob, Page::Chorus, 939,815,57,57},
  {"55", "FEEDBACK", Kind::Knob, Page::Chorus, 1023,815,57,57},
  {"56", "LEVEL", Kind::Knob, Page::Chorus, 1110,815,57,57},
  {"65", "ON", Kind::Toggle, Page::Delay, 548,838,72,25},
  {"82", "TYPE", Kind::Choice, Page::Delay, 634,834,124,27},
  {"35", "TIME", Kind::Knob, Page::Delay, 771,815,57,57},
  {"83", "SPREAD", Kind::Knob, Page::Delay, 855,815,57,57},
  {"36", "FEEDBACK", Kind::Knob, Page::Delay, 939,815,57,57},
  {"98", "TONE", Kind::Knob, Page::Delay, 1023,815,57,57},
  {"37", "MIX", Kind::Knob, Page::Delay, 1110,815,57,57},
  {"77", "ON", Kind::Toggle, Page::Multi, 560,838,75,25},
  {"78", "TYPE", Kind::Choice, Page::Multi, 659,834,150,27},
  {"79", "CONTROL 1", Kind::Knob, Page::Multi, 836,811,63,63},
  {"80", "CONTROL 2", Kind::Knob, Page::Multi, 950,811,63,63},
  {"81", "LEVEL", Kind::Knob, Page::Multi, 1067,811,63,63},
  {"60", "TONE", Kind::Knob, Page::Eq, 644,815,60,60},
  {"61", "FREQ", Kind::Knob, Page::Eq, 751,815,60,60},
  {"62", "LEVEL", Kind::Knob, Page::Eq, 858,815,60,60},
  {"63", "Q", Kind::Knob, Page::Eq, 965,815,60,60},
  {"90", "PAN", Kind::Knob, Page::Eq, 1072,815,60,60},
  {"10", "ON", Kind::Toggle, Page::Advanced, 94,287,72,26},
  {"11", "AMOUNT", Kind::Knob, Page::Advanced, 183,257,82,82},
  {"12", "ATTACK", Kind::Knob, Page::Advanced, 296,257,82,82},
  {"13", "DECAY", Kind::Knob, Page::Advanced, 410,257,82,82},
  {"71", "DESTINATION", Kind::Choice, Page::Advanced, 164,398,236,31},
  {"72", "FINE TUNE", Kind::Knob, Page::Advanced, 657,288,98,98},
  {"85", "UNISON PITCH", Kind::Knob, Page::Advanced, 1100,265,85,85},
  {"92", "UNISON PHASE", Kind::Knob, Page::Advanced, 1215,265,85,85},
  {"40", "BEND RANGE", Kind::Knob, Page::Advanced, 1330,265,85,85},
  {"39", "GLIDE", Kind::Knob, Page::Advanced, 1455,265,85,85},
  {"74", "AUTO GLIDE", Kind::Toggle, Page::Advanced, 1273,397,135,27},
  {"86", "MIDI CC 1", Kind::Knob, Page::Advanced, 144,550,83,83},
  {"50", "SENS 1", Kind::Knob, Page::Advanced, 308,550,83,83},
  {"87", "ASSIGN 1", Kind::Knob, Page::Advanced, 472,550,83,83},
  {"88", "MIDI CC 2", Kind::Knob, Page::Advanced, 144,685,83,83},
  {"51", "SENS 2", Kind::Knob, Page::Advanced, 308,685,83,83},
  {"89", "ASSIGN 2", Kind::Knob, Page::Advanced, 472,685,83,83},
};

void setMetalGradient(juce::Graphics& g, Colour top, Colour bottom,
                      float x, float y, float height) {
    g.setGradientFill(juce::ColourGradient(top,x,y,bottom,x,y+height,false));
}
void drawScrew(juce::Graphics& g,float x,float y,float r=7.f) {
    g.setColour(Colour(0xff070809).withAlpha(.75f));
    g.fillEllipse(x-r-1,y-r+2,(r+1)*2,(r+1)*2);
    setMetalGradient(g,Colour(0xffa6a8aa),Colour(0xff303236),x,y-r,r*2);
    g.fillEllipse(x-r,y-r,2*r,2*r);
    g.setColour(Colour(0xff14181a));g.drawEllipse(x-r,y-r,2*r,2*r,1.f);
    g.drawLine(x-r*.43f,y+r*.43f,x+r*.43f,y-r*.43f,1.5f);
}
void drawPanel(juce::Graphics& g, int x,int y,int w,int h,const char* title) {
    g.setColour(Colour(0xff0a090b).withAlpha(.82f));
    g.fillRoundedRectangle(float(x+3),float(y+5),float(w),float(h),9.f);
    setMetalGradient(g,Colour(0xff454346),Colour(0xff18191c),float(x),float(y),float(h));
    g.fillRoundedRectangle(float(x),float(y),float(w),float(h),9.f);
    g.setColour(Colour(0xff090a0b));g.drawRoundedRectangle(float(x+2),float(y+2),float(w-4),float(h-4),8.f,2.0f);
    g.setColour(Colour(0xff9d3139).withAlpha(.7f));
    g.drawLine(float(x+17),float(y+32),float(x+w-17),float(y+32),1.2f);
    g.setColour(kRedBright);
    g.fillRoundedRectangle(float(x+16),float(y+13),3.5f,13.f,1.5f);
    g.setColour(kPale);
    g.setFont(juce::Font(juce::FontOptions(15.f).withStyle("Bold")));
    g.drawText(title,x+29,y+7,w-65,23,juce::Justification::centredLeft);
    drawScrew(g,float(x+10),float(y+11),4.1f);
    drawScrew(g,float(x+w-12),float(y+11),4.1f);
    drawScrew(g,float(x+10),float(y+h-11),4.1f);
    drawScrew(g,float(x+w-12),float(y+h-11),4.1f);
}
void drawChassis(juce::Graphics& g,bool details) {
    g.fillAll(Colour(0xff101015));
    g.setColour(Colour(0xff050405).withAlpha(.82f));
    g.fillRoundedRectangle(26,35,W-52,H-57,18.f);
    setMetalGradient(g,Colour(0xffdb3640),Colour(0xff751822),29.f,31.f,float(H-66));
    g.fillRoundedRectangle(29,29,W-58,H-65,17.f);
    // Micro-striations mimic brushed anodised aluminium rather than flat vectors.
    uint32_t seed=0x746d6531u;
    for(int i=0;i<2600;++i) {
        seed=seed*1664525u+1013904223u;
        const int x=37+int(seed%1560u);
        seed=seed*1664525u+1013904223u;
        const int y=39+int(seed%837u);
        const float a=float((seed>>16)&15u)/2200.f;
        g.setColour((i%3==0?juce::Colours::white:juce::Colours::black).withAlpha(a));
        g.drawHorizontalLine(y,float(x),float(std::min(1607,x+5+int((seed>>8)&85u))));
    }
    g.setColour(Colour(0xfff7777f).withAlpha(.7f));
    g.drawRoundedRectangle(31,30,W-62,H-68,16.f,1.f);
    g.setColour(Colour(0xff41131b));
    g.drawLine(38.f,145.f,1603.f,145.f,2.f);
    g.setColour(Colour(0xff9d9ea0));
    g.setFont(juce::Font(juce::FontOptions(36.f).withStyle("Bold")));
    g.drawText("Mimetiko Synth 1",79,64,460,50,juce::Justification::centredLeft);
    g.setColour(Colour(0xfff9cccc).withAlpha(.85f));
    g.drawLine(80.f,118.f,502.f,118.f,1.f);
    // Preset LCD glass and its recessed, anodised metal surround.
    g.setColour(Colour(0xff110709));g.fillRoundedRectangle(574,52,451,71,9.f);
    setMetalGradient(g,Colour(0xff320e13),Colour(0xff110f11),587,59,54);
    g.fillRoundedRectangle(584,59,429,57,6.f);
    g.setColour(Colour(0xffd56a72).withAlpha(.5f));
    g.drawRoundedRectangle(584,59,429,57,6.f,1.f);
    g.setColour(Colour(0xfff8abab));
    g.setFont(juce::Font(juce::FontOptions(10.f).withStyle("Bold")));
    g.drawText("PRESET",597,61,395,13,juce::Justification::centred);
    g.setColour(Colour(0xfff8d7d3));
    g.setFont(juce::Font(juce::FontOptions(12.f)));
    g.drawText("RIGHT-CLICK KNOB / SWITCH: MIDI LEARN",1060,122,495,20,juce::Justification::centredRight);
    drawScrew(g,53.f,52.f,8.f);drawScrew(g,float(W-53),52.f,8.f);
    drawScrew(g,53.f,float(H-55),8.f);drawScrew(g,float(W-53),float(H-55),8.f);

    if(!details) {
        drawPanel(g,36,162,421,209,"OSC 1");
        drawPanel(g,36,379,421,208,"OSC 2");
        drawPanel(g,466,162,142,425,"MIXER");
        drawPanel(g,617,162,352,425,"FILTER");
        drawPanel(g,978,162,304,425,"FILTER ENV");
        drawPanel(g,1291,162,313,425,"AMP ENV");
        drawPanel(g,36,599,503,125,"LFO 1");
        drawPanel(g,547,599,503,125,"LFO 2");
        drawPanel(g,1059,599,545,125,"VOICE / UNISON");
        drawPanel(g,36,734,485,157,"ARPEGGIATOR");
        drawPanel(g,529,734,704,157,"EFFECTS");
        drawPanel(g,1241,734,363,157,"MASTER");
        // Horizontal rulers keep the envelope banks reminiscent of the reference hardware.
        g.setColour(Colour(0xff8d8788).withAlpha(.18f));
        for(int y=258;y<=496;y+=23) {
            g.drawLine(1000.f,float(y),1263.f,float(y),.7f);
            g.drawLine(1309.f,float(y),1585.f,float(y),.7f);
        }
    } else {
        drawPanel(g,37,163,483,305,"MOD ENVELOPE");
        drawPanel(g,527,163,484,305,"GLOBAL TUNING");
        drawPanel(g,1018,163,584,305,"VOICE / UNISON DETAIL");
        drawPanel(g,37,479,974,409,"MIDI ROUTING");
        drawPanel(g,1018,479,584,409,"MIMETIKO / ABOUT");
        g.setColour(kPale);
        g.setFont(juce::Font(juce::FontOptions(19.f).withStyle("Bold")));
        g.drawText("Full synthesizer control, original OpenSynth1 DSP",1060,552,480,50,juce::Justification::centredLeft);
        g.setColour(kMuted);g.setFont(juce::Font(juce::FontOptions(15.f)));
        g.drawFittedText("This panel contains the less frequently used parameters.  The main faceplate is intentionally simple and closely follows Concept A.  All 102 upstream parameters remain accessible, automatable and recallable.",1060,616,470,130,juce::Justification::topLeft,7);
        g.drawText("No fictitious reverb or third oscillator is added.",1060,771,470,25,juce::Justification::centredLeft);
    }
}
class HardwareLook final : public juce::LookAndFeel_V4 {
public:
    HardwareLook() {
        setColour(juce::Label::textColourId,kPale);
        setColour(juce::ComboBox::backgroundColourId,Colour(0xff111215));
        setColour(juce::ComboBox::textColourId,kPale);
        setColour(juce::ComboBox::outlineColourId,Colour(0xff525458));
        setColour(juce::ComboBox::arrowColourId,kRedBright);
        setColour(juce::PopupMenu::backgroundColourId,Colour(0xff17171b));
        setColour(juce::PopupMenu::textColourId,kPale);
        setColour(juce::TextButton::buttonColourId,kDark);
        setColour(juce::TextButton::textColourOffId,kPale);
        setColour(juce::TextButton::textColourOnId,kPale);
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,float pos,float start,float end,juce::Slider&) override {
        const float r=std::min(width,height)*.38f;
        const float cx=x+width*.5f, cy=y+height*.5f;
        const float a=start+pos*(end-start)-juce::MathConstants<float>::halfPi;
        // 11 stamped scale marks around the anodised knob well.
        for(int i=0;i<11;++i) {
            const float t=start+(end-start)*float(i)/10.f-juce::MathConstants<float>::halfPi;
            g.setColour(Colour(0xffb7b4b2).withAlpha(.73f));
            g.drawLine(cx+std::cos(t)*r*1.21f,cy+std::sin(t)*r*1.21f,
                       cx+std::cos(t)*r*1.33f,cy+std::sin(t)*r*1.33f,.9f);
        }
        g.setColour(Colour(0xff070709).withAlpha(.85f));
        g.fillEllipse(cx-r-5,cy-r+4,2*r+10,2*r+10);
        g.setColour(Colour(0xff6a151c));
        g.fillEllipse(cx-r-2,cy-r-2,2*r+4,2*r+4);
        setMetalGradient(g,Colour(0xffc6c9cb),Colour(0xff292a2e),cx,cy-r,2*r);
        g.fillEllipse(cx-r,cy-r,2*r,2*r);
        g.setColour(Colour(0xff111215));g.drawEllipse(cx-r,cy-r,2*r,2*r,2.f);
        const float ri=r*.8f;
        setMetalGradient(g,Colour(0xfff3f2ed),Colour(0xff55575c),cx,cy-ri,ri*2);
        g.fillEllipse(cx-ri,cy-ri,2*ri,2*ri);
        g.setColour(Colour(0xff9ea0a2).withAlpha(.25f));
        for(int q=0;q<13;++q) {
            float yy=cy-ri+float(q)*2*ri/13.f;
            float dx=std::sqrt(std::max(0.f,ri*ri-(yy-cy)*(yy-cy)));
            g.drawHorizontalLine(juce::roundToInt(yy),cx-dx*.8f,cx+dx*.8f);
        }
        g.setColour(kRedBright);
        juce::Path arc;
        arc.addCentredArc(cx,cy,r+3,r+3,0,start,start+pos*(end-start),true);
        g.strokePath(arc,juce::PathStrokeType(std::max(1.5f,r*.07f)));
        g.setColour(Colour(0xffeae7e3));
        g.drawLine(cx+std::cos(a)*ri*.34f,cy+std::sin(a)*ri*.34f,
                   cx+std::cos(a)*ri*.91f,cy+std::sin(a)*ri*.91f,
                   std::max(1.3f,r*.07f));
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float sliderPos,
                          float,float,juce::Slider::SliderStyle style,juce::Slider&) override {
        if(style!=juce::Slider::LinearVertical) return;
        const float cx=x+width*.5f;
        g.setColour(Colour(0xff0b0c0e));g.fillRoundedRectangle(cx-4,float(y+8),8.f,float(height-16),3.f);
        g.setColour(Colour(0xff8b2d32));g.fillRoundedRectangle(cx-2,sliderPos,4.f,float(y+height-11)-sliderPos,2.f);
        for(int i=0;i<15;++i) {
            const float yy=y+9+float(i)*(height-18)/14.f;
            g.setColour(Colour(0xffb4b5b6).withAlpha(.5f));
            g.drawLine(cx-19,yy,cx-12,yy,1.f);
            g.drawLine(cx+12,yy,cx+19,yy,1.f);
        }
        const float capW=32.f,capH=18.f;
        g.setColour(Colour(0xff020304).withAlpha(.8f));
        g.fillRoundedRectangle(cx-capW*.5f+2,sliderPos-capH*.5f+3,capW,capH,2.f);
        setMetalGradient(g,Colour(0xfff3f3f2),Colour(0xff68696c),cx,sliderPos-capH*.5f,capH);
        g.fillRoundedRectangle(cx-capW*.5f,sliderPos-capH*.5f,capW,capH,2.f);
        g.setColour(Colour(0xff28292b));g.drawRoundedRectangle(cx-capW*.5f,sliderPos-capH*.5f,capW,capH,2.f,1.f);
        g.setColour(Colour(0xff444446));g.drawLine(cx-capW*.43f,sliderPos,cx+capW*.43f,sliderPos,1.4f);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,const Colour&,bool hovered,bool down) override {
        auto r=button.getLocalBounds().toFloat().reduced(1.f);
        g.setColour(Colour(0xff060709));g.fillRoundedRectangle(r.translated(0,2),4.f);
        const bool on=button.getToggleState();
        setMetalGradient(g,on?Colour(0xffbd2630):hovered?Colour(0xff5d5c61):Colour(0xff434449),
                            on?Colour(0xff651a22):Colour(0xff1a1b20),r.getX(),r.getY(),r.getHeight());
        g.fillRoundedRectangle(r,4.f);
        g.setColour(on?kRedBright:Colour(0xff77777a));g.drawRoundedRectangle(r,4.f,on?1.8f:1.f);
        if(down) {g.setColour(Colour(0xff090909).withAlpha(.2f));g.fillRoundedRectangle(r,4.f);}
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& button,bool,bool) override {
        auto r=button.getLocalBounds().toFloat().reduced(1);
        g.setColour(Colour(0xff0b0c10));g.fillRoundedRectangle(r,3);
        g.setColour(button.getToggleState()?kRedBright:Colour(0xff45484a));
        g.fillEllipse(r.getX()+7,r.getCentreY()-4,8,8);
        if(button.getToggleState()) {
            g.setColour(kRedBright.withAlpha(.2f));
            g.fillEllipse(r.getX()+3,r.getCentreY()-8,16,16);
        }
        g.setColour(kPale);g.setFont(juce::Font(juce::FontOptions(10.f).withStyle("Bold")));
        g.drawFittedText(button.getButtonText(),r.withTrimmedLeft(20).toNearestInt(),
                         juce::Justification::centredLeft,1);
    }
};
class LearnSlider final : public juce::Slider {
public:
    std::function<void()> learn;
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.mods.isRightButtonDown()) {if(learn)learn(); return;}
        juce::Slider::mouseDown(e);
    }
};
class LearnChoice final : public juce::ComboBox {
public:
    std::function<void()> learn;
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.mods.isRightButtonDown()) {if(learn)learn(); return;}
        juce::ComboBox::mouseDown(e);
    }
};
class LearnToggle final : public juce::ToggleButton {
public:
    std::function<void()> learn;
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.mods.isRightButtonDown()) {if(learn)learn();return;}
        juce::ToggleButton::mouseDown(e);
    }
};
} // namespace

class MimetikoSynthCanvas final : public juce::Component {
public:
    explicit MimetikoSynthCanvas(OpenSynth1AudioProcessor& p) : processor(p) {
        setLookAndFeel(&look);
        juce::Image main(juce::Image::ARGB,W,H,true);
        {juce::Graphics g(main);drawChassis(g,false);}
        mainSkin=std::move(main);
        juce::Image detail(juce::Image::ARGB,W,H,true);
        {juce::Graphics g(detail);drawChassis(g,true);}
        detailSkin=std::move(detail);
        for(const auto& spec:specs) addControl(spec);
        makeHeaderButton(presetButton, "INITIAL SOUND", 608,79,367,29, [this]{showPresetMenu();});
        makeHeaderButton(prevButton, "<", 535,76,40,35, [this]{processor.selectPrevPreset();syncPreset();});
        makeHeaderButton(nextButton, ">", 981,76,39,35, [this]{processor.selectNextPreset();syncPreset();});
        makeHeaderButton(bankButton, "BANK", 1060,75,80,32, [this]{showBankMenu();});
        makeHeaderButton(loadButton, "LOAD", 1147,75,70,32, [this]{chooseFile(false);});
        makeHeaderButton(saveButton, "SAVE", 1223,75,70,32, [this]{chooseFile(true);});
        makeHeaderButton(detailButton, "DETAILS", 1300,75,96,32, [this]{advanced=!advanced;updateVisibility();});
        makeHeaderButton(sizeButton, "SIZE 75%", 1403,75,106,32, [this]{showSizeMenu();});
        makeHeaderButton(initButton, "INIT", 1514,75,64,32, [this]{processor.resetToInitPreset();syncPreset();});
        const char* names[]={"CHORUS","DELAY","MULTI FX","EQ"};
        for(int i=0;i<4;++i) {
            auto b=std::make_unique<juce::TextButton>();
            b->setButtonText(names[i]);
            b->setBounds(550+i*126,768,118,27);
            b->onClick=[this,i]{activeFx=static_cast<Page>(int(Page::Chorus)+i);updateVisibility();};
            addAndMakeVisible(*b);
            fxButtons.push_back(std::move(b));
        }
        updateVisibility();
        syncPreset();
    }
    ~MimetikoSynthCanvas() override {setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g) override {g.drawImageAt(advanced?detailSkin:mainSkin,0,0);}
    void resized() override {}
    void syncPreset() {
        presetButton.setButtonText(juce::String::formatted("%03d  /  ",processor.getCurrentProgramNumber())+
                                   processor.getCurrentProgramName().toUpperCase());
        bankButton.setTooltip(processor.getCurrentBankName());
    }
    void syncStatus() {
        if(processor.isMidiLearnActive())
            detailButton.setTooltip("MIDI Learn active for parameter "+juce::String(processor.getMidiLearnTargetParam()));
        else detailButton.setTooltip("Advanced synth and MIDI parameters");
    }
    void setZoomLabel(float zoom) {sizeButton.setButtonText("SIZE "+juce::String(juce::roundToInt(zoom*100.f))+"%");}
private:
    struct Visibility {juce::Component* component;Page page;};
    void track(juce::Component& c,Page page) {visible.push_back({&c,page});addAndMakeVisible(c);}
    void makeHeaderButton(juce::TextButton& b,const juce::String& text,int x,int y,int w,int h,
                          std::function<void()> fn) {
        b.setButtonText(text);b.setBounds(x,y,w,h);b.onClick=std::move(fn);addAndMakeVisible(b);
    }
    void addControl(const Spec& spec) {
        const juce::String id=spec.id;
        auto* parameter=processor.apvts.getParameter(id);
        if(parameter==nullptr) {jassertfalse;return;}
        const int midiId=id.getIntValue();
        const auto learn=[this,midiId]{processor.startMidiLearn(midiId);};
        if(spec.kind==Kind::Toggle) {
            auto b=std::make_unique<LearnToggle>();
            b->setButtonText(spec.label);b->learn=learn;
            b->setBounds(spec.x,spec.y,spec.w,spec.h);
            b->setTooltip("Right-click to MIDI learn: "+juce::String(spec.label));
            track(*b,spec.page);
            buttonAttachments.push_back(std::make_unique<ButtonAttachment>(processor.apvts,id,*b));
            controls.push_back(std::move(b));
            return;
        }
        auto label=std::make_unique<juce::Label>();
        label->setText(spec.label,juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::Font(juce::FontOptions(spec.kind==Kind::Fader?13.f:10.5f).withStyle("Bold")));
        label->setColour(juce::Label::textColourId,Colour(0xffe4e2dc));
        label->setInterceptsMouseClicks(false,false);
        if(spec.kind==Kind::Choice) {
            auto combo=std::make_unique<LearnChoice>();
            combo->learn=learn;
            if(auto* choice=dynamic_cast<juce::AudioParameterChoice*>(parameter)) {
                int idx=1;
                for(const auto& name:choice->choices)combo->addItem(name,idx++);
            } else {jassertfalse;return;}
            combo->setBounds(spec.x,spec.y,spec.w,spec.h);
            combo->setTooltip("Right-click to MIDI learn: "+juce::String(spec.label));
            label->setBounds(spec.x,spec.y-20,spec.w,18);
            track(*combo,spec.page);
            choiceAttachments.push_back(std::make_unique<ChoiceAttachment>(processor.apvts,id,*combo));
            controls.push_back(std::move(combo));
        } else {
            auto slider=std::make_unique<LearnSlider>();
            slider->learn=learn;
            slider->setSliderStyle(spec.kind==Kind::Fader?
                           juce::Slider::LinearVertical:juce::Slider::RotaryHorizontalVerticalDrag);
            slider->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            slider->setPopupDisplayEnabled(true,true,this);
            slider->setBounds(spec.x,spec.y,spec.w,spec.h);
            slider->setTooltip("Right-click to MIDI learn: "+juce::String(spec.label));
            if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter))
                slider->setDoubleClickReturnValue(true,ranged->convertFrom0to1(parameter->getDefaultValue()));
            label->setBounds(spec.x-(spec.w<60?4:10),spec.y+spec.h+2,spec.w+(spec.w<60?8:20),17);
            track(*slider,spec.page);
            sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.apvts,id,*slider));
            controls.push_back(std::move(slider));
        }
        track(*label,spec.page);
        controls.push_back(std::move(label));
    }
    void updateVisibility() {
        for(const auto& v:visible) {
            const bool show=advanced ? v.page==Page::Advanced :
                                        v.page==Page::Main || v.page==activeFx;
            v.component->setVisible(show);
        }
        for(int i=0;i<(int)fxButtons.size();++i) {
            fxButtons[i]->setVisible(!advanced);
            fxButtons[i]->setToggleState(!advanced&&activeFx==static_cast<Page>(int(Page::Chorus)+i),juce::dontSendNotification);
        }
        detailButton.setButtonText(advanced?"< BACK":"DETAILS");
        repaint();
    }
    void showSizeMenu() {
        const int sizes[]={65,75,85,100,115};
        juce::PopupMenu menu;
        for(int i=0;i<5;++i)menu.addItem(i+1,juce::String(sizes[i])+"%");
        auto safe=juce::Component::SafePointer<MimetikoSynthCanvas>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sizeButton),
          [safe](int selected) {
              if(!safe||selected<=0||selected>5)return;
              const int sizes[]={65,75,85,100,115};
              if(auto* parent=safe->getParentComponent())
                  parent->setSize(juce::roundToInt(W*sizes[selected-1]/100.f),
                                  juce::roundToInt(H*sizes[selected-1]/100.f));
          });
    }
    void showBankMenu() {
        juce::PopupMenu m;const auto& banks=processor.getBanks();
        for(int i=0;i<(int)banks.size();++i)
            m.addItem(i+1,juce::String::formatted("%02d  ",banks[i].bankNumber)+banks[i].name,
                      true,i==processor.getCurrentBankNumber());
        m.addSeparator();
        m.addItem(10000,"Select soundbank folder...");
        auto safe=juce::Component::SafePointer<MimetikoSynthCanvas>(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&bankButton),
          [safe](int n) {
              if(!safe||n<=0)return;
              if(n==10000) {safe->chooseBankFolder();return;}
              safe->processor.selectBankAndProgram(n-1,1);
              safe->syncPreset();
          });
    }
    void showPresetMenu() {
        const auto& banks=processor.getBanks();
        const int i=processor.getCurrentBankNumber();
        if(i<0||i>=(int)banks.size())return;
        juce::PopupMenu m;
        for(const auto& preset:banks[i].programs)
            m.addItem(preset.index,juce::String::formatted("%03d  ",preset.index)+preset.name,
                      true,preset.index==processor.getCurrentProgramNumber());
        auto safe=juce::Component::SafePointer<MimetikoSynthCanvas>(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetButton),
          [safe,i](int n){if(safe&&n>0) {safe->processor.selectBankAndProgram(i,n);safe->syncPreset();}});
    }
    void chooseBankFolder() {
        chooser=std::make_unique<juce::FileChooser>(
           "Select OpenSynth1 soundbank folder",processor.getBankDirectory());
        auto safe=juce::Component::SafePointer<MimetikoSynthCanvas>(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories,
            [safe](const juce::FileChooser& f) {
                if(!safe)return;
                const auto folder=f.getResult();
                if(!folder.isDirectory())return;
                safe->processor.setBankDirectory(folder);
                safe->processor.selectBankAndProgram(0,1);
                safe->syncPreset();
            });
    }
    void chooseFile(bool save) {
        chooser=std::make_unique<juce::FileChooser>(
           save?"Save Mimetiko Synth 1 preset":"Load Mimetiko Synth 1 preset",
           processor.getBankDirectory(),"*.sy1");
        auto safe=juce::Component::SafePointer<MimetikoSynthCanvas>(this);
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles |
             (save?juce::FileBrowserComponent::saveMode:juce::FileBrowserComponent::openMode),
            [safe,save](const juce::FileChooser& chooser) {
                if(!safe)return;
                auto f=chooser.getResult();
                if(f==juce::File{})return;
                if(save) {
                    if(!f.hasFileExtension(".sy1")) f=f.withFileExtension(".sy1");
                    safe->processor.saveSy1Preset(f,safe->processor.getCurrentProgramName(),
                                                    safe->processor.getCurrentColorName());
                } else safe->processor.loadSy1Preset(f);
                safe->syncPreset();
            });
    }
    OpenSynth1AudioProcessor& processor;
    HardwareLook look;
    juce::Image mainSkin,detailSkin;
    bool advanced=false;
    Page activeFx=Page::Chorus;
    std::vector<Visibility> visible;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ChoiceAttachment>> choiceAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<juce::TextButton>> fxButtons;
    juce::TextButton presetButton,prevButton,nextButton,bankButton,
                     loadButton,saveButton,detailButton,sizeButton,initButton;
    std::unique_ptr<juce::FileChooser> chooser;
};

OpenSynth1AudioProcessorEditor::OpenSynth1AudioProcessorEditor(OpenSynth1AudioProcessor& p)
: juce::AudioProcessorEditor(&p),processor(p),canvas(std::make_unique<MimetikoSynthCanvas>(p)) {
    addAndMakeVisible(*canvas);
    setResizable(true,true);
    setResizeLimits(1066,598,1886,1058);
    getConstrainer()->setFixedAspectRatio(double(W)/H);
    setSize(1230,690);
    startTimerHz(8);
}
OpenSynth1AudioProcessorEditor::~OpenSynth1AudioProcessorEditor() {stopTimer();canvas.reset();}
void OpenSynth1AudioProcessorEditor::paint(juce::Graphics& g) {g.fillAll(Colour(0xff111115));}
void OpenSynth1AudioProcessorEditor::resized() {
    const float scale=std::min(float(getWidth())/W,float(getHeight())/H);
    canvas->setBounds(0,0,W,H);
    canvas->setTransform(juce::AffineTransform::scale(scale));
    canvas->setZoomLabel(scale);
}
void OpenSynth1AudioProcessorEditor::timerCallback() {
    if(processor.consumePresetLoaded())canvas->syncPreset();
    canvas->syncStatus();
}
