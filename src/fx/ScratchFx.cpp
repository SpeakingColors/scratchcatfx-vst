#include "ScratchFx.h"

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using juce::NormalisableRange;

Layout ScratchFxProcessor::layout()
{
    Layout l;
    auto f = [&l](const char* id, const char* name, float lo, float hi, float def, const char* unit = "") {
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, NormalisableRange<float>(lo, hi), def, juce::AudioParameterFloatAttributes().withLabel(unit)));
    };
    auto b = [&l](const char* id, const char* name) { l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 }, name, false)); };
    // scratch gestures (automate or MIDI-map these in the host)
    b("touch", "Hand on platter");
    f("speed", "Platter speed", -4.f, 4.f, 0.f, "x");
    b("brake", "Brake"); b("backspin", "Backspin"); b("live", "Go live");
    // sound
    f("trim", "Trim", -12.f, 12.f, 0.f, "dB");
    f("lo", "Low", -26.f, 12.f, 0.f, "dB"); f("mid", "Mid", -26.f, 12.f, 0.f, "dB"); f("hi", "High", -26.f, 12.f, 0.f, "dB");
    f("colour", "Colour", -1.f, 1.f, 0.f);
    f("master", "Master", -24.f, 12.f, 0.f, "dB");
    b("purron", "PurrOne colour on"); f("purrdrive", "PurrOne Drive", 0.f, 1.f, 0.f); f("purrxs", "PurrOne Transistor", 0.f, 1.f, 0.f); f("purrfuzz", "PurrOne Fuzz", 0.f, 1.f, 0.f); f("purrmix", "PurrOne mix", 0.f, 1.f, 1.f);
    const juce::StringArray names { "Off", "Filter", "Echo", "Reverb", "Flanger", "Crush" };
    for (int k = 0; k < 3; ++k)
    {
        const juce::String s(k + 1);
        l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "fx" + s + "t", 1 }, "FX " + s + " type", names, k + 1));
        l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "fx" + s + "o", 1 }, "FX " + s + " on", false));
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "fx" + s + "p1", 1 }, "FX " + s + " amount", 0.f, 1.f, 0.5f));
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "fx" + s + "p2", 1 }, "FX " + s + " mix", 0.f, 1.f, 0.4f));
    }
    f("chain", "FX chain mix", 0.f, 1.f, 0.6f);
    f("echo", "Echo beats", 0.125f, 2.f, 0.75f, "beats");
    return l;
}

ScratchFxProcessor::ScratchFxProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", layout())
{
}

void ScratchFxProcessor::prepareToPlay(double sr, int maxBlock)
{
    engine.prepare(sr);
    inL.assign((size_t) maxBlock + 8, 0.f); inR.assign((size_t) maxBlock + 8, 0.f);
    wasTouch = wasBrake = wasSpin = wasLive = false;
}

void ScratchFxProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    if ((int) inL.size() < n) { inL.resize((size_t) n + 8); inR.resize((size_t) n + 8); }
    const int chans = buf.getNumChannels();
    if (chans < 1) return;
    std::copy(buf.getReadPointer(0), buf.getReadPointer(0) + n, inL.begin());
    std::copy(buf.getReadPointer(chans > 1 ? 1 : 0), buf.getReadPointer(chans > 1 ? 1 : 0) + n, inR.begin());

    // gestures: act on the rising edge of the switches
    const bool touch = get("touch") > 0.5f, brake = get("brake") > 0.5f, spin = get("backspin") > 0.5f, live = get("live") > 0.5f;
    if (live && !wasLive) engine.goLive();
    if (brake && !wasBrake) engine.brake();
    if (spin && !wasSpin) engine.backspin();
    if (touch) engine.touch(true, (double) get("speed"));
    else if (wasTouch) { engine.touch(false, 0.0); engine.deck.setPlaying(true); }     // letting go plays on at normal speed
    wasTouch = touch; wasBrake = brake; wasSpin = spin; wasLive = live;

    scfx::FxParams p;
    p.trimDb = get("trim"); p.eqLo = get("lo"); p.eqMid = get("mid"); p.eqHi = get("hi"); p.colour = get("colour"); p.masterDb = get("master");
    for (int k = 0; k < 3; ++k)
    {
        const juce::String s(k + 1);
        p.fxType[k] = (int) get(("fx" + s + "t").toRawUTF8()); p.fxOn[k] = get(("fx" + s + "o").toRawUTF8()) > 0.5f;
        p.fxP1[k] = get(("fx" + s + "p1").toRawUTF8()); p.fxP2[k] = get(("fx" + s + "p2").toRawUTF8());
    }
    p.purrOn = get("purron") > 0.5f; p.purrDrive = get("purrdrive"); p.purrXs = get("purrxs"); p.purrFuzz = get("purrfuzz"); p.purrMix = get("purrmix");
    p.chainMix = get("chain"); p.echoBeats = get("echo");
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) p.hostBpm = *bpm;

    float* outL = buf.getWritePointer(0);
    float* outR = buf.getWritePointer(chans > 1 ? 1 : 0);
    engine.process(p, n, inL.data(), inR.data(), outL, outR);
}

void ScratchFxProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void ScratchFxProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size)) if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
