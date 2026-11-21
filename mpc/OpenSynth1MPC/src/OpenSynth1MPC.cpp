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
#include "Sy1Banks.h"
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
        ArpOn, ArpMode, ArpOct, ArpBeat, ArpGate,
        PlayMode, Porta, AutoPorta, KeyTrack, SubShape,
        OscModOn, OscModAmt, OscModAttack, OscModDecay, OscModDest,
        L1Dest,L2Dest, L1Depth,L2Depth,
        NumParams
    };

    static mpc::PluginInfo makeInfo() {
        mpc::PluginInfo i;
        i.name="Synth 1";
        i.vendor="Mimetiko";
        i.uniqueId=MPC_FOURCC('M','O','S','1');
        i.version=3000;
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
        // All new parameters are appended to keep v0.2 project state IDs stable.
        addParam(boolParam("arpon","Arp On",false));
        addParam(choiceParam("arpmode","Arp Mode",{"Up/Down","Up","Down","Random"},1));
        addParam(choiceParam("arpoct","Arp Oct",{"1","2","3","4"},0));
        addParam(choiceParam("arpbeat","Arp Beat",{"1/4","1/8","1/16","1/32","1/8T","1/16T"},2));
        addParam(intParam("arpgate","Arp Gate",1,100,60,"%"));
        addParam(choiceParam("playmode","Play Mode",{"Poly","Mono","Legato"},0));
        addParam(floatParam("porta","Portamento",0.f,1500.f,0.f,"ms",0));
        addParam(boolParam("autoporta","Auto Glide",false));
        addParam(intParam("keytrk","Key Track",0,100,50,"%"));
        addParam(choiceParam("subshape","Sub Wave",{"Sine","Triangle","Saw","Pulse"},3));
        addParam(boolParam("modon","Mod Env On",false));
        addParam(intParam("modamt","Mod Depth",0,127,64));
        addParam(intParam("modatk","Mod Attack",0,127,0));
        addParam(intParam("moddec","Mod Decay",0,127,0));
        addParam(choiceParam("moddest","Mod Dest",{"Osc2","FM","PWM"},0));
        addParam(choiceParam("l1dest","LFO1 Dest",{"Pitch","Osc2","Filter","Amp","PWM","FM"},2));
        addParam(choiceParam("l2dest","LFO2 Dest",{"Pitch","Osc2","Filter","Amp","PWM","FM"},4));
        addParam(intParam("l1depth","LFO1 Depth",0,127,0));
        addParam(intParam("l2depth","LFO2 Depth",0,127,0));

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
        // Import original Synth1 / OpenSynth1 text presets from
        // Content/SY1/<bank-folder>/*.sy1, before the host queries program count.
        // These are *approximate* mappings where the MPC DSP differs.
        loadSy1Programs();
    }


    // No filesystem access after the constructor; program values are cached
    // in mpc::Plugin and saved by MPC projects through the SDK's state chunks.
    void loadSy1Programs() {
        const auto imported=sy1::load(mpc::contentPath("SY1"),384);
        auto clampRaw=[](float x){return bounded(x,0.f,127.f);};
        for(const auto& patch:imported) {
            std::vector<float> v((size_t)numParams());
            for(int i=0;i<numParams();++i)v[(size_t)i]=param(i).def;
            auto has=[&](int id){return patch.present[(size_t)id];};
            auto raw=[&](int id){return float(patch.value[(size_t)id]);};
            auto put=[&](int id,int parameter,float translated){
                if(has(id))v[(size_t)parameter]=bounded(translated,param(parameter).min,param(parameter).max);
            };
            auto pct=[&](int id){return clampRaw(raw(id))*100.f/127.f;};
            auto shapeLfo=[](int n){switch(n){case 0:return 2;case 1:return 1;case 2:return 3;case 3:case 4:return 4;case 5:return 0;default:return 0;}};
            put(0,Osc1,raw(0));put(1,Osc2,raw(1));
            put(5,Mix,pct(5));put(8,Color,5.f+90.f*clampRaw(raw(8))/127.f);
            // OpenSynth1 uses a non-linear 128-entry semitone table.
            // Approximate its -60..+60 range within this engine's -24..+24.
            put(2,Semi2,(raw(2)-64.f)*.94f);
            put(3,Fine2,(raw(3)-64.f)*.96875f);
            put(95,SubLevel,pct(95));put(97,SubOct,raw(97)>=1.f?0.f:1.f);
            put(96,SubShape,raw(96));
            put(45,FM,pct(45));put(7,Ring,raw(7));put(6,Sync,raw(6));
            put(107,Master,-36.f+36.f*pct(107)/100.f);
            put(19,Cutoff,15.f*std::pow(1066.66667f,clampRaw(raw(19))/127.f));
            put(20,Reson,pct(20));
            if(has(14)){int ft=(int)raw(14);put(14,FMode,ft==2?2.f:ft==3?1.f:0.f);}
            // Filter amount in the source engine is bipolar around 63.
            put(21,FEnvAmt,bounded((raw(21)-63.f)*100.f/64.f,-100.f,100.f));
            put(22,KeyTrack,bounded(raw(22)*100.f/127.f,0.f,100.f));
            put(23,Drive,pct(23));
            put(15,FAttack,raw(15));put(16,FDecay,raw(16));
            put(17,FSustain,raw(17));put(18,FRelease,raw(18));
            put(25,AAttack,raw(25));put(26,ADecay,raw(26));
            put(27,ASustain,raw(27));put(28,ARelease,raw(28));
            put(30,Velocity,pct(30));put(40,Bend,raw(40));
            put(10,OscModOn,raw(10));put(11,OscModAmt,raw(11));
            put(12,OscModAttack,raw(12));put(13,OscModDecay,raw(13));
            put(71,OscModDest,raw(71));
            put(59,ArpOn,raw(59));
            if(has(31))put(31,ArpMode,raw(31)>0?raw(31)-1:0);
            put(32,ArpOct,raw(32));put(34,ArpGate,std::max(1.f,pct(34)));
            // Original 19 beat divisions collapse to six musical divisions here.
            if(has(33))put(33,ArpBeat,raw(33)<6?0.f:raw(33)<10?1.f:raw(33)<14?2.f:3.f);
            put(38,PlayMode,raw(38));put(39,Porta,1500.f*std::pow(clampRaw(raw(39))/127.f,2.f));
            put(74,AutoPorta,raw(74));
            // Original shape order: saw, triangle, square, random, smooth-random, sine.
            if(has(42))put(42,L1Shape,(float)shapeLfo((int)raw(42)));
            if(has(47))put(47,L2Shape,(float)shapeLfo((int)raw(47)));
            if(has(43))put(43,L1Rate,.05f*std::pow(600.f,clampRaw(raw(43))/127.f));
            if(has(48))put(48,L2Rate,.05f*std::pow(600.f,clampRaw(raw(48))/127.f));
            put(67,L1Sync,raw(67));put(69,L2Sync,raw(69));
            put(44,L1Depth,raw(44));put(49,L2Depth,raw(49));
            if(has(41))put(41,L1Dest,raw(41)>=1?raw(41)-1:0);
            if(has(46))put(46,L2Dest,raw(46)>=1?raw(46)-1:0);
            put(54,ChorusRate,.05f+5.95f*clampRaw(raw(54))/127.f);
            put(53,ChorusDepth,pct(53));put(56,ChorusMix,pct(56)*.70f);
            put(36,DelayFB,pct(36)*.85f);
            put(37,DelayMix,pct(37)*.70f);
            if(has(35))put(35,DelayTime,20.f+1180.f*std::min(19.f,raw(35))/19.f);
            const std::string label=(patch.bank=="Loose"?"":patch.bank+": ")+patch.name;
            addProgram(label.c_str(),std::move(v));
        }
        if(!imported.empty())mpc::log("Synth 1: imported %d SY1 programs",int(imported.size()));
    }

    void prepare(double sampleRate, int) override {
        sr_=sampleRate > 1000.0 ? (float)sampleRate : 44100.f;
        for(auto& v:voices_) {
            v.filter.setSampleRate(sampleRate);
            v.amp.setSampleRate(sampleRate);
            v.filt.setSampleRate(sampleRate);
            v.modEnv.setSampleRate(sampleRate);
        }
        lfo1_.setSampleRate(sr_);lfo2_.setSampleRate(sr_);
        chorusL_.fill(0.f);chorusR_.fill(0.f);delayL_.fill(0.f);delayR_.fill(0.f);
        chorusWrite_=delayWrite_=0;chorusPhase_=0.f;
    }
    void reset() override {
        alloc_.reset();
        for(auto& v:voices_) { v.amp.reset();v.filt.reset();v.modEnv.reset();v.filter.reset();v.note=-1;v.p1=0;v.p2=0;v.ps=0;v.ctrl=0; }
        lfo1_.reset();lfo2_.reset();
        chorusL_.fill(0.f);chorusR_.fill(0.f);delayL_.fill(0.f);delayR_.fill(0.f);
        chorusWrite_=delayWrite_=0;chorusPhase_=0.f;
    }
    void process(mpc::ProcessContext& ctx) override {
        updateParameters(ctx);
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
        for(int i=0;i<ctx.numFrames;++i){
            float dry=softDrive(l[i],s_.drive)*master;
            float left=dry,right=dry;
            // Keep delay state advancing even when mix is zero, avoiding stale echoes.
            chorus(left,right);
            delay(left,right);
            l[i]=bounded(left,-1.f,1.f);
            if(r)r[i]=bounded(right,-1.f,1.f);
        }
    }

