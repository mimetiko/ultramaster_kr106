#include "PluginEditor.h"
#include <algorithm>
#include <functional>
#include <vector>

namespace {
constexpr int W=1780,H=1000;
const juce::Colour red(0xffb52631), redLit(0xffec4650), face(0xffa8232f),
                   charcoal(0xff26292d), section(0xff373b40), cream(0xffe8e3d8),
                   muted(0xffadb3b6), lcd(0xff101d1b), mint(0xffb7d6bd);

struct Cell { const char* id; const char* label; char kind='k'; };
struct Panel { int x,y,w,h,cols; const char* title; std::vector<Cell> cells; };

struct MimetikoLook final : juce::LookAndFeel_V4 {
    MimetikoLook() {
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1d2225));
        setColour(juce::ComboBox::textColourId, cream);
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff646c72));
        setColour(juce::ComboBox::arrowColourId, redLit);
        setColour(juce::PopupMenu::backgroundColourId, charcoal);
        setColour(juce::PopupMenu::textColourId, cream);
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3e4246));
        setColour(juce::TextButton::textColourOffId, cream);
        setColour(juce::Label::textColourId, cream);
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,
                          float start,float end,juce::Slider&) override {
        auto bounds=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h);
        const auto c=bounds.getCentre();
        const float radius=std::min(bounds.getWidth(),bounds.getHeight())*.42f;
        g.setColour(juce::Colour(0xff17191a)); g.fillEllipse(c.x-radius-2,c.y-radius-2,(radius+2)*2,(radius+2)*2);
        g.setColour(juce::Colour(0xff737778)); g.drawEllipse(c.x-radius-2,c.y-radius-2,(radius+2)*2,(radius+2)*2,1.5f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff656b6b),c.x,c.y-radius,
                                               juce::Colour(0xff242628),c.x,c.y+radius,false));
        g.fillEllipse(c.x-radius,c.y-radius,radius*2,radius*2);
        const float a=start+pos*(end-start)-juce::MathConstants<float>::halfPi;
        g.setColour(cream);
        g.drawLine(c.x+std::cos(a)*radius*.28f,c.y+std::sin(a)*radius*.28f,
                   c.x+std::cos(a)*radius*.84f,c.y+std::sin(a)*radius*.84f,2.5f);
        g.setColour(redLit); g.drawArc(c.x-radius-5,c.y-radius-5,(radius+5)*2,(radius+5)*2,
                                       start,end,1.2f);
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool,bool) override {
        auto r=b.getLocalBounds().reduced(2).toFloat();
        g.setColour(juce::Colour(0xff1e2225));g.fillRoundedRectangle(r,4);
        g.setColour(b.getToggleState()?redLit:juce::Colour(0xff737778));
        g.fillEllipse(r.getX()+7,r.getCentreY()-3,6,6);
        g.setColour(cream);g.setFont(juce::Font(juce::FontOptions(11.f).withStyle("Bold")));
        g.drawText(b.getButtonText(),r.withTrimmedLeft(19),juce::Justification::centredLeft,true);
    }
};
using Attachment=juce::AudioProcessorValueTreeState::SliderAttachment;
using ChoiceAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
using ToggleAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;

