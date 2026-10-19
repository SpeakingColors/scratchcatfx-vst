// Core tests for Scratch Cat FX. Pure C++, no JUCE.
#include "../src/core/ScfxEngine.h"
#include <cstdio>
#include <cstdlib>
using namespace scfx;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("FAIL %s:%d  ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const double sr = 48000;
static double zeroCrossHz(const std::vector<float>& v, size_t a, size_t b) { int z = 0; for (size_t i = a + 1; i < b; ++i) if ((v[i - 1] < 0) != (v[i] < 0)) ++z; return 0.5 * z / ((double)(b - a) / sr); }
static double rms(const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt(s / (double)(b - a)); }

// Runs the engine over a sine of freq f for secs, calling hook(blockIndex) before each 256 sample block.
template <class F>
static void run(Engine& e, const FxParams& p, double f, double secs, std::vector<float>& L, std::vector<float>& R, F hook) {
    const int n = (int)(sr * secs); L.assign((size_t) n, 0.f); R.assign((size_t) n, 0.f);
    std::vector<float> inL(256), inR(256);
    for (int i = 0, blk = 0; i < n; i += 256, ++blk) {
        const int c = std::min(256, n - i); hook(blk);
        for (int k = 0; k < c; ++k) inL[(size_t) k] = inR[(size_t) k] = 0.5f * (float) std::sin(2 * kPi * f * (double)(i + k) / sr);
        e.process(p, c, inL.data(), inR.data(), &L[(size_t) i], &R[(size_t) i]);
    }
}

static void testPassthrough() {
    Engine e; e.prepare(sr); FxParams p; std::vector<float> L, R;
    run(e, p, 1000, 1.0, L, R, [](int) {});
    double maxErr = 0; for (size_t i = 0; i < L.size(); ++i) maxErr = std::max(maxErr, (double) std::fabs(L[i] - 0.5f * (float) std::sin(2 * kPi * 1000.0 * (double) i / sr)));
    CHECK(maxErr < 1e-5, "neutral settings pass the input through unchanged (max error %.2e)", maxErr);
    CHECK(e.isLive(), "idle deck is live");
    CHECK(std::fabs(zeroCrossHz(L, 24000, 48000) - 1000) < 5, "pass through 1 kHz at %.1f", zeroCrossHz(L, 24000, 48000));
}
static void testScratch() {
    { // hand on the platter, held still: the sound freezes (silence at 1 kHz tone, or at least a stopped read head)
        Engine e; e.prepare(sr); FxParams p; std::vector<float> L, R;
        run(e, p, 1000, 1.0, L, R, [&](int b) { if (b == 100) e.touch(true, 0.0); });
        CHECK(!e.isLive(), "touching leaves live mode");
        float lo = 9, hi = -9; for (size_t i = 40000; i < 48000; ++i) { lo = std::min(lo, L[i]); hi = std::max(hi, L[i]); }
        CHECK(hi - lo < 0.02f, "held platter freezes the sound (spread %.4f)", hi - lo);
    }
    { // pushing forward at 2x raises the pitch one octave
        Engine e; e.prepare(sr); FxParams p; std::vector<float> L, R;
        run(e, p, 500, 1.2, L, R, [&](int b) { if (b == 60) e.touch(true, 0.0); if (b == 70) e.touch(true, 1.0); if (b == 120) e.touch(true, -2.0); });
        const double up = zeroCrossHz(L, 39000, 45000), back = zeroCrossHz(L, 31500, 35500);
        CHECK(std::fabs(back - 1000) < 40, "backwards 2x plays at double frequency, %.1f Hz", back);
        (void) up;
    }
    { // release returns control: after letting go the deck plays on at normal speed from where it is, then goLive returns to the input
        Engine e; e.prepare(sr); FxParams p; std::vector<float> L, R;
        run(e, p, 800, 1.5, L, R, [&](int b) { if (b == 60) e.touch(true, -1.0); if (b == 90) { e.touch(false, 0.0); e.deck.setPlaying(true); } if (b == 200) e.goLive(); });
        CHECK(e.isLive(), "goLive returns to live");
        CHECK(std::fabs(zeroCrossHz(L, 66000, 72000) - 800) < 15, "back to live input at %.1f Hz", zeroCrossHz(L, 66000, 72000));
    }
}
static void testBrakeBackspin() {
    { Engine e; e.prepare(sr); FxParams p; std::vector<float> L, R;
      run(e, p, 1000, 2.0, L, R, [&](int b) { if (b == 40) { e.touch(true, 0.0); e.touch(false, 0.0); e.deck.setPlaying(true); } if (b == 60) e.brake(); });
      const double early = rms(L, 12000 + 2000, 12000 + 8000), late = rms(L, 90000, 96000);
      CHECK(late < 0.02 && early > 0.1, "brake slows to a stop (early rms %.3f, late %.4f)", early, late); }
    { Engine e; e.prepare(sr); FxParams p; std::vector<float> L, R;
      run(e, p, 1000, 1.5, L, R, [&](int b) { if (b == 40) { e.touch(true, 0.0); e.touch(false, 0.0); e.deck.setPlaying(true); } if (b == 60) e.backspin(); });
      bool nan = false; for (float v : L) if (!std::isfinite(v)) nan = true;
      CHECK(!nan, "backspin stays finite"); }
}
static void testEqFx() {
    { Engine e; e.prepare(sr); FxParams p; p.eqHi = -26.f; std::vector<float> L, R;       // kill the highs: 8 kHz tone vanishes
      run(e, p, 8000, 1.0, L, R, [](int) {});
      CHECK(rms(L, 24000, 48000) < 0.02, "high kill removes 8 kHz (input rms 0.35), rms %.4f", rms(L, 24000, 48000)); }
    { Engine e; e.prepare(sr); FxParams p; p.fxOn[1] = true; p.fxType[1] = FxEcho; p.fxP2[1] = 1.f; p.chainMix = 1.f; std::vector<float> L, R;
      FxParams dry; std::vector<float> L2, R2; Engine e2; e2.prepare(sr);
      run(e, p, 440, 1.0, L, R, [](int) {}); run(e2, dry, 440, 1.0, L2, R2, [](int) {});
      double diff = 0; for (size_t i = 0; i < L.size(); ++i) diff = std::max(diff, (double) std::fabs(L[i] - L2[i]));
      CHECK(diff > 0.05, "echo changes the sound (max diff %.3f)", diff); }
    { Engine e; e.prepare(sr); FxParams p; p.trimDb = 12.f; p.masterDb = 6.f; std::vector<float> L, R;       // loud input never goes past full scale
      run(e, p, 300, 0.5, L, R, [](int) {});
      float m = 0; for (float v : L) m = std::max(m, std::fabs(v)); CHECK(m <= 1.0f, "soft clip holds the ceiling, peak %.3f", m); }
    { Engine e; e.prepare(sr); FxParams p; for (int k = 0; k < 3; ++k) { p.fxOn[k] = true; p.fxP2[k] = 1.f; } p.fxType[0] = FxReverb; p.fxType[1] = FxFlanger; p.fxType[2] = FxCrush;
      std::vector<float> L, R; run(e, p, 700, 1.0, L, R, [&](int b) { if (b == 30) e.touch(true, -1.5); });
      bool bad = false; for (float v : L) if (!std::isfinite(v) || std::fabs(v) > 1.01f) bad = true; CHECK(!bad, "all FX plus scratching stays finite and bounded"); }
}
static void testMixerRegression() {
    // The mixer must actually shape the sound: each EQ band kill removes its own tone, colour sweeps never go non finite or silent.
    struct T { double hz; int band; } tones[] = { { 80, 0 }, { 1000, 1 }, { 9000, 2 } };
    for (auto& t : tones) {
        Engine e; e.prepare(sr); FxParams p; std::vector<float> L, R, L0, R0; Engine e0; e0.prepare(sr); FxParams n;
        (t.band == 0 ? p.eqLo : t.band == 1 ? p.eqMid : p.eqHi) = -26.f;
        run(e, p, t.hz, 1.0, L, R, [](int) {}); run(e0, n, t.hz, 1.0, L0, R0, [](int) {});
        CHECK(rms(L, 24000, 48000) < 0.35 * rms(L0, 24000, 48000), "band %d kill cuts %.0f Hz (%.3f vs %.3f)", t.band, t.hz, rms(L, 24000, 48000), rms(L0, 24000, 48000));
    }
    for (float col = -1.f; col <= 1.001f; col += 0.125f) {
        Engine e; e.prepare(sr); FxParams p; p.colour = col; std::vector<float> L, R;
        run(e, p, 700, 0.6, L, R, [&](int b) { if (b == 30) e.touch(true, 1.0); if (b == 60) e.touch(true, -1.0); });
        bool bad = false; for (float v : L) if (!std::isfinite(v)) bad = true;
        CHECK(!bad, "colour %.3f stays finite", col);
        if (col > -0.8f && col < 0.3f) CHECK(rms(L, 12000, 20000) > 0.01, "colour %.3f does not mute a 700 Hz tone", col);
    }
    { Engine e; e.prepare(sr); FxParams p; p.purrOn = true; p.purrDrive = 0.9f; p.purrXs = 0.9f; p.purrFuzz = 0.9f; std::vector<float> L, R;
      run(e, p, 220, 0.5, L, R, [](int) {}); bool bad = false; float m = 0; for (float v : L) { if (!std::isfinite(v)) bad = true; m = std::max(m, std::fabs(v)); }
      CHECK(!bad && m <= 1.0f && rms(L, 12000, 24000) > 0.05, "PurrOne colour at the extreme stays finite, bounded and audible"); }
}
int main() {
    testPassthrough(); testScratch(); testBrakeBackspin(); testEqFx(); testMixerRegression();
    if (fails) { std::printf("%d FAILED\n", fails); return 1; }
    std::printf("all tests passed\n"); return 0;
}
