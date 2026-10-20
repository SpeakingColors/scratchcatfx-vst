#include "ScratchFx.h"
#include "ScratchFxEditor.h"

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using juce::NormalisableRange;

namespace
{
const char* kHotIds[8] = { "hot1", "hot2", "hot3", "hot4", "hot5", "hot6", "hot7", "hot8" };
}

Layout ScratchFxProcessor::layout()
{
    Layout l;
    auto f = [&l](const char* id, const char* name, float lo, float hi, float def, const char* unit = "") {
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, NormalisableRange<float>(lo, hi), def, juce::AudioParameterFloatAttributes().withLabel(unit)));
    };
    auto b = [&l](const juce::String& id, const juce::String& name, bool def = false) { l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 }, name, def)); };
    auto c = [&l](const juce::String& id, const juce::String& name, const juce::StringArray& items, int def) { l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id, 1 }, name, items, def)); };

    // deck: gestures and buttons (automate or MIDI-map these in the host; switches act on the rising edge)
    b("touch", "Hand on platter"); f("speed", "Platter speed", -4.f, 4.f, 0.f, "x");
    b("play", "Play / pause"); b("cue", "Cue"); b("brake", "Brake"); b("backspin", "Backspin"); b("live", "Go live");
    for (int i = 0; i < 8; ++i) b(kHotIds[i], "Hot cue " + juce::String(i + 1));
    b("loopin", "Loop in"); b("loopout", "Loop out"); b("loopexit", "Loop exit");
    b("roll", "Roll"); b("censor", "Censor"); b("stutter", "Stutter"); b("trans", "Transform");
    b("sync", "Sync to host BPM"); b("keylock", "Key lock"); b("quant", "Quantize", true); b("vinyl", "Vinyl feel", true);
    f("tempo", "Tempo fader", -1.f, 1.f, 0.f); c("range", "Tempo range", { "8 %", "16 %", "50 %" }, 1);
    f("hostbpm", "Host BPM (when the host gives none)", 40.f, 240.f, 120.f, "BPM"); c("capbars", "Capture length", { "1 bar", "2 bars", "4 bars", "8 bars" }, 2);
    // mixer
    f("trim", "Trim", -12.f, 12.f, 0.f, "dB");
    f("hi", "High", -24.f, 12.f, 0.f, "dB"); f("mid", "Mid", -24.f, 12.f, 0.f, "dB"); f("lo", "Low", -24.f, 12.f, 0.f, "dB");
    f("colour", "Colour", -1.f, 1.f, 0.f);
    b("killhi", "Kill high"); b("killmid", "Kill mid"); b("killlo", "Kill low");
    f("fader", "Deck fader", -60.f, 2.f, 0.f, "dB"); f("xf", "Crossfader", -1.f, 1.f, 0.f);
    c("xcurve", "Crossfader curve", { "Smooth", "Sharp", "Constant" }, 0); c("fxtarget", "FX on", { "Scratch deck", "Master" }, 0);
    f("master", "Master", -24.f, 6.f, 0.f, "dB");
    // effects
    const juce::StringArray names { "Filter", "Echo", "Reverb", "Flanger", "Crush" };
    static const float defP1[3] = { 0.25f, 0.5f, 0.5f }, defP2[3] = { 1.f, 0.4f, 0.3f };
    for (int k = 0; k < 3; ++k)
    {
        const juce::String s(k + 1);
        c("fx" + s + "t", "FX " + s + " type", names, k);
        b("fx" + s + "o", "FX " + s + " on");
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "fx" + s + "p1", 1 }, "FX " + s + " amount", 0.f, 1.f, defP1[k]));
        l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "fx" + s + "p2", 1 }, "FX " + s + " dry/wet", 0.f, 1.f, defP2[k]));
    }
    // PurrOne colour
    b("purron", "PurrOne colour on"); f("purrdrive", "PurrOne Drive", 0.f, 1.f, 0.f); f("purrxs", "PurrOne Transistor", 0.f, 1.f, 0.f); f("purrfuzz", "PurrOne Fuzz", 0.f, 1.f, 0.f); f("purrmix", "PurrOne mix", 0.f, 1.f, 1.f);
    return l;
}

ScratchFxProcessor::ScratchFxProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", layout())
{
    formats.registerBasicFormats();
    for (auto& h : restoreHot) h = -1.0;
    analysis = "DAW input is live. Press Capture to grab the last bars, or load a file.";
}
ScratchFxProcessor::~ScratchFxProcessor() { cancelPendingUpdate(); pool.removeAllJobs(true, 4000); }

