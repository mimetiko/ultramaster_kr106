// OpenSynth1 MPC Core - stage 1 of the OpenSynth1 engine port.
// Based on waveform equations and envelope mapping from letsdig/OpenSynth1 (MIT).
// MPC host adapter built against inmusic-software/mpc-plugin-builder-sdk (MIT).
// This is NOT a complete port; no imported .sy1 banks, arpeggiator or effects yet.
//
// Invariant: audio callback does not allocate, lock, access disk or log.
// Oscillators: exact base sin / modified-saw / variable pulse / phase-distorted
// triangle equations of OpenSynth1; polyBLEP edges; phase-mod FM and sync.
// Polyphony capped to eight voices for MPC One Cortex-A17 baseline.

#include "mpc_plugin.h"
#include "dsp/adsr.h"
#include "dsp/svf.h"
#include "dsp/voice.h"
#include "dsp/math.h"
#include "dsp/lfo.h"
#include "dsp/tempo.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <utility>
#include <algorithm>

using namespace mpc::dsp;

namespace {
constexpr float pi2 = 6.283185307179586f;
float bounded(float x, float a, float b) { return std::max(a, std::min(x, b)); }
float wrap(float x) { x -= std::floor(x); return x; }
float polyBlep(float t, float dt) {
    if (dt <= 0.f) return 0.f;
    if (t < dt) { t /= dt; return t+t-t*t-1.f; }
    if (t > 1.f-dt) { t=(t-1.f)/dt; return t*t+t+t+1.f; }
    return 0.f;
}
// Ported from OpenSynth1 Source/PluginProcessor.cpp getOsc1Sample().
// Osc 2 uses the same 3 core waveforms; the white-noise case is local.
float oscSample(float phase, float dt, int shape, float pw, uint32_t& rng) {
    switch (shape) {
    case 0: return std::sin(phase * pi2); // sine
    case 1: { // saw, with OpenSynth1 color-dependent phase distortion
        float fraction = pw > 0.5f ? 1.f - (pw-0.5f)*1.8f : 1.f;
        float safePw = bounded(fraction, 0.05f, 1.f);
        float v = phase < safePw ? 2.f*(phase/safePw)-1.f : -1.f;
        v -= polyBlep(phase,dt);
        if (safePw < 1.f) v += polyBlep(wrap(phase+1.f-safePw),dt);
        return v;
    }
    case 2: { // pulse, polyBLEP at both discontinuities
        float v = phase < pw ? 1.f : -1.f;
        return v + polyBlep(phase,dt) - polyBlep(wrap(phase+1.f-pw),dt);
    }
    case 3: { // triangle with original OpenSynth1 spike morph
        float safePw = bounded(pw,0.01f,0.99f);
        return phase < safePw ? 2.f*(phase/safePw)-1.f
                              : 1.f-2.f*((phase-safePw)/(1.f-safePw));
    }
    case 4: // osc 2 white noise
        rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5;
        return float(rng & 0x00ffffffu)/8388608.f-1.f;
    default: return 0.f;
    }
}
// Original OpenSynth1 envelope scaling, Source/PluginProcessor.cpp.
// MIDI-like 0..127 -> 5ms..1s..6s, with 0 mapped to 5ms for stability.
float envMs(float p) {
    float n = bounded(p,0.f,127.f)/127.f;
    if (n<0.001f) return 5.f;
    return n<=0.5f ? 5.f*std::pow(200.f, n/0.5f)
                   : 1000.f*std::pow(6.f,(n-0.5f)/0.5f);
}
float susLevel(float p) {
    float n = bounded(p,0.f,127.f)/127.f;
    return std::pow(n,1.5f);
}
}

class OpenSynth1MPC : public mpc::Plugin {
public:
    enum P {
        Osc1, Osc2, Mix, Color, Semi2, Fine2, SubLevel, SubOct,
        FM, Ring, Sync, Master,
        Cutoff, Reson, FMode, FEnvAmt, FAttack, FDecay, FSustain, FRelease,
        AAttack, ADecay, ASustain, ARelease, Velocity, Bend,
        L1Shape,L1Rate,L1Sync,L1Div,L1Pitch,L1Filter,
        L2Shape,L2Rate,L2Sync,L2Div,L2PWM,L2Amp,
        ChorusRate,ChorusDepth,ChorusMix,DelayTime,DelayFB,DelayMix,Drive,
        NumParams
    };

