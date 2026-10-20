// Scratch Cat FX: the plugin (audio processor). Engine in src/core, editor in ScratchFxEditor (cat-ui, from docs/mockup).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "../core/ScfxEngine.h"
#include <array>
#include <atomic>
#include <memory>

namespace scfxui
{
// Things the editor asks the audio thread to do (platter drag, waveform drag, beat loops, source switch, delete a hot cue).
enum CmdType { kJog = 1, kBeatLoop, kHotClear, kSeek, kLive, kLoopExit };
struct Cmd { int type = 0, i = 0; double a = 0.0; };
}

class ScratchFxProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    ScratchFxProcessor();
    ~ScratchFxProcessor() override;

    // ---- editor side ----
    void post(const scfxui::Cmd& c);
    void loadFile(const juce::File& f);                  // analyse on a background thread, then use as the deck source
    void capture();                                      // grab the last N bars of the DAW input into the deck
    void selectSource(int kind);                         // 0 DAW input (live), 1 captured loop, 2 loaded file
    int sourceKind() const { return view.src.load(); }
    bool hasTrackKind(int kind) const;
    std::shared_ptr<const scfx::Track> shownTrack() const;
    juce::String statusLine() const;
    juce::String fileLabel() const;
    float get(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }
    void setParam(const juce::String& id, float realValue);
    bool tipsOn() const { return (bool) apvts.state.getProperty("tips", true); }
    void setTipsOn(bool b) { apvts.state.setProperty("tips", b, nullptr); }
    int getProp(const juce::String& k, int def) const { return (int) apvts.state.getProperty(k, def); }
    void setProp(const juce::String& k, int v) { apvts.state.setProperty(k, v, nullptr); }

    // read by the editor (written on the audio thread)
    struct View {
        std::atomic<double> pos { 0.0 }, len { 0.0 }, bpm { 0.0 }, cue { 0.0 }, loopA { -1.0 }, loopB { -1.0 }, behind { 0.0 }, tempoPct { 0.0 }, hostBpm { 120.0 };
        std::atomic<float> phase { -1.f };
        std::atomic<bool> playing { false }, live { true }, loopOn { false }, touching { false }, hostHasBpm { false }, synced { false };
        std::atomic<int> src { 0 };                      // 0 live, 1 capture, 2 file
        std::array<std::atomic<double>, 8> hot;
        View() { for (auto& h : hot) h.store(-1.0); }
    } view;

    juce::AudioProcessorValueTreeState apvts;
    scfx::Engine engine;

    // ---- AudioProcessor ----
    const juce::String getName() const override { return "Scratch Cat FX"; }
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override
    {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void handleAsyncUpdate() override;
    void applyCmd(const scfxui::Cmd&);
    void finishLoad(std::shared_ptr<scfx::Track> t, int kind, const juce::String& info);
    void queueTrack(int kind, std::shared_ptr<const scfx::Track> t);

    juce::AbstractFifo cmdFifo { 256 };
    std::array<scfxui::Cmd, 256> cmdBuf;
    double fs = 44100.0; int maxBlock = 512;
    std::vector<float> inL, inR;
    // edge detection of the host-automatable switches
    bool wasTouch = false, wasBrake = false, wasSpin = false, wasLive = false, wasPlay = false, wasCue = false, wasHot[8] = {}, wasLoopIn = false, wasLoopOut = false, wasLoopExit = false;
    int wasMove = 0;
    juce::SpinLock trackLock, ringLock, statusLock;
    std::shared_ptr<const scfx::Track> pending, retired;
    std::shared_ptr<const scfx::Track> shown, capTrack, fileTrack;
    int pendingKind = -1;
    double restoreHot[8]; bool haveRestore = false;
    juce::String analysis, fileName;
    double stampPpq = -1.0; unsigned long long stampHead = 0;      // musical position and ring position at the end of the last block (under trackLock)
    juce::AudioFormatManager formats;
    juce::ThreadPool pool { 1 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScratchFxProcessor)
};