juce::AudioProcessorEditor* ScratchFxProcessor::createEditor() { return new ScratchFxEditor(*this); }

void ScratchFxProcessor::prepareToPlay(double sr, int mb)
{
    {
        const juce::SpinLock::ScopedLockType l(ringLock);
        fs = sr; maxBlock = std::max(mb, 64);
        engine.prepare(sr);
    }
    inL.assign((size_t) maxBlock + 8, 0.f); inR.assign((size_t) maxBlock + 8, 0.f);
    wasTouch = wasBrake = wasSpin = wasLive = wasPlay = wasCue = wasLoopIn = wasLoopOut = wasLoopExit = false; wasMove = 0;
    for (auto& w : wasHot) w = false;
    view.src.store(0); view.live.store(true);
    // a track loaded before prepare (state restore) is picked up by the audio thread on the first block
    const juce::SpinLock::ScopedLockType l(trackLock);
    if (shown != nullptr && pending == nullptr) { pending = shown; }
}

// ---------------------------------------------------------------- commands from the editor
void ScratchFxProcessor::post(const scfxui::Cmd& c)
{
    const auto w = cmdFifo.write(1);
    if (w.blockSize1 > 0) cmdBuf[(size_t) w.startIndex1] = c; else if (w.blockSize2 > 0) cmdBuf[(size_t) w.startIndex2] = c;
}
void ScratchFxProcessor::applyCmd(const scfxui::Cmd& c)
{
    auto& d = engine.deck;
    switch (c.type)
    {
        case scfxui::kJog:
            if (c.i != 0) d.setJog(true, c.a);
            else { const bool wasTouching = d.touching(); d.setJog(false, 0.0); if (wasTouching && d.isLiveInput()) d.setPlaying(true); }   // letting go plays on at normal speed
            break;
        case scfxui::kBeatLoop: d.beatLoop(c.a); break;
        case scfxui::kLoopExit: d.loopExit(); break;
        case scfxui::kHotClear: d.clearHotCue(c.i); break;
        case scfxui::kSeek: d.seekSeconds(c.a); break;
        case scfxui::kLive: d.useLiveInput(true); view.src.store(0); break;
        default: break;
    }
}
void ScratchFxProcessor::setParam(const juce::String& id, float real)
{
    if (auto* p = apvts.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(real)); p->endChangeGesture(); }
}
void ScratchFxProcessor::handleAsyncUpdate()
{
    const juce::SpinLock::ScopedLockType l(trackLock);
    retired.reset();     // tracks the audio thread let go of are freed here, never on the audio thread
}