    static mpc::PluginInfo makeInfo() {
        mpc::PluginInfo i;
        i.name="Synth 1";
        i.vendor="Mimetiko";
        i.uniqueId=MPC_FOURCC('M','O','S','1');
        i.version=2000;
        i.isInstrument=true;
        i.numInputs=0; i.numOutputs=2;
        return i;
    }
    OpenSynth1MPC():mpc::Plugin(makeInfo()) {
        using namespace mpc;
        addParam(choiceParam("osc1","Osc 1",{"Sine","Saw","Pulse","Triangle"},1));
        addParam(choiceParam("osc2","Osc 2",{"Off","Saw","Pulse","Triangle","Noise"},1));
        addParam(floatParam("mix","Osc Mix",0,100,40,"%",0));
        addParam(floatParam("color","Color/PW",5,95,50,"%",0));
        addParam(intParam("semi2","Osc2 Semi",-24,24,0,"st"));
        addParam(intParam("fine2","Osc2 Fine",-64,63,0,"ct"));
        addParam(floatParam("sub","Sub Level",0,100,0,"%",0));
        addParam(choiceParam("suboct","Sub Oct",{"-1","-2"},0));
        addParam(floatParam("fm","FM Depth",0,100,0,"%",0));
        addParam(boolParam("ring","Ring Mod",false));
        addParam(boolParam("sync","Osc Sync",false));
        addParam(floatParam("master","Master",-36,0,-12,"dB",1));
        addParam(logParam("cutoff","Cutoff",25,16000,3000,"Hz",0));
        addParam(floatParam("reson","Resonance",0,100,15,"%",0));
        addParam(choiceParam("fmode","Filter Mode",{"LP","BP","HP"},0));
        addParam(floatParam("fenv","Filter Env",-100,100,45,"%",0));
        addParam(intParam("fatk","F Attack",0,127,0));
        addParam(intParam("fdec","F Decay",0,127,40));
        addParam(intParam("fsus","F Sustain",0,127,18));
        addParam(intParam("frel","F Release",0,127,34));
        addParam(intParam("aatk","A Attack",0,127,0));
        addParam(intParam("adec","A Decay",0,127,33));
        addParam(intParam("asus","A Sustain",0,127,105));
        addParam(intParam("arel","A Release",0,127,31));
        addParam(floatParam("vel","Velocity",0,100,60,"%",0));
        addParam(intParam("bend","Bend Range",0,24,2,"st"));
        // Modulation, with independent tempo-syncable LFOs.
        addParam(choiceParam("l1shape","LFO1 Wave",{"Sine","Triangle","Saw","Square","S&H"},0));
        addParam(logParam("l1rate","LFO1 Rate",0.05f,30.f,3.f,"Hz",2));
        addParam(boolParam("l1sync","LFO1 Sync",false));
        addParam(choiceParam("l1div","LFO1 Div",divisionNames(),kDivisionQuarter));
        addParam(floatParam("l1pitch","LFO1 Pitch",0.f,12.f,0.f,"st",2));
        addParam(floatParam("l1filt","LFO1 Filter",-100.f,100.f,0.f,"%",0));
        addParam(choiceParam("l2shape","LFO2 Wave",{"Sine","Triangle","Saw","Square","S&H"},0));
        addParam(logParam("l2rate","LFO2 Rate",0.05f,30.f,.7f,"Hz",2));
        addParam(boolParam("l2sync","LFO2 Sync",false));
        addParam(choiceParam("l2div","LFO2 Div",divisionNames(),kDivisionQuarter));
        addParam(floatParam("l2pwm","LFO2 PWM",0.f,100.f,0.f,"%",0));
        addParam(floatParam("l2amp","LFO2 Amp",0.f,100.f,0.f,"%",0));
        // Lightweight stereo effects and master saturation.
        addParam(logParam("chorate","Chorus Rate",0.05f,6.f,.35f,"Hz",2));
        addParam(floatParam("chodepth","Chorus Depth",0.f,100.f,35.f,"%",0));
        addParam(floatParam("chomix","Chorus Mix",0.f,70.f,0.f,"%",0));
        addParam(logParam("dtime","Delay Time",20.f,1200.f,360.f,"ms",0));
        addParam(floatParam("dfb","Delay FB",0.f,85.f,28.f,"%",0));
        addParam(floatParam("dmix","Delay Mix",0.f,70.f,0.f,"%",0));
        addParam(floatParam("drive","Drive",0.f,100.f,0.f,"%",0));

        auto preset=[this](const char* name,std::initializer_list<std::pair<int,float>> overrides) {
            std::vector<float> v((size_t)numParams());
            for(int i=0;i<numParams();++i) v[(size_t)i]=param(i).def;
            for(auto o:overrides) v[(size_t)o.first]=o.second;
            addProgram(name,std::move(v));
        };
        preset("Init Saw",{});
        preset("Dry Bass",{{Osc1,2},{Osc2,1},{Semi2,-12},{Mix,35},{Cutoff,450},{Reson,27},{FEnvAmt,65},{FDecay,28},{FSustain,0},{ADecay,28},{ASustain,85}});
        preset("Slow Cloud",{{Osc1,3},{Osc2,2},{Mix,50},{Fine2,11},{Cutoff,1500},{FEnvAmt,25},{FAttack,70},{FRelease,75},{AAttack,65},{ARelease,83}});
        preset("Ring Rust",{{Osc1,1},{Osc2,1},{Semi2,7},{Ring,1},{Cutoff,2900},{Reson,57},{FEnvAmt,-30},{ADecay,22}});
        preset("Phase Glass",{{Osc1,0},{Osc2,1},{FM,18},{Color,60},{Cutoff,7500},{Reson,15},{AAttack,6},{ARelease,45}});
        preset("Pulsing Drift",{{Osc1,2},{Osc2,2},{Color,38},{Cutoff,1650},{Reson,30},{L1Rate,1.2f},{L1Filter,38},{L2Rate,.17f},{L2PWM,70},{ChorusMix,24}});
        preset("Broken Glass",{{Osc1,1},{Osc2,1},{FM,27},{Semi2,9},{Cutoff,6000},{L1Rate,5.2f},{L1Pitch,.15f},{Drive,22},{DelayMix,18}});
        preset("Dust Echo",{{Osc1,3},{Osc2,1},{Fine2,8},{AAttack,45},{ARelease,78},{Cutoff,950},{L1Filter,21},{ChorusMix,27},{DelayMix,26}});
    }