private:
    struct Voice {
        ADSR amp,filt,modEnv;SVF filter;
        float p1=0,p2=0,ps=0,pitchNote=60.f;
        uint32_t rng=0x34abcdefu;
        int note=-1,ctrl=0;
        float velocity=1.f;
    };
    struct Settings {
        int osc1=1,osc2=1,semi2=0,fine2=0,suboct=0,fmode=0,bend=2;
        bool ring=false,sync=false,modOn=false,arpOn=false,autoPorta=false;
        int playMode=0,arpMode=1,arpOct=0,arpBeat=2,arpGate=60,subShape=3,modDest=0,l1dest=2,l2dest=4;
        float portaMs=0.f,glideK=0.f,keyTrack=.5f,modAmount=0.f,l1depth=0.f,l2depth=0.f,arpBpm=120.f;
        float l1pitch=0.f,l1filter=0.f,l2pwm=0.f,l2amp=0.f;
        float chorusRate=.35f,chorusDepth=.35f,chorusMix=0.f;
        float delayTime=.36f,delayFB=.28f,delayMix=0.f,drive=0.f;
        float mix=.4f,pw=.5f,sub=0,fm=0,master=-12,cutoff=3000,reso=.15f,fenv=.45f,vel=.6f;
    } s_;
    static constexpr int kVoices=8;
    Voice voices_[kVoices];
    VoiceAllocator<kVoices> alloc_;
    float sr_=44100.f;
    LFO lfo1_,lfo2_;
    float l1_=0.f,l2_=0.f;
    std::array<float,144000> delayL_{},delayR_{};
    std::array<float,4096> chorusL_{},chorusR_{};
    int delayWrite_=0,chorusWrite_=0;
    std::array<uint8_t,128> arpHeld_{},monoHeld_{};
    int arpCurrent_=-1,arpStep_=0,arpCounter_=0,arpGateCounter_=0;
    bool arpWasOn_=false;
    int lastPlayMode_=0;
    uint32_t arpRandom_=0x0138ade1u;
    float chorusPhase_=0.f;

    int countHeld(const std::array<uint8_t,128>& h) const {
        int n=0;for(int i=0;i<128;++i)if(h[(size_t)i])++n;
        return n;
    }
    void releaseAll(){
        for(auto& v:voices_){v.amp.noteOff();v.filt.noteOff();v.modEnv.noteOff();}
        alloc_.reset();arpCurrent_=-1;arpCounter_=0;arpGateCounter_=0;
    }
    int arpPeriod() const {
        const float beat[]={1.f,.5f,.25f,.125f,1.f/3.f,1.f/6.f};
        float b=beat[std::max(0,std::min(5,s_.arpBeat))];
        return std::max(16,int(sr_*60.f*b/std::max(30.f,s_.arpBpm)));
    }
    int arpPick() {
        int notes[128],n=0;
        for(int i=0;i<128;++i)if(arpHeld_[(size_t)i])notes[n++]=i;
        if(n==0)return -1;
        int len=n*(s_.arpOct+1),k=0;
        if(s_.arpMode==0){
            int cycle=std::max(1,2*len-2),pos=arpStep_%cycle;
            k=pos<len?pos:2*len-2-pos;
            arpStep_=(pos+1)%cycle;
        }else if(s_.arpMode==1){k=arpStep_%len;arpStep_=(arpStep_+1)%len;}
        else if(s_.arpMode==2){k=len-1-arpStep_%len;arpStep_=(arpStep_+1)%len;}
        else {
            arpRandom_^=arpRandom_<<13;arpRandom_^=arpRandom_>>17;arpRandom_^=arpRandom_<<5;
            k=int(arpRandom_%(uint32_t)len);
        }
        return std::min(127,notes[k%n]+12*(k/n));
    }
    void startVoice(int idx,int note,int velocity,bool retrigger=true){
        auto& v=voices_[(size_t)idx];
        bool active=v.amp.isActive();
        if(!active || s_.portaMs<=0.f)v.pitchNote=float(note);
        v.note=note;
        v.velocity=1.f-s_.vel+s_.vel*(float(velocity)/127.f);
        if(retrigger||!active){
            v.p1=v.p2=v.ps=0.f;v.ctrl=0;v.filter.reset();
            v.amp.noteOn();v.filt.noteOn();v.modEnv.noteOn();
        }
    }
    void arpTick(){
        if(!s_.arpOn)return;
        if(arpGateCounter_>0 && --arpGateCounter_==0 && arpCurrent_>=0){
            int ix=alloc_.noteOff(arpCurrent_);
            if(ix>=0)release(ix);
            arpCurrent_=-1;
        }
        if(--arpCounter_>0)return;
        arpCounter_=arpPeriod();
        if(arpCurrent_>=0){
            int ix=alloc_.noteOff(arpCurrent_);
            if(ix>=0)release(ix);
            arpCurrent_=-1;
        }
        int note=arpPick();
        if(note>=0){
            int ix=alloc_.noteOn(note,100);
            startVoice(ix,note,100);
            arpCurrent_=note;
            arpGateCounter_=std::max(1,(arpCounter_*s_.arpGate)/100);
        }
    }
    void monoNoteOn(int note,int velocity){
        int previous=countHeld(monoHeld_);
        monoHeld_[(size_t)note]=(uint8_t)velocity;
        auto& v=voices_[0];
        bool active=v.amp.isActive();
        bool legato=s_.playMode==2 && previous>0 && active;
        bool glide=s_.portaMs>0.f && (!s_.autoPorta || previous>0);
        if(!glide)v.pitchNote=float(note);
        startVoice(0,note,velocity,!legato);
    }
    void monoNoteOff(int note){
        monoHeld_[(size_t)note]=0;
        int next=-1;
        for(int i=0;i<128;++i)if(monoHeld_[(size_t)i])next=i;
        if(next<0){release(0);return;}
        auto& v=voices_[0];
        if(s_.portaMs<=0.f)v.pitchNote=float(next);
        v.note=next;
        if(s_.playMode==1){v.amp.noteOn();v.filt.noteOn();v.modEnv.noteOn();}
    }

    static float softDrive(float x,float amount){
        if(amount<=.001f)return x;
        float k=1.f+amount*8.f;
        return (x*k)/(1.f+std::abs(x)*(k-1.f));
    }
    void chorus(float& l,float& r){
        constexpr int N=4096;
        chorusPhase_=wrap(chorusPhase_+s_.chorusRate/sr_);
        float swing=std::sin(chorusPhase_*pi2);
        int center=int(sr_*.013f);
        int width=int(sr_*.008f*s_.chorusDepth);
        auto tap=[&](const std::array<float,N>& b,int d){
            return b[(size_t)((chorusWrite_-std::max(1,d)+2*N)%N)];
        };
        float wetL=tap(chorusL_,center+int(width*swing));
        float wetR=tap(chorusR_,center-int(width*swing));
        chorusL_[(size_t)chorusWrite_]=l;
        chorusR_[(size_t)chorusWrite_]=r;
        chorusWrite_=(chorusWrite_+1)%N;
        l=l*(1.f-s_.chorusMix)+wetL*s_.chorusMix;
        r=r*(1.f-s_.chorusMix)+wetR*s_.chorusMix;
    }
    void delay(float& l,float& r){
        constexpr int N=144000;
        int d=std::min(N-2,std::max(1,int(s_.delayTime*sr_)));
        int tl=(delayWrite_-d+N)%N;
        int tr=(delayWrite_-std::min(N-2,d+int(sr_*.007f))+N)%N;
        float dl=delayL_[(size_t)tl],dr=delayR_[(size_t)tr];
        delayL_[(size_t)delayWrite_]=bounded(l+dr*s_.delayFB,-2.f,2.f);
        delayR_[(size_t)delayWrite_]=bounded(r+dl*s_.delayFB,-2.f,2.f);
        delayWrite_=(delayWrite_+1)%N;
        l=l*(1.f-s_.delayMix)+dl*s_.delayMix;
        r=r*(1.f-s_.delayMix)+dr*s_.delayMix;
    }

    void updateParameters(mpc::ProcessContext& ctx) {
        s_.osc1=getInt(Osc1);s_.osc2=getInt(Osc2);s_.semi2=getInt(Semi2);s_.fine2=getInt(Fine2);
        s_.suboct=getInt(SubOct);s_.fmode=getInt(FMode);
        s_.ring=getBool(Ring);s_.sync=getBool(Sync);s_.bend=getInt(Bend);
        s_.mix=get(Mix)*.01f;s_.pw=bounded(get(Color)*.01f,.05f,.95f);
        s_.sub=get(SubLevel)*.01f;s_.fm=get(FM)*.01f;
        s_.master=get(Master);s_.cutoff=get(Cutoff);
        s_.reso=get(Reson)*.01f;s_.fenv=get(FEnvAmt)*.01f;s_.vel=get(Velocity)*.01f;
        alloc_.setBendRange((float)s_.bend);
        s_.l1pitch=get(L1Pitch); s_.l1filter=get(L1Filter)*.01f;
        s_.l2pwm=get(L2PWM)*.01f; s_.l2amp=get(L2Amp)*.01f;
        lfo1_.setShape(getInt(L1Shape));lfo2_.setShape(getInt(L2Shape));
        float bpm=(ctx.transport.tempoValid && ctx.transport.bpm>20.0)?float(ctx.transport.bpm):120.f;
        if(getBool(L1Sync)){
            lfo1_.setTempoSync(bpm,divisionBeats(getInt(L1Div)));
            if(ctx.transport.playing && ctx.transport.ppqValid)lfo1_.syncToPpq(ctx.transport.ppqPos);
        }else lfo1_.setFrequency(get(L1Rate));
        if(getBool(L2Sync)){
            lfo2_.setTempoSync(bpm,divisionBeats(getInt(L2Div)));
            if(ctx.transport.playing && ctx.transport.ppqValid)lfo2_.syncToPpq(ctx.transport.ppqPos);
        }else lfo2_.setFrequency(get(L2Rate));
        s_.chorusRate=get(ChorusRate);s_.chorusDepth=get(ChorusDepth)*.01f;s_.chorusMix=get(ChorusMix)*.01f;
        s_.delayTime=get(DelayTime)*.001f;s_.delayFB=get(DelayFB)*.01f;s_.delayMix=get(DelayMix)*.01f;
        s_.drive=get(Drive)*.01f;
        s_.arpOn=getBool(ArpOn);s_.arpMode=getInt(ArpMode);s_.arpOct=getInt(ArpOct);
        s_.arpBeat=getInt(ArpBeat);s_.arpGate=getInt(ArpGate);
        s_.playMode=getInt(PlayMode);s_.portaMs=get(Porta);s_.autoPorta=getBool(AutoPorta);
        s_.keyTrack=get(KeyTrack)*.01f;s_.subShape=getInt(SubShape);
        s_.modOn=getBool(OscModOn);s_.modAmount=(get(OscModAmt)-64.f)/64.f;s_.modDest=getInt(OscModDest);
        s_.l1dest=getInt(L1Dest);s_.l2dest=getInt(L2Dest);
        s_.l1depth=get(L1Depth)/127.f;s_.l2depth=get(L2Depth)/127.f;
        s_.arpBpm=bpm;
        s_.glideK=s_.portaMs>0.f?std::exp(-1000.f/(sr_*s_.portaMs)):0.f;
        if(s_.arpOn!=arpWasOn_ || s_.playMode!=lastPlayMode_){
            releaseAll();arpHeld_.fill(0);monoHeld_.fill(0);arpStep_=0;
            arpWasOn_=s_.arpOn;lastPlayMode_=s_.playMode;
        }
        float fa=envMs(get(FAttack)),fd=envMs(get(FDecay)),fs=susLevel(get(FSustain)),fr=envMs(get(FRelease));
        float aa=envMs(get(AAttack)),ad=envMs(get(ADecay)),as=susLevel(get(ASustain)),ar=envMs(get(ARelease));
        for(auto& v:voices_) {
            v.filt.set(fa,fd,fs,fr);
            v.amp.set(aa,ad,as,ar);
            v.modEnv.set(envMs(get(OscModAttack)),envMs(get(OscModDecay)),0.f,5.f);
        }
    }
    void release(int n) { voices_[n].amp.noteOff();voices_[n].filt.noteOff();voices_[n].modEnv.noteOff(); }
    void midi(const mpc::MidiEvent& ev) {
        if(ev.isNoteOn()){
            if(s_.arpOn){
                arpHeld_[(size_t)ev.note()]=(uint8_t)ev.velocity();
                if(countHeld(arpHeld_)==1)arpCounter_=0;
            }else if(s_.playMode!=0) monoNoteOn(ev.note(),ev.velocity());
            else {
                int ix=alloc_.noteOn(ev.note(),ev.velocity());
                startVoice(ix,ev.note(),ev.velocity());
            }
        }else if(ev.isNoteOff()){
            if(s_.arpOn){
                arpHeld_[(size_t)ev.note()]=0;
                if(countHeld(arpHeld_)==0&&arpCurrent_>=0){
                    int ix=alloc_.noteOff(arpCurrent_);if(ix>=0)release(ix);
                    arpCurrent_=-1;
                }
            }else if(s_.playMode!=0)monoNoteOff(ev.note());
            else {int ix=alloc_.noteOff(ev.note());if(ix>=0)release(ix);}
        }else if(ev.isAllNotesOff()){
            arpHeld_.fill(0);monoHeld_.fill(0);releaseAll();
        }else if(ev.isController() && ev.controller()==64){
            if(!s_.arpOn&&s_.playMode==0)
                alloc_.setSustain(ev.controllerValue()>=64,[this](int ix){release(ix);});
        }else if(ev.isPitchBend())alloc_.setPitchBend(ev.pitchBend());
        // CC1 is routed by the host when mapped, without audio-thread parameter writes.
    }
    void render(mpc::ProcessContext& ctx,int from,int to) {
        float* out=ctx.outputs[0];
        for(int i=from;i<to;++i){
            l1_=lfo1_.process();l2_=lfo2_.process();
            float sum=0.f;
            for(int ix=0;ix<kVoices;++ix){
                Voice& v=voices_[ix];if(!v.amp.isActive())continue;
                float fa=v.filt.process(),aa=v.amp.process();
                float base=alloc_.noteToHz((float)v.note);
                float pitch=std::pow(2.f,(l1_*s_.l1pitch)/12.f);
                float d1=bounded(base*pitch/sr_,0.f,.45f);
                float d2=bounded(base*pitch*std::pow(2.f,(s_.semi2+s_.fine2*.01f)/12.f)/sr_,0.f,.45f);
                float ds=bounded(base*pitch/(sr_*(s_.suboct==0?2.f:4.f)),0.f,.45f);
                float pw=bounded(s_.pw+l2_*s_.l2pwm*.45f,.05f,.95f);
                // OpenSynth1-style oscillator-2 phase modulation of oscillator-1.
                float osc2=s_.osc2==0?0.f:oscSample(v.p2,d2,s_.osc2,pw,v.rng);
                float phase=wrap(v.p1+osc2*(s_.fm*s_.fm*3.5f));
                float osc1=oscSample(phase,d1,s_.osc1,pw,v.rng);
                float sub=oscSample(v.ps,ds,2,.5f,v.rng);
                float mixed=s_.ring ? osc1*osc2 : osc1*(1.f-s_.mix)+osc2*s_.mix;
                mixed += sub*s_.sub*.45f;
                if(++v.ctrl>=16) {
                    v.ctrl=0;
                    float oct=bounded(s_.fenv*fa*7.f+s_.l1filter*l1_*3.f,-10.f,10.f);
                    v.filter.set(bounded(s_.cutoff*std::pow(2.f,oct),25.f,sr_*.44f),s_.reso);
                }
                float tremolo=1.f-.5f*s_.l2amp*(1.f+l2_);
                sum+=v.filter.process(mixed,s_.fmode)*aa*v.velocity*.24f*tremolo;
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