// ---------------------------------------------------------------- audio
void ScratchFxProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    if ((int) inL.size() < n) { inL.resize((size_t) n + 8); inR.resize((size_t) n + 8); }
    const int chans = buf.getNumChannels();
    if (chans < 1) return;
    std::copy(buf.getReadPointer(0), buf.getReadPointer(0) + n, inL.begin());
    std::copy(buf.getReadPointer(chans > 1 ? 1 : 0), buf.getReadPointer(chans > 1 ? 1 : 0) + n, inR.begin());

    // 1. commands from the editor
    {
        int s1, n1, s2, n2; cmdFifo.prepareToRead(cmdFifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) applyCmd(cmdBuf[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) applyCmd(cmdBuf[(size_t) (s2 + i)]);
        cmdFifo.finishedRead(n1 + n2);
    }
    // 2. a new track (loaded file, capture, or a source switch)
    {
        const juce::SpinLock::ScopedTryLockType l(trackLock);
        if (l.isLocked() && pending != nullptr)
        {
            retired = engine.deck.setTrack(pending);
            if (haveRestore) { for (int i = 0; i < 8; ++i) if (restoreHot[i] >= 0) engine.deck.setHotCuePos(i, restoreHot[i] * pending->sr); haveRestore = false; }
            view.src.store(pendingKind < 0 ? 2 : pendingKind);
            pending.reset(); triggerAsyncUpdate();
        }
    }
    // 3. the host-automatable switches (rising and falling edges)
    auto on = [this](const char* id) { return get(id) > 0.5f; };
    auto& dk = engine.deck;
    const bool touch = on("touch"), brake = on("brake"), spin = on("backspin"), live = on("live"), play = on("play"), cue = on("cue");
    if (live && !wasLive) { if (dk.isLiveInput()) engine.goLive(); else { dk.useLiveInput(true); view.src.store(0); } }     // from a loop or file: back to the DAW input
    if (brake && !wasBrake) engine.brake();
    if (spin && !wasSpin) engine.backspin();
    if (touch) engine.touch(true, (double) get("speed"));
    else if (wasTouch) { engine.touch(false, 0.0); if (dk.isLiveInput()) dk.setPlaying(true); }
    if (play && !wasPlay) dk.togglePlay();
    if (cue != wasCue) dk.cue(cue);
    for (int i = 0; i < 8; ++i) { const bool h = on(kHotIds[i]); if (h && !wasHot[i]) dk.hotCue(i, true); wasHot[i] = h; }
    const bool li = on("loopin"), lo = on("loopout"), lx = on("loopexit");
    if (li && !wasLoopIn) dk.loopIn(); if (lo && !wasLoopOut) dk.loopOut(); if (lx && !wasLoopExit) dk.loopExit();
    const int move = on("roll") ? 1 : on("censor") ? 2 : on("stutter") ? 3 : on("trans") ? 4 : 0;
    if (move != wasMove)
    {
        if (wasMove != 0) dk.endMove();
        if (move == 1) dk.startMove(scfx::Move::Roll, 0.125); else if (move == 2) dk.startMove(scfx::Move::Censor, 0.25);
        else if (move == 3) dk.startMove(scfx::Move::Stutter, 0.0625); else if (move == 4) dk.startMove(scfx::Move::Transform, 0.25);
    }
    wasTouch = touch; wasBrake = brake; wasSpin = spin; wasLive = live; wasPlay = play; wasCue = cue; wasLoopIn = li; wasLoopOut = lo; wasLoopExit = lx; wasMove = move;

    // 4. tempo: the host's when it gives one, else the Host BPM knob
    double host = (double) get("hostbpm"); bool hostGives = false; double ppq = -1.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition())
    {
        if (auto bpm = pos->getBpm()) { if (*bpm > 20.0) { host = *bpm; hostGives = true; } }
        if (auto pp = pos->getPpqPosition()) if (pos->getIsPlaying()) ppq = *pp + (double) n / fs * host / 60.0;     // musical position at the end of this block
    }
    static const float ranges[3] = { 0.08f, 0.16f, 0.5f };
    dk.setPitch(get("tempo"), ranges[juce::jlimit(0, 2, (int) std::lround(get("range")))]);
    dk.setKeyLock(on("keylock")); dk.setQuant(on("quant")); dk.setSync(on("sync"), host); dk.setVinylTime(on("vinyl") ? 0.06f : 0.012f);

    scfx::FxParams p;
    p.trimDb = get("trim"); p.eqLo = get("lo"); p.eqMid = get("mid"); p.eqHi = get("hi"); p.colour = get("colour"); p.masterDb = get("master");
    p.killLo = on("killlo"); p.killMid = on("killmid"); p.killHi = on("killhi");
    p.deckFader = get("fader") <= -59.5f ? 0.f : juce::Decibels::decibelsToGain(get("fader"));
    p.xf = get("xf"); p.xCurve = (int) std::lround(get("xcurve")); p.fxTarget = (int) std::lround(get("fxtarget"));
    for (int k = 0; k < 3; ++k)
    {
        const juce::String s(k + 1);
        p.fxType[k] = 1 + juce::jlimit(0, 4, (int) std::lround(get(("fx" + s + "t").toRawUTF8()))); p.fxOn[k] = get(("fx" + s + "o").toRawUTF8()) > 0.5f;
        p.fxP1[k] = get(("fx" + s + "p1").toRawUTF8()); p.fxP2[k] = get(("fx" + s + "p2").toRawUTF8());
    }
    p.chainMix = 1.f;
    p.purrOn = on("purron"); p.purrDrive = get("purrdrive"); p.purrXs = get("purrxs"); p.purrFuzz = get("purrfuzz"); p.purrMix = get("purrmix");
    p.hostBpm = host;

    float* outL = buf.getWritePointer(0);
    float* outR = buf.getWritePointer(chans > 1 ? 1 : 0);
    engine.process(p, n, inL.data(), inR.data(), outL, outR);

    // 5. tell the editor
    view.pos.store(dk.positionSeconds()); view.len.store(dk.lengthSeconds()); view.bpm.store(dk.currentBpm()); view.cue.store(dk.cueSeconds());
    view.loopA.store(dk.loopStartSec()); view.loopB.store(dk.loopEndSec()); view.phase.store((float) dk.beatPhase()); view.behind.store(dk.behindSeconds());
    view.playing.store(dk.isPlaying()); view.live.store(dk.inLiveMode()); view.loopOn.store(dk.loopActive()); view.touching.store(dk.touching());
    view.tempoPct.store((dk.tempo() - 1.0) * 100.0); view.hostBpm.store(host); view.hostHasBpm.store(hostGives); view.synced.store(dk.isSyncOn());
    for (int i = 0; i < 8; ++i) { const double h = dk.hotCuePos(i); view.hot[(size_t) i].store(h < 0 || !dk.track() ? -1.0 : h / dk.track()->sr); }
    { const juce::SpinLock::ScopedTryLockType l(trackLock); if (l.isLocked()) { stampPpq = ppq; stampHead = dk.headFrames(); } }
}

