// Scratch Cat FX: the plugin (audio processor). Engine in src/core. The editor is JUCE's generic one for now;
// the cat-ui editor from docs/mockup is on the ROADMAP.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../core/ScfxEngine.h"

class ScratchFxProcessor : public juce::AudioProcessor
{
public:
    ScratchFxProcessor();
    const juce::String getName() const override { return "Scratch Cat FX"; }
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override
    {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    scfx::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    float get(const char* id) const { return apvts.getRawParameterValue(id)->load(); }
    bool wasTouch = false, wasBrake = false, wasSpin = false, wasLive = false;
    std::vector<float> inL, inR;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScratchFxProcessor)
};