    void prepare(double sampleRate, int) override {
        sr_=sampleRate > 1000.0 ? (float)sampleRate : 44100.f;
        for(auto& v:voices_) {
            v.filter.setSampleRate(sampleRate);
            v.amp.setSampleRate(sampleRate);
            v.filt.setSampleRate(sampleRate);
        }
    }
    void reset() override {
        alloc_.reset();
        for(auto& v:voices_) { v.amp.reset();v.filt.reset();v.filter.reset();v.note=-1;v.p1=0;v.p2=0;v.ps=0;v.ctrl=0; }
    }
    void process(mpc::ProcessContext& ctx) override {
        updateParameters();
        int pos=0;
        for(int i=0;i<ctx.numMidiEvents;++i) {
            const auto& ev=ctx.midi[i];
            int at=std::max(pos,std::min(ctx.numFrames,ev.sampleOffset));
            render(ctx,pos,at);
            pos=at;
            midi(ev);
        }
        render(ctx,pos,ctx.numFrames);
        float* l=ctx.outputs[0];float* r=ctx.numOutputs>1?ctx.outputs[1]:nullptr;
        float master=std::pow(10.f,s_.master/20.f);
        for(int i=0;i<ctx.numFrames;++i) {
            l[i]=bounded(l[i]*master,-1.f,1.f);
            if(r) r[i]=l[i];
        }
    }

private:
    struct Voice {
        ADSR amp,filt;SVF filter;
        float p1=0,p2=0,ps=0;
        uint32_t rng=0x34abcdefu;
        int note=-1,ctrl=0;
        float velocity=1.f;
    };
    struct Settings {
        int osc1=1,osc2=1,semi2=0,fine2=0,suboct=0,fmode=0,bend=2;
        bool ring=false,sync=false;
        float mix=.4f,pw=.5f,sub=0,fm=0,master=-12,cutoff=3000,reso=.15f,fenv=.45f,vel=.6f;
    } s_;
    static constexpr int kVoices=8;
    Voice voices_[kVoices];
    VoiceAllocator<kVoices> alloc_;
    float sr_=44100.f;

