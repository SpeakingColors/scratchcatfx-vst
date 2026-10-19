// Scratch Cat FX: the effect engine. DAW input -> record buffer -> scratch deck -> trim, EQ, colour -> 3 slot FX chain -> master.
// With nothing pressed the deck is in live mode and the sound passes straight through (zero latency, bit exact at neutral).
#pragma once
#include "ScfxDeck.h"
#include <atomic>

namespace scfx {

struct FxParams {
    float trimDb = 0, eqLo = 0, eqMid = 0, eqHi = 0, colour = 0, masterDb = 0;
    int fxType[3] = { FxFilter, FxEcho, FxReverb }; bool fxOn[3] = { false, false, false };
    float fxP1[3] = { 0.5f, 0.45f, 0.6f }, fxP2[3] = { 0.5f, 0.4f, 0.25f };
    float chainMix = 0.6f, echoBeats = 0.75f;
    double hostBpm = 120.0;
};

class Engine {
public:
    Deck deck;
    std::atomic<float> peakIn { 0.f }, peakOut { 0.f };

    void prepare(double sr) {
        fs = sr; deck.prepare(sr); strip.prepare(sr); for (auto& f : fx) f.prepare(sr);
        deck.useLiveInput(true); trimS = masterS = 1.f;
    }
    // Scratch gestures. touch = hand on the platter; speed in "normal speeds" (1 = forward at normal speed, -2 = backwards twice as fast).
    void touch(bool on, double speed) { deck.setJog(on, speed * Deck::kPlatterRevPerSec); }
    void brake() { deck.brake(); }
    void backspin() { deck.backspin(); }
    void goLive() { deck.goLive(); }
    bool isLive() const { return deck.inLiveMode(); }

    // In place safe: outL/outR may be the same buffers as inL/inR (the deck copies its input first).
    void process(const FxParams& p, int n, const float* inL, const float* inR, float* outL, float* outR) {
        if ((int) tmpL.size() < n) { tmpL.resize((size_t) n); tmpR.resize((size_t) n); }
        float pin = 0, pout = 0;
        for (int i = 0; i < n; ++i) pin = std::max(pin, std::max(std::fabs(inL[i]), std::fabs(inR ? inR[i] : inL[i])));
        deck.process(tmpL.data(), tmpR.data(), n, inL, inR);
        strip.setParams(p.eqLo, p.eqMid, p.eqHi, p.colour);
        const float sm = 1.f - std::exp(-1.f / (0.005f * (float) fs));
        for (int i = 0; i < n; ++i) {
            trimS += (dbToLin(p.trimDb) - trimS) * sm;
            outL[i] = strip.tick(0, tmpL[i] * trimS); outR[i] = strip.tick(1, tmpR[i] * trimS);
        }
        for (int k = 0; k < 3; ++k) {
            if (fx[k].type != p.fxType[k]) { fx[k].type = p.fxType[k]; fx[k].prepare(fs); }
            fx[k].on = p.fxOn[k]; fx[k].p1 = p.fxP1[k]; fx[k].p2 = p.fxP2[k] * p.chainMix;
            fx[k].bpm = (float) std::max(30.0, p.hostBpm); fx[k].echoBeats = p.echoBeats;
            fx[k].process(outL, outR, n);
        }
        for (int i = 0; i < n; ++i) {
            masterS += (dbToLin(p.masterDb) - masterS) * sm;
            outL[i] = softClip(outL[i] * masterS); outR[i] = softClip(outR[i] * masterS);
            pout = std::max(pout, std::max(std::fabs(outL[i]), std::fabs(outR[i])));
        }
        auto fall = [n, this](std::atomic<float>& a, float v) { const float old = a.load() * std::pow(0.2f, (float) n / (float) fs); a.store(std::max(old, v)); };
        fall(peakIn, pin); fall(peakOut, pout);
    }
    static float softClip(float x) {          // only acts above 0.95, normal levels stay bit exact
        const float a = std::fabs(x); if (a <= 0.95f) return x;
        const float s = x < 0 ? -1.f : 1.f; return s * (0.95f + 0.05f * std::tanh((a - 0.95f) / 0.05f));
    }
    ChannelStrip strip;
    FxSlot fx[3];
private:
    double fs = 44100;
    std::vector<float> tmpL, tmpR;
    float trimS = 1.f, masterS = 1.f;
};
}