// ---------------------------------------------------------------- files, capture and analysis
bool ScratchFxProcessor::hasTrackKind(int kind) const
{
    const juce::SpinLock::ScopedLockType l(const_cast<juce::SpinLock&>(trackLock));
    return kind == 0 || (kind == 1 && capTrack != nullptr) || (kind == 2 && fileTrack != nullptr);
}
std::shared_ptr<const scfx::Track> ScratchFxProcessor::shownTrack() const
{
    const juce::SpinLock::ScopedLockType l(const_cast<juce::SpinLock&>(trackLock)); return shown;
}
juce::String ScratchFxProcessor::statusLine() const
{
    const juce::SpinLock::ScopedLockType l(const_cast<juce::SpinLock&>(statusLock)); return analysis;
}
juce::String ScratchFxProcessor::fileLabel() const
{
    const juce::SpinLock::ScopedLockType l(const_cast<juce::SpinLock&>(statusLock)); return fileName;
}
void ScratchFxProcessor::queueTrack(int kind, std::shared_ptr<const scfx::Track> t)
{
    { const juce::SpinLock::ScopedLockType l(trackLock); shown = t; pending = t; pendingKind = kind; if (kind == 1) capTrack = t; else if (kind == 2) fileTrack = t; }
}
void ScratchFxProcessor::selectSource(int kind)
{
    if (kind == 0)
    {
        { const juce::SpinLock::ScopedLockType l(trackLock); shown = nullptr; }
        scfxui::Cmd c; c.type = scfxui::kLive; post(c);
        const juce::SpinLock::ScopedLockType l(statusLock); analysis = "DAW input is live. Press Capture to grab the last bars, or load a file.";
        return;
    }
    std::shared_ptr<const scfx::Track> t;
    { const juce::SpinLock::ScopedLockType l(trackLock); t = kind == 1 ? capTrack : fileTrack; }
    if (t == nullptr) return;
    queueTrack(kind, t);
    const juce::SpinLock::ScopedLockType l(statusLock); analysis = juce::String(kind == 1 ? "Captured loop" : fileName) + "   " + juce::String(t->bpm, 1) + " BPM";
}

void ScratchFxProcessor::capture()
{
    const double bpm = std::max(40.0, view.hostBpm.load());
    static const int barsOf[4] = { 1, 2, 4, 8 };
    const int bars = barsOf[juce::jlimit(0, 3, (int) std::lround(get("capbars")))];
    double ppq; unsigned long long head;
    { const juce::SpinLock::ScopedLockType l(trackLock); ppq = stampPpq; head = stampHead; }
    { const juce::SpinLock::ScopedLockType l(statusLock); analysis = "Capturing " + juce::String(bars) + (bars == 1 ? " bar" : " bars") + " ..."; }
    pool.addJob([this, bpm, bars, ppq, head]
    {
        const double want = std::min(50.0, (double) bars * 4.0 * 60.0 / bpm);
        auto t = std::make_shared<scfx::Track>();
        size_t got = 0;
        { const juce::SpinLock::ScopedLockType l(ringLock); got = engine.deck.grabLast(want, *t, ppq >= 0.0 ? head : 0ull); }
        if (got < 2048) { const juce::SpinLock::ScopedLockType l(statusLock); analysis = "Nothing to capture yet. Play audio through the plugin first."; return; }
        t->bpm = bpm; t->name = "Capture";
        if (ppq >= 0.0) { const double fr = ppq - std::floor(ppq); t->firstBeat = fr > 1e-4 ? (1.0 - fr) * 60.0 / bpm : 0.0; }
        t->key = scfx::analyseKey(t->l.data(), t->r.data(), t->frames(), t->sr);
        t->peaks = scfx::makePeaks(t->l.data(), t->r.data(), t->frames(), (int) std::max<size_t>(1, t->frames() / 256));
        juce::String info = "Captured " + juce::String(bars) + (bars == 1 ? " bar" : " bars") + " at " + juce::String(bpm, 1) + " BPM";
        if (t->key.tonic >= 0) info += ", key " + juce::String(scfx::keyName(t->key)) + " (" + juce::String(scfx::camelot(t->key)) + ")";
        juce::MessageManager::callAsync([this, t, info] { finishLoad(t, 1, info); });
    });
}

