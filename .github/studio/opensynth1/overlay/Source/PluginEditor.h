#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>

// Mimetiko Synth 1: spacious, single-page, Nord-inspired JUCE editor.
// Only the editor changes: all DSP, parameter IDs and .sy1 compatibility remain upstream.
class MimetikoSynthCanvas;

class OpenSynth1AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit OpenSynth1AudioProcessorEditor(OpenSynth1AudioProcessor& processor);
    ~OpenSynth1AudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    OpenSynth1AudioProcessor& processor;
    std::unique_ptr<MimetikoSynthCanvas> canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenSynth1AudioProcessorEditor)
};

using Synth1CloneAudioProcessorEditor = OpenSynth1AudioProcessorEditor;