class LearnSlider final : public juce::Slider {
public:
    std::function<void()> learn;
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.mods.isRightButtonDown()) { if(learn)learn(); return; }
        juce::Slider::mouseDown(e);
    }
};
class LearnChoice final : public juce::ComboBox {
public:
    std::function<void()> learn;
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.mods.isRightButtonDown()) { if(learn)learn(); return; }
        juce::ComboBox::mouseDown(e);
    }
};
class LearnToggle final : public juce::ToggleButton {
public:
    std::function<void()> learn;
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.mods.isRightButtonDown()) { if(learn)learn(); return; }
        juce::ToggleButton::mouseDown(e);
    }
};
std::vector<Panel> makePanels() {
    return {
        {16,112,395,199,5,"OSCILLATOR 1",{
            {"0","WAVE",'c'},{"45","FM"},{"76","DETUNE"},{"95","SUB LEVEL"},
            {"96","SUB WAVE",'c'},{"97","SUB OCT",'c'}}},
        {421,112,395,199,5,"OSCILLATOR 2",{
            {"1","WAVE",'c'},{"2","PITCH"},{"108","PITCH ON",'t'},{"3","FINE"},
            {"109","FINE ON",'t'},{"4","KEY TRACK",'t'},{"6","SYNC",'t'},{"7","RING MOD",'t'}}},
        {826,112,500,199,6,"OSCILLATOR COMMON",{
            {"5","MIX"},{"8","PULSE WIDTH"},{"9","KEY SHIFT"},
            {"72","FINE TUNE"},{"91","PHASE"}}},
        {1336,112,428,199,5,"VOICE & UNISON",{
            {"38","MODE",'c'},{"94","POLY"},{"73","UNISON",'t'},{"93","VOICES"},
            {"75","DETUNE"},{"84","SPREAD"},{"85","PITCH"},{"92","PHASE"},
            {"39","GLIDE"},{"74","AUTO",'t'}}},
        {16,321,544,199,6,"FILTER",{
            {"14","TYPE",'c'},{"19","CUTOFF"},{"20","RESONANCE"},{"21","ENV AMT"},
            {"22","KEY TRACK"},{"23","DRIVE"},{"24","VELOCITY",'t'}}},
        {570,321,336,199,4,"FILTER ENVELOPE",{
            {"15","ATTACK",'f'},{"16","DECAY",'f'},{"17","SUSTAIN",'f'},{"18","RELEASE",'f'}}},
        {916,321,336,199,4,"AMP ENVELOPE",{
            {"25","ATTACK",'f'},{"26","DECAY",'f'},{"27","SUSTAIN",'f'},{"28","RELEASE",'f'}}},
        {1262,321,228,199,2,"AMPLIFIER",{
            {"29","GAIN"},{"30","VELOCITY"}}},
        {1500,321,264,199,3,"ARPEGGIATOR",{
            {"59","ON",'t'},{"31","MODE",'c'},{"32","RANGE",'c'},
            {"33","RATE"},{"34","GATE"}}},
        {16,530,435,208,5,"LFO 1",{
            {"57","ON",'t'},{"42","WAVE",'c'},{"41","DEST",'c'},
            {"43","RATE"},{"44","DEPTH"},{"67","TEMPO",'t'},{"68","KEY SYNC",'t'}}},
        {461,530,435,208,5,"LFO 2",{
            {"58","ON",'t'},{"47","WAVE",'c'},{"46","DEST",'c'},
            {"48","RATE"},{"49","DEPTH"},{"69","TEMPO",'t'},{"70","KEY SYNC",'t'}}},
        {906,530,414,208,5,"MODULATION ENVELOPE",{
            {"10","ON",'t'},{"12","ATTACK"},{"13","DECAY"},
            {"11","AMOUNT"},{"71","DEST",'c'}}},
        {1330,530,434,208,6,"MIDI ROUTING",{
            {"86","SOURCE 1"},{"50","SENS 1"},{"87","ASSIGN 1"},
            {"88","SOURCE 2"},{"51","SENS 2"},{"89","ASSIGN 2"},
            {"40","BEND RNG"}}},
        {16,748,345,235,4,"DELAY",{
            {"65","ON",'t'},{"82","TYPE",'c'},{"35","TIME"},{"83","SPREAD"},
            {"36","FEEDBACK"},{"98","TONE"},{"37","MIX"}}},
        {371,748,349,235,4,"CHORUS / FLANGER",{
            {"66","ON",'t'},{"64","TYPE",'c'},{"52","TIME"},{"53","DEPTH"},
            {"54","RATE"},{"55","FEEDBACK"},{"56","LEVEL"}}},
        {730,748,359,235,4,"MULTI EFFECT",{
            {"77","ON",'t'},{"78","TYPE",'c'},{"79","CONTROL 1"},
            {"80","CONTROL 2"},{"81","LEVEL"}}},
        {1099,748,335,235,4,"EQ & PAN",{
            {"60","TONE"},{"61","FREQUENCY"},{"62","LEVEL"},{"63","Q"},{"90","PAN"}}},
        {1444,748,320,235,3,"MASTER",{
            {"107","VOLUME"}}}
    };
}
} // namespace