    void updateParameters() {
        s_.osc1=getInt(Osc1);s_.osc2=getInt(Osc2);s_.semi2=getInt(Semi2);s_.fine2=getInt(Fine2);
        s_.suboct=getInt(SubOct);s_.fmode=getInt(FMode);
        s_.ring=getBool(Ring);s_.sync=getBool(Sync);s_.bend=getInt(Bend);
        s_.mix=get(Mix)*.01f;s_.pw=bounded(get(Color)*.01f,.05f,.95f);
        s_.sub=get(SubLevel)*.01f;s_.fm=get(FM)*.01f;
        s_.master=get(Master);s_.cutoff=get(Cutoff);
        s_.reso=get(Reson)*.01f;s_.fenv=get(FEnvAmt)*.01f;s_.vel=get(Velocity)*.01f;
        alloc_.setBendRange((float)s_.bend);
        float fa=envMs(get(FAttack)),fd=envMs(get(FDecay)),fs=susLevel(get(FSustain)),fr=envMs(get(FRelease));
        float aa=envMs(get(AAttack)),ad=envMs(get(ADecay)),as=susLevel(get(ASustain)),ar=envMs(get(ARelease));
        for(auto& v:voices_) {
            v.filt.set(fa,fd,fs,fr);
            v.amp.set(aa,ad,as,ar);
        }
    }
    void release(int n) { voices_[n].amp.noteOff();voices_[n].filt.noteOff(); }
    void midi(const mpc::MidiEvent& ev) {
        if(ev.isNoteOn()){
            int ix=alloc_.noteOn(ev.note(),ev.velocity());
            Voice& v=voices_[ix];v.note=ev.note();
            v.velocity=1.f-s_.vel+s_.vel*(ev.velocity()/127.f);
            v.p1=v.p2=v.ps=0.f;v.ctrl=0;
            v.filter.reset();v.amp.noteOn();v.filt.noteOn();
        } else if(ev.isNoteOff()){
            int ix=alloc_.noteOff(ev.note());if(ix>=0)release(ix);
        } else if(ev.isAllNotesOff()){
            alloc_.allNotesOff([this](int ix){release(ix);});
        } else if(ev.isController() && ev.controller()==64){
            alloc_.setSustain(ev.controllerValue()>=64,[this](int ix){release(ix);});
        } else if(ev.isPitchBend()) alloc_.setPitchBend(ev.pitchBend());
    }
    void render(mpc::ProcessContext& ctx,int from,int to) {
        float* out=ctx.outputs[0];
        for(int i=from;i<to;++i){
            float sum=0.f;
            for(int ix=0;ix<kVoices;++ix){
                Voice& v=voices_[ix];if(!v.amp.isActive())continue;
                float fa=v.filt.process(),aa=v.amp.process();
                float base=alloc_.noteToHz((float)v.note);
                float d1=bounded(base/sr_,0.f,.45f);
                float d2=bounded((base*std::pow(2.f,(s_.semi2+s_.fine2*.01f)/12.f))/sr_,0.f,.45f);
                float ds=bounded(base/(sr_*(s_.suboct==0?2.f:4.f)),0.f,.45f);
                // OpenSynth1-style oscillator-2 phase modulation of oscillator-1.
                float osc2=s_.osc2==0?0.f:oscSample(v.p2,d2,s_.osc2,s_.pw,v.rng);
                float phase=wrap(v.p1+osc2*(s_.fm*s_.fm*3.5f));
                float osc1=oscSample(phase,d1,s_.osc1,s_.pw,v.rng);
                float sub=oscSample(v.ps,ds,2,.5f,v.rng);
                float mixed=s_.ring ? osc1*osc2 : osc1*(1.f-s_.mix)+osc2*s_.mix;
                mixed += sub*s_.sub*.45f;
                if(++v.ctrl>=16) {
                    v.ctrl=0;
                    float oct=s_.fenv*fa*7.f;
                    v.filter.set(bounded(s_.cutoff*std::pow(2.f,oct),25.f,sr_*.44f),s_.reso);
                }
                sum+=v.filter.process(mixed,s_.fmode)*aa*v.velocity*.24f;
                bool wrapped=v.p1+d1>=1.f;
                v.p1=wrap(v.p1+d1);
                v.p2=s_.sync&&wrapped?0.f:wrap(v.p2+d2);
                v.ps=wrap(v.ps+ds);
                if(!v.amp.isActive())alloc_.voiceFinished(ix);
            }
            out[i]+=sum;
        }
    }
};
MPC_PLUGIN_ENTRY(OpenSynth1MPC)