void ScratchFxProcessor::loadFile(const juce::File& f)
{
    { const juce::SpinLock::ScopedLockType l(statusLock); analysis = "Loading " + f.getFileName() + " ..."; }
    apvts.state.setProperty("file", f.getFullPathName(), nullptr);
    pool.addJob([this, f]
    {
        std::unique_ptr<juce::AudioFormatReader> rd(formats.createReaderFor(f));
        if (rd == nullptr || rd->lengthInSamples < 1024)
        { const juce::SpinLock::ScopedLockType l(statusLock); analysis = "Could not read " + f.getFileName(); return; }
        const juce::int64 maxFrames = (juce::int64) (rd->sampleRate * 60.0 * 20.0);    // 20 minutes at most
        const int n = (int) std::min<juce::int64>(rd->lengthInSamples, maxFrames);
        juce::AudioBuffer<float> b((int) std::max<unsigned>(2, rd->numChannels), n);
        rd->read(&b, 0, n, 0, true, true);
        auto t = std::make_shared<scfx::Track>();
        t->sr = rd->sampleRate; t->name = f.getFileNameWithoutExtension().toStdString();
        t->l.assign(b.getReadPointer(0), b.getReadPointer(0) + n);
        t->r.assign(b.getReadPointer(rd->numChannels > 1 ? 1 : 0), b.getReadPointer(rd->numChannels > 1 ? 1 : 0) + n);
        { const juce::SpinLock::ScopedLockType l(statusLock); analysis = "Analysing " + f.getFileName() + " ..."; }
        const auto bp = scfx::analyseBpm(t->l.data(), t->r.data(), t->frames(), t->sr);
        t->bpm = bp.bpm; t->firstBeat = bp.firstBeat;
        t->key = scfx::analyseKey(t->l.data(), t->r.data(), t->frames(), t->sr);
        t->peaks = scfx::makePeaks(t->l.data(), t->r.data(), t->frames(), (int) std::max<size_t>(1, t->frames() / 256));
        juce::String info = "Analyzed: " + juce::String(t->bpm, 1) + " BPM";
        if (t->key.tonic >= 0) info += ", key " + juce::String(scfx::keyName(t->key)) + " (" + juce::String(scfx::camelot(t->key)) + ")";
        const juce::String nm = f.getFileNameWithoutExtension();
        juce::MessageManager::callAsync([this, t, info, nm] { { const juce::SpinLock::ScopedLockType l(statusLock); fileName = nm; } finishLoad(t, 2, info); });
    });
}
void ScratchFxProcessor::finishLoad(std::shared_ptr<scfx::Track> t, int kind, const juce::String& info)
{
    { const juce::SpinLock::ScopedLockType l(statusLock); analysis = info; }
    queueTrack(kind, t);
}

// ---------------------------------------------------------------- state
void ScratchFxProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::String hs; for (int i = 0; i < 8; ++i) hs += (i ? "," : "") + juce::String(view.hot[(size_t) i].load(), 4);
    apvts.state.setProperty("hot", hs, nullptr);
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void ScratchFxProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || ! xml->hasTagName(apvts.state.getType())) return;
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    juce::File f(apvts.state.getProperty("file", "").toString());
    if (f.existsAsFile())
    {
        auto hs = juce::StringArray::fromTokens(apvts.state.getProperty("hot", "").toString(), ",", "");
        { const juce::SpinLock::ScopedLockType l(trackLock); for (int i = 0; i < 8; ++i) restoreHot[i] = i < hs.size() ? hs[i].getDoubleValue() : -1.0; haveRestore = hs.size() > 0; }
        loadFile(f);
    }
}