class MimetikoSynthCanvas final : public juce::Component {
public:
    explicit MimetikoSynthCanvas(OpenSynth1AudioProcessor& p) : processor(p) {
        setLookAndFeel(&look);
        addAndMakeVisible(presetDisplay); addAndMakeVisible(bankButton);
        addAndMakeVisible(prevButton); addAndMakeVisible(nextButton);
        addAndMakeVisible(loadButton); addAndMakeVisible(saveButton); addAndMakeVisible(initButton);
        prevButton.setButtonText("<");nextButton.setButtonText(">");
        bankButton.setButtonText("BANK");
        loadButton.setButtonText("LOAD .SY1");saveButton.setButtonText("SAVE .SY1");
        initButton.setButtonText("INIT");
        presetDisplay.setButtonText("001 / INITIAL SOUND");
        prevButton.onClick=[this]{processor.selectPrevPreset();syncPreset();};
        nextButton.onClick=[this]{processor.selectNextPreset();syncPreset();};
        bankButton.onClick=[this]{showBankMenu();};
        presetDisplay.onClick=[this]{showPresetMenu();};
        loadButton.onClick=[this]{chooseFile(false);};
        saveButton.onClick=[this]{chooseFile(true);};
        initButton.onClick=[this]{processor.resetToInitPreset();syncPreset();};
        for(auto& panel:makePanels()) {
            panels.push_back(panel);
            for(int i=0;i<(int)panel.cells.size();++i)
                addControl(panel,panel.cells[i],i);
        }
        syncPreset();
    }
    ~MimetikoSynthCanvas() override { setLookAndFeel(nullptr); }
    void resized() override {
        presetDisplay.setBounds(689,25,355,44);
        bankButton.setBounds(1052,25,72,44);
        prevButton.setBounds(1133,25,41,44);
        nextButton.setBounds(1180,25,41,44);
        loadButton.setBounds(1233,25,110,44);
        saveButton.setBounds(1352,25,110,44);
        initButton.setBounds(1472,25,76,44);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(charcoal);
        juce::ColourGradient top(face,0,0,juce::Colour(0xff801c28),0,100,false);
        g.setGradientFill(top);g.fillRect(0,0,W,99);
        g.setColour(juce::Colour(0xfff2e4db));
        g.setFont(juce::Font(juce::FontOptions(34.f).withStyle("Bold")));
        g.drawText("MIMETIKO",26,16,330,42,juce::Justification::centredLeft);
        g.setFont(juce::Font(juce::FontOptions(17.f).withStyle("Bold")));
        g.drawText("SYNTH 1",28,61,220,20,juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xffe9adac));
        g.setFont(juce::Font(juce::FontOptions(12.f).withStyle("Bold")));
        g.drawText("VIRTUAL ANALOG  /  MODERN EDITION",280,53,370,24,juce::Justification::centredLeft);
        g.setColour(lcd);g.fillRoundedRectangle(681.f,17.f,371.f,59.f,5);
        g.setColour(juce::Colour(0xff64756d));g.drawRoundedRectangle(681.f,17.f,371.f,59.f,5,1);
        g.setColour(cream);
        g.setFont(juce::Font(juce::FontOptions(11.f)));
        g.drawText("RIGHT CLICK A CONTROL TO MIDI LEARN",1550,25,216,25,juce::Justification::centred);
        for(const auto& panel:panels) {
            auto r=juce::Rectangle<float>((float)panel.x,(float)panel.y,(float)panel.w,(float)panel.h);
            g.setColour(section);g.fillRoundedRectangle(r,6);
            g.setColour(juce::Colour(0xff66696c));g.drawRoundedRectangle(r,6,1);
            g.setColour(juce::Colour(0xff232629));g.fillRoundedRectangle(r.removeFromTop(30),5);
            g.setColour(cream);g.setFont(juce::Font(juce::FontOptions(13.f).withStyle("Bold")));
            g.drawText(panel.title,panel.x+13,panel.y+5,panel.w-20,21,juce::Justification::centredLeft);
            g.setColour(redLit);g.fillRect(panel.x+11,panel.y+28,juce::jmin(92,panel.w-22),2);
        }
        g.setColour(juce::Colour(0xffaeb1ad));g.setFont(juce::Font(juce::FontOptions(10.f)));
        g.drawText("MIMETIKO  /  ONE PAGE SOUND DESIGN",23,986,350,11,juce::Justification::centredLeft);
    }
    void syncPreset() {
        presetDisplay.setButtonText(juce::String::formatted("%03d  /  ",processor.getCurrentProgramNumber())
                                         +processor.getCurrentProgramName().toUpperCase());
        bankButton.setTooltip(processor.getCurrentBankName());
        repaint(675,15,555,75);
    }
private:
    void addControl(const Panel& p,const Cell& c,int index) {
        const int rows=((int)p.cells.size()+p.cols-1)/p.cols;
        const int col=index%p.cols,row=index/p.cols;
        const float cw=float(p.w-20)/float(p.cols);
        const float available=float(p.h-39);
        const float ch=available/float(rows);
        const int x=p.x+10+int(cw*col),y=p.y+35+int(ch*row);
        const int ww=std::max(34,int(cw)-6);
        const auto id=juce::String(c.id);
        const int paramId=id.getIntValue();
        auto startLearn=[this,paramId] { processor.startMidiLearn(paramId); };
        auto label=std::make_unique<juce::Label>();
        label->setText(c.label,juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::Font(juce::FontOptions(10.f).withStyle("Bold")));
        label->setColour(juce::Label::textColourId,muted);
        label->setInterceptsMouseClicks(false,false);
        if(c.kind=='c') {
            auto co=std::make_unique<LearnChoice>();
            co->learn=startLearn;
            if(auto* choice=dynamic_cast<juce::AudioParameterChoice*>(processor.apvts.getParameter(id))) {
                int choiceIndex=1;
                for(auto& option:choice->choices)co->addItem(option,choiceIndex++);
            }
            co->setBounds(x+2,y+15,ww-4,27);
            co->setTooltip(juce::String(c.label)+" (right-click: MIDI Learn)");
            addAndMakeVisible(*co);
            choiceAttachments.push_back(std::make_unique<ChoiceAttachment>(processor.apvts,id,*co));
            label->setBounds(x,y+44,ww,19);
            choices.push_back(std::move(co));
        } else if(c.kind=='t') {
            auto toggle=std::make_unique<LearnToggle>();
            toggle->learn=startLearn;
            toggle->setButtonText("ON / OFF");
            toggle->setBounds(x+2,y+19,ww-4,29);
            toggle->setTooltip(juce::String(c.label)+" (right-click: MIDI Learn)");
            addAndMakeVisible(*toggle);
            toggleAttachments.push_back(std::make_unique<ToggleAttachment>(processor.apvts,id,*toggle));
            label->setBounds(x,y+49,ww,18);
            toggles.push_back(std::move(toggle));
        } else {
            auto sl=std::make_unique<LearnSlider>();
            sl->learn=startLearn;
            sl->setSliderStyle(c.kind=='f'?juce::Slider::LinearVertical:juce::Slider::RotaryHorizontalVerticalDrag);
            sl->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            sl->setPopupDisplayEnabled(true,true,this);
            sl->setBounds(x+3,y+2,ww-6,juce::jmin(72,int(ch)-25));
            sl->setTooltip(juce::String(c.label)+" (right-click: MIDI Learn)");
            addAndMakeVisible(*sl);
            sliderAttachments.push_back(std::make_unique<Attachment>(processor.apvts,id,*sl));
            label->setBounds(x,y+juce::jmin(75,int(ch)-21),ww,17);
            sliders.push_back(std::move(sl));
        }
        addAndMakeVisible(*label);
        labels.push_back(std::move(label));
    }
    void showBankMenu() {
        juce::PopupMenu menu;
        const auto& banks=processor.getBanks();
        for(int i=0;i<(int)banks.size();++i)
            menu.addItem(i+1,juce::String::formatted("%02d  ",banks[i].bankNumber)+banks[i].name,
                         true,i==processor.getCurrentBankNumber());
        auto safe=juce::Component::SafePointer<MimetikoSynthCanvas>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&bankButton),
                           [safe](int n){if(safe && n>0) {
                               safe->processor.selectBankAndProgram(n-1,1);safe->syncPreset();
                           }});
    }
    void showPresetMenu() {
        const auto& banks=processor.getBanks();
        const int idx=processor.getCurrentBankNumber();
        if(idx<0||idx>=(int)banks.size())return;
        juce::PopupMenu menu;
        for(const auto& item:banks[idx].programs)
            menu.addItem(item.index,juce::String::formatted("%03d  ",item.index)+item.name,
                         true,item.index==processor.getCurrentProgramNumber());
        auto safe=juce::Component::SafePointer<MimetikoSynthCanvas>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetDisplay),
                           [safe,idx](int n){if(safe && n>0) {
                               safe->processor.selectBankAndProgram(idx,n);safe->syncPreset();
                           }});
    }
    void chooseFile(bool save) {
        chooser=std::make_unique<juce::FileChooser>(
            save?"Save Synth1 patch":"Load Synth1 patch",
            processor.getBankDirectory(), "*.sy1");
        auto safe=juce::Component::SafePointer<MimetikoSynthCanvas>(this);
        chooser->launchAsync(save?juce::FileBrowserComponent::saveMode:
                                  juce::FileBrowserComponent::openMode,
            [safe,save](const juce::FileChooser& fc) {
                if(!safe)return;
                auto f=fc.getResult();
                if(f==juce::File{})return;
                if(save) {
                    if(!f.hasFileExtension(".sy1"))f=f.withFileExtension(".sy1");
                    safe->processor.saveSy1Preset(f,safe->processor.getCurrentProgramName(),
                                                     safe->processor.getCurrentColorName());
                } else safe->processor.loadSy1Preset(f);
                safe->syncPreset();
            });
    }
    OpenSynth1AudioProcessor& processor;
    MimetikoLook look;
    std::vector<Panel> panels;
    juce::TextButton presetDisplay,bankButton,prevButton,nextButton,loadButton,saveButton,initButton;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<LearnSlider>> sliders;
    std::vector<std::unique_ptr<LearnChoice>> choices;
    std::vector<std::unique_ptr<LearnToggle>> toggles;
    std::vector<std::unique_ptr<Attachment>> sliderAttachments;
    std::vector<std::unique_ptr<ChoiceAttachment>> choiceAttachments;
    std::vector<std::unique_ptr<ToggleAttachment>> toggleAttachments;
};

OpenSynth1AudioProcessorEditor::OpenSynth1AudioProcessorEditor(OpenSynth1AudioProcessor& p)
: AudioProcessorEditor(&p),processor(p),canvas(std::make_unique<MimetikoSynthCanvas>(p)) {
    addAndMakeVisible(*canvas);
    setResizable(true,true);
    setResizeLimits(1157,650,2400,1348);
    getConstrainer()->setFixedAspectRatio(double(W)/double(H));
    setSize(W,H);
    startTimerHz(8);
}
OpenSynth1AudioProcessorEditor::~OpenSynth1AudioProcessorEditor() { stopTimer(); canvas.reset(); }
void OpenSynth1AudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(charcoal); }
void OpenSynth1AudioProcessorEditor::resized() {
    auto scale=std::min(float(getWidth())/W,float(getHeight())/H);
    canvas->setBounds(0,0,W,H);
    canvas->setTransform(juce::AffineTransform::scale(scale));
}
void OpenSynth1AudioProcessorEditor::timerCallback() {
    if(processor.consumePresetLoaded())canvas->syncPreset();
}
