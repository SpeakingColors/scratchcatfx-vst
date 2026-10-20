#include "ScratchFxEditor.h"
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>
#include <catui/Logo.h>

using juce::Colour; using juce::Rectangle; using juce::String;
namespace
{
// Canvas: the approved mockup window, 1700 x 900 with 30 px wood cheeks. Every position below is in that canvas.
constexpr int kBaseW = 1700, kBaseH = 900, kMinW = 900, kMaxW = 3400, kCheek = 30;
const Colour kGround(0xff15161a), kPanel(0xff1c1d22), kLine(0xff2b2d34), kLine2(0xff3a3d46), kInk(0xfff4f5f7), kDim(0xffa9adb8), kDisp(0xff06080a), kGrid(0xff1b2523);
const Colour kOrange(0xffFF8A3D), kPeach(0xffF6A970), kTeal(0xff4FD1C5), kYellow(0xffF2D16B), kGreen(0xff7FD48A), kPink(0xffF28BA8), kBtn(0xff202228), kSeg(0xff94938e), kScale(0xff8d93e0);
const Colour kPad[8] = { Colour(0xffF28BA8), Colour(0xffF2D16B), Colour(0xff4FD1C5), Colour(0xffB99BEA), Colour(0xffF6A970), Colour(0xff7FB6F2), Colour(0xff8FD48A), Colour(0xffECEAE4) };
const char* kHotIds[8] = { "hot1", "hot2", "hot3", "hot4", "hot5", "hot6", "hot7", "hot8" };
enum Style { sNormal = 0, sOutline, sSegment, sKill };

juce::Font mkFont(float size, bool bold = false, float kern = 0.f, bool mono = false)
{
    juce::Font f(mono ? juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain)
                      : juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    if (kern > 0.f) f.setExtraKerningFactor(kern);
    return f;
}
void txt(juce::Graphics& g, const String& s, Rectangle<int> r, float size, Colour c, juce::Justification j = juce::Justification::centredLeft, bool bold = false, float kern = 0.1f, bool mono = false)
{
    g.setColour(c); g.setFont(mkFont(size, bold, kern, mono)); g.drawText(s, r, j, false);
}
String fmtDb(double v) { return String(v, 1) + " dB"; }
String fmtPct(double v) { return String((int) std::lround(v * 100.0)) + " %"; }

// ---- look and feel ----
class FxLAF : public catui::CatLookAndFeel
{
public:
    FxLAF()
    {
        setColour(juce::PopupMenu::backgroundColourId, kPanel); setColour(juce::PopupMenu::textColourId, kInk);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kOrange.darker(0.3f)); setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::ComboBox::textColourId, kInk); setColour(juce::TooltipWindow::backgroundColourId, Colour(0xff0a0a0c)); setColour(juce::TooltipWindow::textColourId, kInk);
        setColour(juce::TooltipWindow::outlineColourId, kLine2);
        setColour(juce::AlertWindow::backgroundColourId, kPanel); setColour(juce::AlertWindow::textColourId, kInk); setColour(juce::AlertWindow::outlineColourId, kLine2);
        setColour(juce::TextEditor::backgroundColourId, kDisp); setColour(juce::TextEditor::textColourId, kInk); setColour(juce::TextEditor::outlineColourId, kLine2);
    }
    static int styleOf(juce::Component& c) { return (int) c.getProperties().getWithDefault("style", 0); }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const Colour& base, bool hl, bool down) override
    {
        const int st = styleOf(b); auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        const bool on = b.getToggleState(); const float rad = st == sSegment ? 0.f : 4.f;
        Colour fill = kBtn;
        if (on && (st == sNormal || st == sSegment || st == sKill)) fill = base;
        else if (on && st == sOutline) fill = Colour(0xff2a1d14);
        if (down) fill = fill.brighter(0.25f); else if (hl) fill = fill.brighter(0.12f);
        g.setColour(fill); g.fillRoundedRectangle(r, rad);
        if (st == sSegment) { g.setColour(kSeg.withAlpha(0.8f)); if (!(b.getConnectedEdgeFlags() & juce::Button::ConnectedOnRight)) {} g.drawVerticalLine(b.getWidth() - 1, 0.f, (float) b.getHeight()); }
        else { g.setColour(on && st == sOutline ? base : (on ? base : kLine2)); g.drawRoundedRectangle(r, rad, 1.2f); }
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const int st = styleOf(b); const bool on = b.getToggleState();
        Colour c = kInk; if (on && st != sOutline) c = Colour(0xff0b0b0d); else if (on) c = b.findColour(juce::TextButton::buttonOnColourId);
        if (!b.isEnabled()) c = c.withAlpha(0.4f);
        g.setColour(c); g.setFont(mkFont(13.f, true, 0.06f)); g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(2, 0), juce::Justification::centred, 1);
    }
    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
        g.setColour(kDisp); g.fillRoundedRectangle(r, 4.f); g.setColour(kLine2); g.drawRoundedRectangle(r, 4.f, 1.2f);
        juce::Path p; const float cx = (float) w - 12.f, cy = (float) h * 0.5f; p.addTriangle(cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour(kDim); g.fillPath(p);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return mkFont(13.f, true, 0.04f); }
    void positionComboBoxText(juce::ComboBox& box, juce::Label& l) override { l.setBounds(8, 1, box.getWidth() - 26, box.getHeight() - 2); l.setFont(getComboBoxFont(box)); }
    int getSliderThumbRadius(juce::Slider& s) override { return s.isHorizontal() ? 12 : 10; }
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s) override
    {
        const Colour c = s.findColour(juce::Slider::thumbColourId);
        if (s.getProperties().contains("disp"))     // synced tempo: show where the deck really is
        {
            const float n = juce::jlimit(0.f, 1.f, (float) (double) s.getProperties()["disp"]);
            if (st == juce::Slider::LinearVertical) pos = (float) (y + h) - 10.f - n * (float) (h - 20); else pos = (float) x + 12.f + n * (float) (w - 24);
        }
        if (st == juce::Slider::LinearVertical)
        {
            const float cx = (float) x + (float) w * 0.5f;
            g.setColour(Colour(0xff101216)); g.fillRoundedRectangle(cx - 3.f, (float) y + 6.f, 6.f, (float) h - 12.f, 3.f);
            g.setColour(kLine); g.drawRoundedRectangle(cx - 3.f, (float) y + 6.f, 6.f, (float) h - 12.f, 3.f, 1.f);
            Rectangle<float> th(cx - 19.f, pos - 10.f, 38.f, 20.f); g.setColour(Colour(0xffe8e9ec)); g.fillRoundedRectangle(th, 3.f);
            g.setColour(c); g.fillRect(th.getX() + 4, th.getCentreY() - 1.f, th.getWidth() - 8, 2.f);
        }
        else
        {
            const float cy = (float) y + (float) h * 0.5f;
            g.setColour(Colour(0xff101216)); g.fillRoundedRectangle((float) x + 4.f, cy - 3.f, (float) w - 8.f, 6.f, 3.f);
            g.setColour(kLine); g.drawRoundedRectangle((float) x + 4.f, cy - 3.f, (float) w - 8.f, 6.f, 3.f, 1.f);
            Rectangle<float> th(pos - 12.f, cy - 13.f, 24.f, 26.f); g.setColour(Colour(0xffe8e9ec)); g.fillRoundedRectangle(th, 3.f);
            g.setColour(c); g.fillRect(th.getCentreX() - 1.f, th.getY() + 4.f, 2.f, th.getHeight() - 8.f);
        }
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(1.f); const float d = juce::jmin(r.getWidth(), r.getHeight()); r = r.withSizeKeepingCentre(d, d);
        const float ang = a0 + pos * (a1 - a0); const Colour c = s.findColour(juce::Slider::rotarySliderFillColourId);
        g.setColour(Colour(0xff23252c)); g.fillEllipse(r.reduced(d * 0.08f));
        const float rad = d * 0.5f - 1.5f;
        juce::Path bg; bg.addCentredArc(r.getCentreX(), r.getCentreY(), rad, rad, 0.f, a0, a1, true);
        g.setColour(Colour(0xff2e3139)); g.strokePath(bg, juce::PathStrokeType(d > 38 ? 3.f : 2.5f));
        const bool bip = (bool) s.getProperties().getWithDefault("bipolar", false);
        const float from = bip ? (a0 + a1) * 0.5f : a0;
        juce::Path arc; arc.addCentredArc(r.getCentreX(), r.getCentreY(), rad, rad, 0.f, juce::jmin(from, ang), juce::jmax(from, ang), true);
        g.setColour(c); g.strokePath(arc, juce::PathStrokeType(d > 38 ? 3.f : 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path t; t.addRoundedRectangle(-1.4f, -d * 0.5f + 5.f, 2.8f, d * 0.26f, 1.f);
        g.setColour(kInk); g.fillPath(t, juce::AffineTransform::rotation(ang).translated(r.getCentre()));
    }
};

// A button that reports press and release.
struct HoldButton : juce::TextButton
{
    std::function<void()> onPress, onRelease, onRight;
    explicit HoldButton(const String& t = {}) : juce::TextButton(t) {}
    void mouseDown(const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onRight) onRight(); return; } juce::TextButton::mouseDown(e); }
    void buttonStateChanged() override
    {
        const bool d = isDown();
        if (d && !was) { if (onPress) onPress(); }
        if (!d && was) { if (onRelease) onRelease(); }
        was = d;
    }
    bool was = false;
};

// A pad: hot cue colour when set, dark when empty.
struct Pad : HoldButton
{
    Colour col; bool isSet = false; bool delMode = false;
    explicit Pad(const String& t) : HoldButton(t) {}
    void paintButton(juce::Graphics& g, bool hl, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        Colour f = isSet ? col : Colour(0xff1f2125); if (down) f = f.brighter(0.25f); else if (hl) f = f.brighter(0.1f);
        g.setColour(f); g.fillRoundedRectangle(r, 4.f);
        g.setColour(isSet ? (delMode ? Colour(0xffff4060) : col.brighter(0.2f)) : kLine2); g.drawRoundedRectangle(r, 4.f, delMode && isSet ? 2.f : 1.2f);
        g.setColour(isSet ? Colour(0xff111111) : kInk); g.setFont(mkFont(15.f, true)); g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred);
    }
};

// A round transport button (CUE, PLAY).
struct RoundButton : HoldButton
{
    Colour col; bool playIcon = false; bool lit = false;
    explicit RoundButton(const String& t) : HoldButton(t) {}
    void paintButton(juce::Graphics& g, bool hl, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced(3.f); const float d = std::min(r.getWidth(), r.getHeight()); r = r.withSizeKeepingCentre(d, d);
        g.setColour(down ? col.withAlpha(0.35f) : (lit ? col.withAlpha(0.22f) : (hl ? Colour(0xff25272d) : Colour(0xff1f2125)))); g.fillEllipse(r);
        g.setColour(col); g.drawEllipse(r, 4.f);
        if (!playIcon) { g.setFont(mkFont(28.f, true, 0.05f)); g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred); }
        else
        {
            const auto c = r.getCentre(); juce::Path tri; tri.addTriangle(c.x - 22, c.y - 20, c.x - 22, c.y + 20, c.x + 4, c.y);
            g.fillPath(tri); g.fillRect(c.x + 10, c.y - 20, 8.f, 40.f); g.fillRect(c.x + 22, c.y - 20, 8.f, 40.f);
        }
    }
};

// Knob with its name and value written under or beside it.
struct Knob : juce::Component, public juce::SettableTooltipClient
{
    juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    String name; std::function<String(double)> fmt; bool right = false; int dia = 44;
    Knob()
    {
        addAndMakeVisible(s); s.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true); s.setMouseDragSensitivity(220);
        s.onValueChange = [this] { repaint(); };
    }
    void resized() override
    {
        if (right) s.setBounds(0, 0, dia, dia); else s.setBounds((getWidth() - dia) / 2, 0, dia, dia);
    }
    void paint(juce::Graphics& g) override
    {
        const String v = fmt ? fmt(s.getValue()) : String(s.getValue(), 2);
        if (right) { txt(g, name, { dia + 6, 4, getWidth() - dia - 6, 14 }, 11.f, kDim, juce::Justification::centredLeft, true, 0.12f); txt(g, v, { dia + 6, 19, getWidth() - dia - 6, 14 }, 12.f, kOrange, juce::Justification::centredLeft, false, 0.f, true); }
        else { txt(g, name, { 0, dia + 4, getWidth(), 14 }, 11.f, kDim, juce::Justification::centred, true, 0.12f); txt(g, v, { 0, dia + 19, getWidth(), 14 }, 12.f, kOrange, juce::Justification::centred, false, 0.f, true); }
    }
};
}   // namespace

// ============================================================================ the platter and the waveform
namespace
{
class Platter : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit Platter(ScratchFxProcessor& p) : proc(p) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
    ~Platter() override { stopTimer(); }
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(3.f); const float d = std::min(r.getWidth(), r.getHeight()); r = r.withSizeKeepingCentre(d, d);
        const auto c = r.getCentre();
        const double rev = proc.view.pos.load() * scfx::Deck::kPlatterRevPerSec; const float ang = (float) (rev - std::floor(rev)) * juce::MathConstants<float>::twoPi;
        g.setColour(Colour(0xff0e1014)); g.fillEllipse(r);
        g.setColour(Colour(0xff3a3d46)); g.drawEllipse(r, 3.f);
        const int n = 60;
        for (int i = 0; i < n; ++i)
        {
            const float a = ang + (float) i * juce::MathConstants<float>::twoPi / (float) n; const bool big = i % 5 == 0;
            const float r1 = d * 0.5f - 12.f, r2 = r1 - (big ? 30.f : 18.f);
            g.setColour(kOrange.withAlpha(big ? 1.f : 0.8f));
            g.drawLine(c.x + std::sin(a) * r1, c.y - std::cos(a) * r1, c.x + std::sin(a) * r2, c.y - std::cos(a) * r2, big ? 4.f : 3.f);
        }
        g.setColour(Colour(0xff2e3139)); g.drawEllipse(r.reduced(d * 0.17f), 2.f); g.setColour(Colour(0xff23252c)); g.drawEllipse(r.reduced(d * 0.27f), 1.5f);
        const float hub = d * 0.095f * 2.f;
        g.setColour(kOrange); g.fillEllipse(Rectangle<float>(hub * 2, hub * 2).withCentre(c));
        g.setColour(Colour(0xff15161a)); g.setFont(mkFont(15.f, true, 0.1f)); g.drawText("DECK", Rectangle<float>(hub * 2, 22.f).withCentre(c), juce::Justification::centred);
        g.setColour(Colour(0xfff0eee9)); g.drawLine(c.x, c.y, c.x + std::sin(ang) * (d * 0.5f - 14.f), c.y - std::cos(ang) * (d * 0.5f - 14.f), 3.f);
        if (proc.view.touching.load()) { g.setColour(kOrange.withAlpha(0.5f)); g.drawEllipse(r.reduced(1.f), 3.f); }
    }
    void mouseDown(const juce::MouseEvent& e) override { last = ang(e); acc = 0.f; scfxui::Cmd c; c.type = scfxui::kJog; c.i = 1; proc.post(c); startTimerHz(60); }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const float a = ang(e); float dlt = a - last;
        while (dlt > juce::MathConstants<float>::pi) dlt -= juce::MathConstants<float>::twoPi;
        while (dlt < -juce::MathConstants<float>::pi) dlt += juce::MathConstants<float>::twoPi;
        acc += dlt / juce::MathConstants<float>::twoPi; last = a;
    }
    void mouseUp(const juce::MouseEvent&) override { stopTimer(); scfxui::Cmd c; c.type = scfxui::kJog; c.i = 0; proc.post(c); }
private:
    float ang(const juce::MouseEvent& e) const { const auto c = getLocalBounds().toFloat().getCentre(); return std::atan2(e.position.x - c.x, -(e.position.y - c.y)); }
    void timerCallback() override { scfxui::Cmd c; c.type = scfxui::kJog; c.i = 1; c.a = (double) acc * 60.0; acc = 0.f; proc.post(c); }
    ScratchFxProcessor& proc; float last = 0.f, acc = 0.f;
};

// The scrolling waveform around the playhead. Drag it like a record to scratch.
class Wave : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit Wave(ScratchFxProcessor& p) : proc(p) { setMouseCursor(juce::MouseCursor::DraggingHandCursor); }
    ~Wave() override { stopTimer(); }
    int bars = 4;
    std::shared_ptr<const scfx::Track> trk;
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(kDisp); g.fillRect(r); g.setColour(kLine); g.drawRect(r, 1.f);
        auto& v = proc.view; const int src = v.src.load(); const double host = std::max(40.0, v.hostBpm.load());
        const double bpmNow = v.bpm.load() > 20.0 ? v.bpm.load() : host;
        windowSec = (double) bars * 4.0 * 60.0 / bpmNow;
        const float cy = r.getCentreY(), hh = r.getHeight() * 0.44f; const float W = r.getWidth();
        g.setColour(kOrange.withAlpha(0.5f)); g.drawHorizontalLine((int) cy, r.getX(), r.getRight());
        String title;
        if (src == 0 || trk == nullptr)
        {
            // live picture of the DAW input, newest on the right of "now"
            title = "DAW INPUT LIVE  " + String(host, 1) + " BPM";
            const unsigned head = proc.engine.histIdx.load(std::memory_order_acquire);
            const double secPerPx = windowSec / (double) W; const double behind = v.behind.load();
            g.setColour(kOrange.withAlpha(0.85f));
            for (int x = 0; x < (int) W; ++x)
            {
                const double tBack = behind - ((double) x - (double) W * 0.5) * secPerPx;       // seconds before "now" at this pixel
                if (tBack < 0) continue;
                const long bin = (long) head - 1 - (long) (tBack * scfx::Engine::kHistPerSec); if (bin < 0 || (long) head - bin >= scfx::Engine::kHist) continue;
                const float a = std::min(1.f, proc.engine.hist[(size_t) (bin % scfx::Engine::kHist)].load(std::memory_order_relaxed) * 1.4f);
                g.drawVerticalLine((int) r.getX() + x, cy - a * hh, cy + a * hh + 1.f);
            }
        }
        else
        {
            const double pos = v.pos.load(); const double tempo = trk->bpm > 0 ? std::max(0.1, v.bpm.load() / trk->bpm) : 1.0;
            const double srcPerPx = windowSec / (double) W * tempo; srcPx = srcPerPx;
            const int bins = (int) (trk->peaks.size() / 2); const double binSec = 256.0 / trk->sr;
            g.setColour(kOrange.withAlpha(0.8f));
            for (int x = 0; x < (int) W; ++x)
            {
                double t0 = pos + ((double) x - (double) W * 0.5) * srcPerPx;
                const double len = (double) trk->frames() / trk->sr;
                if (len > 0) { t0 = std::fmod(t0, len); if (t0 < 0) t0 += len; }       // loops repeat, so the picture wraps
                int b0 = (int) (t0 / binSec), b1 = (int) ((t0 + srcPerPx) / binSec); if (b0 >= bins) continue; b1 = std::min(bins - 1, std::max(b0, b1));
                float mn = 0, mx = 0; for (int b = b0; b <= b1; ++b) { mn = std::min(mn, trk->peaks[(size_t) b * 2]); mx = std::max(mx, trk->peaks[(size_t) b * 2 + 1]); }
                g.drawVerticalLine((int) r.getX() + x, cy - mx * hh * 1.6f, cy - mn * hh * 1.6f + 1.f);
            }
            auto xOf = [&](double sec) { return r.getX() + (float) ((double) W * 0.5 + (sec - pos) / srcPerPx); };
            const double len = (double) trk->frames() / trk->sr;
            auto drawAt = [&](double secAbs, auto&& fn) { for (int k = -1; k <= 1; ++k) fn(xOf(secAbs + (double) k * len)); };
            if (trk->bpm > 0)
            {
                const double per = 60.0 / trk->bpm; const double left = pos - (double) W * 0.5 * srcPerPx, right = pos + (double) W * 0.5 * srcPerPx;
                long k0 = (long) std::floor((left - trk->firstBeat) / per);
                for (long k = k0; trk->firstBeat + (double) k * per < right; ++k)
                {
                    const float x = xOf(trk->firstBeat + (double) k * per); if (x < r.getX() || x > r.getRight()) continue;
                    const bool bar = ((k % 4) + 4) % 4 == 0;
                    g.setColour(juce::Colours::white.withAlpha(bar ? 0.22f : 0.1f)); g.drawVerticalLine((int) x, r.getY() + (bar ? 14.f : 0.f), bar ? r.getBottom() : r.getY() + 10.f);
                    if (bar) { g.setColour(juce::Colours::white.withAlpha(0.45f)); g.setFont(mkFont(10.f)); g.drawText(String((int) (((k / 4) % 1000 + 1000) % 1000) + 1), (int) x + 2, (int) r.getBottom() - 14, 24, 12, juce::Justification::left); }
                }
            }
            if (v.loopOn.load() && v.loopA.load() >= 0 && v.loopB.load() > v.loopA.load())
            { g.setColour(kYellow.withAlpha(0.18f)); const float a = xOf(v.loopA.load()), b = xOf(v.loopB.load()); g.fillRect(Rectangle<float>(a, r.getY(), b - a, r.getHeight()).getIntersection(r)); g.setColour(kYellow.withAlpha(0.7f)); g.drawVerticalLine((int) a, r.getY(), r.getBottom()); g.drawVerticalLine((int) b, r.getY(), r.getBottom()); }
            for (int i = 0; i < 8; ++i)
            {
                const double h = v.hot[(size_t) i].load(); if (h < 0) continue;
                drawAt(h, [&](float x) { if (x < r.getX() || x > r.getRight()) return; g.setColour(kPad[i]); g.fillRect(x - 1.f, r.getY(), 2.f, r.getHeight()); g.fillRect(x, r.getY(), 16.f, 14.f); g.setColour(Colour(0xff111111)); g.setFont(mkFont(10.5f, true)); g.drawText(String(i + 1), (int) x, (int) r.getY(), 16, 14, juce::Justification::centred); });
            }
            drawAt(v.cue.load(), [&](float cx) { if (cx < r.getX() || cx > r.getRight()) return; g.setColour(kYellow); juce::Path tri; tri.addTriangle(cx - 5, r.getBottom(), cx + 5, r.getBottom(), cx, r.getBottom() - 9); g.fillPath(tri); });
            title = "A  " + String(v.bpm.load(), 1) + " BPM";
        }
        g.setColour(kOrange); g.setFont(mkFont(12.5f, true, 0.05f, true)); g.drawText(title, (int) r.getX() + 6, (int) r.getY() + 4, 360, 16, juce::Justification::left);
        g.setColour(juce::Colours::white); g.drawVerticalLine((int) r.getCentreX(), r.getY(), r.getBottom());
    }
    void mouseDown(const juce::MouseEvent& e) override { lastX = e.position.x; acc = 0.0; scfxui::Cmd c; c.type = scfxui::kJog; c.i = 1; proc.post(c); startTimerHz(60); }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const double dx = (double) (e.position.x - lastX); lastX = e.position.x;
        const double secPerPx = proc.view.src.load() == 0 || trk == nullptr ? windowSec / (double) std::max(1, getWidth()) : srcPx;
        acc += -dx * secPerPx * scfx::Deck::kPlatterRevPerSec;       // dragging right pulls the waveform backwards in time
    }
    void mouseUp(const juce::MouseEvent&) override { stopTimer(); scfxui::Cmd c; c.type = scfxui::kJog; c.i = 0; proc.post(c); }
private:
    void timerCallback() override { scfxui::Cmd c; c.type = scfxui::kJog; c.i = 1; c.a = acc * 60.0; acc = 0.0; proc.post(c); }
    ScratchFxProcessor& proc; float lastX = 0.f; double acc = 0.0, windowSec = 4.0, srcPx = 0.01;
};
}   // namespace

// ============================================================================ presets
namespace
{
using PV = std::vector<std::pair<String, float>>;
struct PresetDef { String name; PV v; };
const char* kPresetKeys[] = { "purron", "purrdrive", "purrxs", "purrfuzz", "purrmix", "vinyl", "master", "xf", "xcurve", "fxtarget", "trim", "lo", "mid", "hi", "colour", "fader", "killlo", "killmid", "killhi",
                              "tempo", "range", "keylock", "sync", "quant",
                              "fx1t", "fx1o", "fx1p1", "fx1p2", "fx2t", "fx2o", "fx2p1", "fx2p2", "fx3t", "fx3o", "fx3p1", "fx3p2" };
PV fx3(std::initializer_list<std::array<float, 4>> a) { PV v; int k = 1; for (auto& s : a) { const String n(k++); v.push_back({ "fx" + n + "o", s[0] }); v.push_back({ "fx" + n + "t", s[1] }); v.push_back({ "fx" + n + "p1", s[2] }); v.push_back({ "fx" + n + "p2", s[3] }); } return v; }
const std::vector<PresetDef>& factory()
{
    // type index: 0 Filter, 1 Echo, 2 Reverb, 3 Flanger, 4 Crush
    static const std::vector<PresetDef> f = {
        { "Init", {} },
        { "Wash Out", fx3({ { 0, 0, 0.25f, 1.f }, { 1, 1, 0.75f, 0.5f }, { 1, 2, 0.85f, 0.7f } }) },
        { "Battle scratch", { { "xcurve", 1.f }, { "vinyl", 0.f }, { "hi", 3.f } } },
        { "Radio squash", fx3({ { 1, 0, 0.72f, 1.f }, { 0, 1, 0.5f, 0.4f }, { 1, 4, 0.35f, 0.5f } }) },
        { "Echo out", fx3({ { 0, 0, 0.25f, 1.f }, { 1, 1, 0.5f, 0.55f }, { 0, 2, 0.5f, 0.3f } }) },
        { "Warm tape", { { "purron", 1.f }, { "purrdrive", 0.35f }, { "purrxs", 0.4f }, { "purrmix", 0.6f } } },
        { "Fuzz lead", { { "purron", 1.f }, { "purrfuzz", 0.6f }, { "purrdrive", 0.2f }, { "purrmix", 0.8f } } },
        { "Key lock jam", { { "keylock", 1.f }, { "tempo", 0.25f } } },
    };
    return f;
}
juce::File presetDir() { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Studio 87").getChildFile("Scratch Cat FX").getChildFile("presets"); }
}

// ============================================================================ the editor
struct ScratchFxEditor::Impl : public juce::Component
{
    ScratchFxEditor& ed; ScratchFxProcessor& proc; FxLAF laf; catui::RenderGate gate { *this };
    juce::TooltipWindow tips { this, 900 };
    Wave wave { proc }; Platter platter { proc };
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> cAtt;
    Pad* pads[8] = {}; RoundButton *playBtn = nullptr, *cueBtn = nullptr; HoldButton* liveBtn = nullptr;
    juce::TextButton *delBtn = nullptr, *zoomBtn[4] = {}, *rangeBtn[3] = {}, *beatBtn[6] = {};
    juce::ComboBox srcBox; juce::Slider* tempoFader = nullptr; Knob* hostKnob = nullptr; Knob* fxAmount[3] = {};
    HoldButton* presetName = nullptr;
    bool padDownParam[8] = {};
    int lastBeat = -1, lastSrcKind = -2, zoomIdx = 1; String lastFileName; bool pauseBg = true; std::unique_ptr<juce::FileChooser> chooser;
    float holdL = 0.f, holdR = 0.f, vuS = 0.f; int holdLc = 0, holdRc = 0;
    String presetLabel = "Init"; String statusCache; double lastHost = -1.0; bool lastHostHas = false;

    Impl(ScratchFxEditor& e, ScratchFxProcessor& p) : ed(e), proc(p)
    {
        setSize(kBaseW, kBaseH);
        presetLabel = proc.apvts.state.getProperty("preset", "Init").toString();
        buildHeader(); buildRig(); buildWave(); buildDeck(); buildMixer();
        gate.onFrame = [this] { frame(); };
        gate.start(30); applyTips();
    }
    ~Impl() override { gate.stop(); }

    // ---- small builders ----
    Knob* knob(const String& id, const String& name, int x, int y, int dia, bool right, Colour col, std::function<String(double)> fmt, const String& tip, bool bipolar = false, int boxW = 80)
    {
        auto k = std::make_unique<Knob>(); k->name = name; k->fmt = std::move(fmt); k->right = right; k->dia = dia;
        if (right) k->setBounds(x, y, dia + boxW, dia); else k->setBounds(x + dia / 2 - boxW / 2, y, boxW, dia + 34);
        k->s.setColour(juce::Slider::rotarySliderFillColourId, col); k->s.getProperties().set("bipolar", bipolar);
        k->setTooltip(tip); k->s.setTooltip(tip);
        auto* pr = proc.apvts.getParameter(id); k->s.setDoubleClickReturnValue(true, pr->convertFrom0to1(pr->getDefaultValue()));
        addAndMakeVisible(*k); sAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, k->s));
        auto* raw = k.get(); owned.push_back(std::move(k)); return raw;
    }
    juce::Slider* linear(const String& id, Rectangle<int> r, bool vertical, Colour col, const String& tip)
    {
        auto s = std::make_unique<juce::Slider>(vertical ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        s->setBounds(r); s->setColour(juce::Slider::thumbColourId, col); s->setTooltip(tip);
        auto* pr = proc.apvts.getParameter(id); s->setDoubleClickReturnValue(true, pr->convertFrom0to1(pr->getDefaultValue()));
        addAndMakeVisible(*s); sAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, id, *s));
        auto* raw = s.get(); owned.push_back(std::move(s)); return raw;
    }
    juce::TextButton* toggle(const String& id, const String& text, Rectangle<int> r, Colour col, const String& tip, int style = sNormal)
    {
        auto b = std::make_unique<juce::TextButton>(text); b->setClickingTogglesState(true); b->setBounds(r); b->setColour(juce::TextButton::buttonColourId, col); b->setColour(juce::TextButton::buttonOnColourId, col);
        b->getProperties().set("style", style); b->setTooltip(tip);
        addAndMakeVisible(*b); bAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, id, *b));
        auto* raw = b.get(); owned.push_back(std::move(b)); return raw;
    }
    juce::ComboBox* choice(const String& id, Rectangle<int> r, const String& tip)
    {
        auto c = std::make_unique<juce::ComboBox>(); c->setBounds(r); c->setTooltip(tip);
        if (auto* pc = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(id))) c->addItemList(pc->choices, 1);
        addAndMakeVisible(*c); cAtt.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, id, *c));
        auto* raw = c.get(); owned.push_back(std::move(c)); return raw;
    }
    // a button wired to a host-automatable momentary switch: pressed = on, released = off
    void wireHold(HoldButton* b, const String& id)
    {
        b->onPress = [this, id] { if (auto* p = proc.apvts.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(1.f); } };
        b->onRelease = [this, id] { if (auto* p = proc.apvts.getParameter(id)) { p->setValueNotifyingHost(0.f); p->endChangeGesture(); } };
    }
    HoldButton* hold(const String& id, const String& text, Rectangle<int> r, Colour col, const String& tip, int style = sNormal)
    {
        auto b = std::make_unique<HoldButton>(text); b->setBounds(r); b->setColour(juce::TextButton::buttonColourId, col); b->setColour(juce::TextButton::buttonOnColourId, col); b->getProperties().set("style", style);
        b->setTooltip(tip); addAndMakeVisible(*b); wireHold(b.get(), id); auto* raw = b.get(); owned.push_back(std::move(b)); return raw;
    }
    juce::TextButton* action(const String& text, Rectangle<int> r, const String& tip, std::function<void()> fn, int style = sNormal, Colour col = kOrange)
    {
        auto b = std::make_unique<juce::TextButton>(text); b->setBounds(r); b->setTooltip(tip); b->setColour(juce::TextButton::buttonColourId, col); b->setColour(juce::TextButton::buttonOnColourId, col); b->getProperties().set("style", style);
        b->onClick = std::move(fn); addAndMakeVisible(*b); auto* raw = b.get(); owned.push_back(std::move(b)); return raw;
    }
    void cmd(int type, int i = 0, double a = 0.0) { scfxui::Cmd c; c.type = type; c.i = i; c.a = a; proc.post(c); }
    float realOf(const String& id) const { return proc.get(id); }

    // ---- sections ----
    void buildHeader()
    {
        action(String::fromUTF8("\xe2\x80\xb9"), { 397, 17, 29, 31 }, "Previous preset.", [this] { stepPreset(-1); });
        presetName = new HoldButton(presetLabel); owned.emplace_back(presetName); presetName->setBounds(432, 16, 715, 32); presetName->setTooltip("Open the preset list: factory presets and your own.");
        addAndMakeVisible(*presetName); presetName->onClick = [this] { showPresetMenu(); }; presetName->getProperties().set("style", (int) sNormal);
        action(String::fromUTF8("\xe2\x80\xba"), { 1153, 17, 29, 31 }, "Next preset.", [this] { stepPreset(1); });
        action("SAVE", { 1188, 17, 63, 31 }, "Save the current settings as your own preset (kept on this computer).", [this] { savePresetDialog(); });
        action(String::fromUTF8("\xc2\xb7\xc2\xb7\xc2\xb7"), { 1257, 17, 38, 31 }, "Delete, export or import your presets.", [this] { showPresetMore(); });
        action("GUIDE", { 1311, 17, 70, 31 }, "Quick guide: what each part does.", [this] { showGuide(); });
        action("SETTINGS", { 1387, 17, 97, 31 }, "Info bubbles, drawing in the background, window size.", [this] { showSettings(); });
        action("ABOUT & SUPPORT", { 1491, 17, 165, 31 }, "About Studio 87, privacy, and ways to support the project.", [this] { showAbout(); });
    }
    void buildRig()
    {
        addAndMakeVisible(srcBox); srcBox.setBounds(160, 99, 189, 31);
        srcBox.setTooltip("What the scratch deck plays. DAW input: the live sound of the track this plugin is on. Captured loop: the last bars you captured. File: a track you loaded.");
        srcBox.onChange = [this] { const int id = srcBox.getSelectedId(); if (id > 0) proc.selectSource(id - 1); };
        liveBtn = new HoldButton(String::fromUTF8("\xe2\x96\xb6 DAW LIVE")); owned.emplace_back(liveBtn); liveBtn->setBounds(365, 91, 117, 32); liveBtn->setColour(juce::TextButton::buttonOnColourId, kOrange);
        liveBtn->setTooltip("Back to the live DAW input: the deck passes your DAW's sound straight through again. Lit while the deck is live."); addAndMakeVisible(*liveBtn); wireHold(liveBtn, "live");
        action("LOAD FILE", { 490, 92, 102, 31 }, "Load an audio file (wav, aiff, flac, ogg, mp3) into the scratch deck. It is analysed for tempo and key.", [this] { chooseFile(); });
        action("CAPTURE", { 600, 92, 94, 31 }, "Grab the last bars of the DAW input into the scratch deck as a loop, with the grid lined up to your DAW's beat. Set the length with the box on the right.", [this] { proc.capture(); });
        choice("capbars", { 880, 99, 100, 31 }, "How many bars Capture grabs.");
        hostKnob = knob("hostbpm", "HOST BPM", 711, 91, 32, true, kOrange, [this](double v) { return String(proc.view.hostHasBpm.load() ? proc.view.hostBpm.load() : v, 1); },
                        "The tempo of your DAW. When your DAW tells the plugin its tempo this follows it, otherwise set it here. Sync and Capture use it.", false, 130);
    }
    void buildWave()
    {
        addAndMakeVisible(wave); wave.setBounds(45, 148, 1202, 188);
        wave.setTooltip("The deck around the playhead (white line). Drag it like a record to scratch. Beat lines are every beat, taller ones are bars. Coloured marks are hot cues, yellow is the loop.");
        zoomIdx = juce::jlimit(0, 3, proc.getProp("zoom", 1)); wave.bars = 2 << zoomIdx;
        static const int zx[4] = { 95, 181, 267, 353 }, zw[4] = { 78, 78, 78, 86 }; static const char* zt[4] = { "2 BARS", "4 BARS", "8 BARS", "16 BARS" };
        for (int i = 0; i < 4; ++i)
            zoomBtn[i] = action(zt[i], { zx[i], 343, zw[i], 31 }, String("Show ") + zt[i] + " across the waveform.", [this, i] { zoomIdx = i; wave.bars = 2 << i; proc.setProp("zoom", i); wave.repaint(); }, sNormal, kOrange);
    }
    void buildDeck()
    {
        addAndMakeVisible(platter); platter.setBounds(688, 393, 378, 378);
        platter.setTooltip("The platter. Drag it to scratch: it follows your hand forward and backward and holds when you stop. Let go to play on.");
        // hot cues
        delBtn = new juce::TextButton("Delete"); owned.emplace_back(delBtn); delBtn->setBounds(59, 465, 62, 26); delBtn->setClickingTogglesState(true); delBtn->setColour(juce::TextButton::buttonOnColourId, Colour(0xffff4060));
        delBtn->setTooltip("Delete switch: while on, clicking a set hot cue removes it."); addAndMakeVisible(*delBtn);
        for (int i = 0; i < 8; ++i)
        {
            auto* b = new Pad(String(i + 1)); owned.emplace_back(b); b->col = kPad[i]; b->setBounds(59 + (i % 2) * 119, 499 + (i / 2) * 52, 111, 44);
            b->setTooltip("Hot cue " + String(i + 1) + ". Empty: sets a marker at the playhead. Set: jumps there. Turn Delete on to remove it. Needs a captured loop or a file.");
            addAndMakeVisible(*b); pads[i] = b;
            b->onPress = [this, i] {
                if (delBtn->getToggleState()) { if (pads[i]->isSet) cmd(scfxui::kHotClear, i); padDownParam[i] = false; return; }
                padDownParam[i] = true; if (auto* p = proc.apvts.getParameter(kHotIds[i])) { p->beginChangeGesture(); p->setValueNotifyingHost(1.f); } };
            b->onRelease = [this, i] { if (padDownParam[i]) { padDownParam[i] = false; if (auto* p = proc.apvts.getParameter(kHotIds[i])) { p->setValueNotifyingHost(0.f); p->endChangeGesture(); } } };
        }
        // performance row: Vinyl | Trans. | Stutter
        toggle("vinyl", "Vinyl", { 304, 413, 93, 31 }, kPeach, "On: pausing slows the record down like a turntable. Off: it stops at once.", sSegment);
        hold("trans", "Trans.", { 397, 413, 101, 31 }, kPeach, "Transform gate: chops the sound on and off in time while you hold it. Needs a captured loop or a file.", sSegment);
        hold("stutter", "Stutter", { 498, 413, 104, 31 }, kPeach, "Hold: retriggers the cue point every 1/16 beat. Needs a captured loop or a file.", sSegment);
        hold("brake", "Brake", { 303, 453, 146, 27 }, kPeach, "Slows the record to a stop.");
        hold("backspin", "Backspin", { 457, 453, 146, 27 }, kPeach, "The record spins back fast, then stops.");
        hold("roll", "Roll", { 303, 488, 146, 27 }, kPeach, "Hold: rolls a tiny loop of 1/8 beat; letting go lands where the track would have been. Needs a captured loop or a file.");
        hold("censor", "Censor", { 457, 488, 146, 27 }, kPeach, "Hold: plays backwards; letting go jumps to where the track would have been. Needs a captured loop or a file.");
        static const char* bl[6] = { "\xc2\xbc", "\xc2\xbd", "1", "2", "4", "8" }; static const double bv[6] = { 0.25, 0.5, 1, 2, 4, 8 }; static const int bx[6] = { 304, 356, 408, 457, 505, 554 }, bw[6] = { 52, 52, 48, 48, 48, 48 };
        for (int i = 0; i < 6; ++i)
            beatBtn[i] = action(String::fromUTF8(bl[i]), { bx[i], 524, bw[i], 31 }, "Loop of " + String(bv[i]) + " beat(s) from the playhead. Press again to leave the loop.", [this, i] {
                if (lastBeat == i && proc.view.loopOn.load()) { cmd(scfxui::kLoopExit); lastBeat = -1; } else { cmd(scfxui::kBeatLoop, 0, bv[i]); lastBeat = i; } }, sSegment, kPeach);
        hold("loopin", "Loop in", { 303, 564, 95, 27 }, kPeach, "Set the loop start here.");
        hold("loopout", "Loop out", { 406, 564, 95, 27 }, kPeach, "Set the loop end here and start looping.");
        hold("loopexit", "Exit", { 508, 564, 95, 27 }, kPeach, "Leave the loop.");
        cueBtn = new RoundButton("CUE"); owned.emplace_back(cueBtn); cueBtn->col = kYellow; cueBtn->setBounds(341, 603, 104, 104); cueBtn->setTooltip("CUE. While playing: jumps back to the cue point and stops. Stopped: sets the cue point here. Hold on the cue point to preview.");
        addAndMakeVisible(*cueBtn); wireHold(cueBtn, "cue");
        playBtn = new RoundButton(""); owned.emplace_back(playBtn); playBtn->col = kGreen; playBtn->playIcon = true; playBtn->setBounds(461, 603, 104, 104); playBtn->setTooltip("Play or pause the deck. Needs a captured loop or a file.");
        addAndMakeVisible(*playBtn); wireHold(playBtn, "play");
        toggle("sync", "Sync", { 304, 720, 90, 31 }, kTeal, "Match this deck's tempo to the host BPM. Your beat position stays yours: nudge or scratch to line up.", sSegment);
        toggle("keylock", "Key lock", { 394, 720, 112, 31 }, kTeal, "Key lock: change the tempo without changing the pitch.", sSegment);
        toggle("quant", "Quant", { 506, 720, 96, 31 }, kTeal, "Quantize: cue points, hot cues and loops snap to the beat grid.", sSegment);
        // tempo fader
        tempoFader = linear("tempo", { 1176, 404, 44, 284 }, true, kOrange, "Tempo fader. Up is faster, down is slower. Double click: back to 0. While Sync is on it shows where the deck really is.");
        static const char* rt[3] = { "8", "16", "50" }; static const int rx[3] = { 1153, 1179, 1211 }, rw[3] = { 25, 32, 32 };
        for (int i = 0; i < 3; ++i)
            rangeBtn[i] = action(rt[i], { rx[i], 712, rw[i], 25 }, "How far the tempo fader goes (percent).", [this, i] { proc.setParam("range", (float) i); }, sSegment, kOrange);
        action("Reset", { 1171, 744, 55, 27 }, "Put the tempo fader back to zero.", [this] { proc.setParam("tempo", 0.f); });
    }
    void buildMixer()
    {
        auto db = [](double v) { return fmtDb(v); };
        knob("trim", "TRIM", 1274, 176, 44, false, kOrange, db, "Input gain of the deck.", true);
        knob("hi", "HIGH", 1354, 176, 44, false, kOrange, db, "High shelf. Applies to everything you hear, DAW input included.", true);
        knob("mid", "MID", 1434, 176, 44, false, kOrange, db, "Mid band.", true);
        knob("lo", "LOW", 1514, 176, 44, false, kOrange, db, "Low shelf.", true);
        knob("colour", "COLOR", 1594, 176, 44, false, kOrange, [](double v) { return std::fabs(v) < 0.02 ? String("off") : v < 0 ? "LP " + String((int) std::lround(-v * 100)) : "HP " + String((int) std::lround(v * 100)); },
             "One-knob filter: left is low-pass, right is high-pass, centre is off.", true);
        toggle("killhi", "HIGH", { 1306, 256, 61, 31 }, kPink, "Silences the highs. Keep it pressed during a drop swap.", sKill);
        toggle("killmid", "MID", { 1374, 256, 51, 31 }, kPink, "Silences the mids.", sKill);
        toggle("killlo", "LOW", { 1433, 256, 57, 31 }, kPink, "Silences the lows.", sKill);
        linear("fader", { 1268, 303, 40, 116 }, true, kOrange, "Channel fader of the scratch deck. Double click: 0 dB.");
        linear("master", { 1320, 303, 40, 116 }, true, kOrange, "Output level. Double click: 0 dB.");
        linear("xf", { 1333, 428, 260, 26 }, false, kOrange, "Crossfader: left is the scratch deck, right is the DAW input playing straight. On the live deck the left half is the deck alone and the right half cuts to the straight DAW sound.");
        choice("xcurve", { 1265, 469, 95, 31 }, "Smooth: both full in the middle. Sharp: cuts like a battle mixer. Constant: equal power.");
        choice("fxtarget", { 1368, 469, 120, 31 }, "Send the effects to the scratch deck only, or to everything (master).");
        static const char* ft = "What this slot does.";
        for (int k = 0; k < 3; ++k)
        {
            const int y0 = 539 + 51 * k; const String n(k + 1);
            toggle("fx" + n + "o", "FX " + n, { 1265, y0, 56, 31 }, kOrange, "Turn this effect slot on or off.", sOutline);
            auto* cb = choice("fx" + n + "t", { 1327, y0 + 7, 114, 31 }, ft); (void) cb;
            cb->onChange = [this, k] { if (fxAmount[k]) fxAmount[k]->repaint(); };
            fxAmount[k] = knob("fx" + n + "p1", "AMOUNT", 1447, y0, 31, true, kOrange, [this, k](double v) {
                const int t = (int) std::lround(proc.get("fx" + String(k + 1) + "t"));
                if (t == 0) return String(v < 0.5 ? "LP" : "HP");
                if (t == 1) { static const char* e[4] = { "1/4", "1/2", "3/4", "1" }; return String(e[std::min(3, (int) (v * 4.0))]) + " beat"; }
                return String((int) std::lround(v * 100.0)) + "%"; },
                "Filter: sweep from low-pass to high-pass. Echo: delay time in beats. Reverb: size. Flanger: rate. Crush: amount.", false, 66);
            knob("fx" + n + "p2", "DRY/WET", 1548, y0, 31, true, kOrange, fmtPct, "How much of the effect is mixed in.", false, 90);
        }
        toggle("purron", "ON", { 1265, 734, 46, 31 }, kOrange, "Turns the PurrOne colour stage on or off. It sits after the three effects and before the master.", sOutline);
        knob("purrdrive", "DRIVE", 1343, 712, 44, false, kOrange, fmtPct, "Smooth tanh saturation. Under 80 % is warm, over 80 % is hard.", false, 70);
        knob("purrxs", "TRANSISTOR", 1440, 712, 44, false, kOrange, fmtPct, "Lopsided transistor curve: adds even harmonics, a thicker, warmer edge.", false, 80);
        knob("purrfuzz", "FUZZ", 1511, 712, 44, false, kOrange, fmtPct, "Fuzz: biased clipping with an octave-up growl. Nasty above 80 %.", false, 60);
        knob("purrmix", "MIX", 1595, 712, 44, false, kOrange, fmtPct, "Dry/wet of the colour stage.", false, 70);
    }

    // ---- actions ----
    void chooseFile()
    {
        chooser = std::make_unique<juce::FileChooser>("Load a track into the scratch deck", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc)
        { auto f = fc.getResult(); if (f.existsAsFile()) proc.loadFile(f); });
    }
    void applyTips() { tips.setMillisecondsBeforeTipAppears(proc.tipsOn() ? 900 : 100000000); }

    // ---- presets ----
    std::vector<PresetDef> allPresets() const
    {
        auto v = factory(); auto dir = presetDir();
        if (dir.isDirectory())
            for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
                if (auto xml = juce::parseXML(f))
                {
                    PresetDef d; d.name = xml->getStringAttribute("name", f.getFileNameWithoutExtension());
                    for (auto* e : xml->getChildIterator()) d.v.push_back({ e->getStringAttribute("id"), (float) e->getDoubleAttribute("v") });
                    d.name = "\x01" + d.name; v.push_back(d);       // \x01 marks a user preset
                }
        return v;
    }
    static String shown(const PresetDef& d) { return d.name.startsWithChar('\x01') ? d.name.substring(1) : d.name; }
    void applyPreset(const PresetDef& d)
    {
        for (auto* k : kPresetKeys) if (auto* p = proc.apvts.getParameter(k)) proc.setParam(k, p->convertFrom0to1(p->getDefaultValue()));
        for (auto& kv : d.v) if (proc.apvts.getParameter(kv.first)) proc.setParam(kv.first, kv.second);
        presetLabel = shown(d); proc.apvts.state.setProperty("preset", presetLabel, nullptr); presetName->setButtonText(presetLabel);
    }
    void stepPreset(int dir)
    {
        auto v = allPresets(); if (v.empty()) return; int cur = 0;
        for (int i = 0; i < (int) v.size(); ++i) if (shown(v[(size_t) i]) == presetLabel) cur = i;
        applyPreset(v[(size_t) ((cur + dir + (int) v.size()) % (int) v.size())]);
    }
    void showPresetMenu()
    {
        auto v = allPresets(); juce::PopupMenu m; int id = 1; bool sepDone = false; auto nFactory = factory().size();
        for (size_t i = 0; i < v.size(); ++i) { if (i == nFactory && !sepDone) { m.addSeparator(); m.addSectionHeader("Your presets"); sepDone = true; } m.addItem(id++, shown(v[i]), true, shown(v[i]) == presetLabel); }
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetName).withMinimumWidth(300), [this, v](int r) { if (r >= 1 && r <= (int) v.size()) applyPreset(v[(size_t) (r - 1)]); });
    }
    void savePresetDialog()
    {
        auto* w = new juce::AlertWindow("Save preset", "Name for this preset (kept on this computer):", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor("n", presetLabel == "Init" ? String("My preset") : presetLabel); w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<juce::AlertWindow> sw(w);
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, sw](int r) { if (r == 1 && sw != nullptr) saveUserPreset(sw->getTextEditorContents("n")); }), true);
    }
    void saveUserPreset(String name)
    {
        name = name.trim(); if (name.isEmpty()) return;
        const String safe = juce::File::createLegalFileName(name); auto dir = presetDir(); dir.createDirectory();
        juce::XmlElement x("Preset"); x.setAttribute("name", name);
        for (auto* k : kPresetKeys) { auto* e = x.createNewChildElement("P"); e->setAttribute("id", k); e->setAttribute("v", (double) proc.get(k)); }
        if (x.writeTo(dir.getChildFile(safe + ".xml"))) { presetLabel = name; proc.apvts.state.setProperty("preset", name, nullptr); presetName->setButtonText(name); }
    }
    void showPresetMore()
    {
        juce::PopupMenu m; m.addItem(1, "Delete this preset (yours only)"); m.addItem(2, "Export this preset to a file..."); m.addItem(3, "Import a preset file..."); m.addItem(4, "Show my presets folder");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&ed), [this](int r)
        {
            auto dir = presetDir();
            if (r == 1) { auto f = dir.getChildFile(juce::File::createLegalFileName(presetLabel) + ".xml"); if (f.existsAsFile()) { f.deleteFile(); presetLabel = "Init"; presetName->setButtonText(presetLabel); } }
            else if (r == 2) { saveUserPreset(presetLabel); auto f = dir.getChildFile(juce::File::createLegalFileName(presetLabel) + ".xml");
                chooser = std::make_unique<juce::FileChooser>("Export preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(f.getFileName()), "*.xml");
                chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [f](const juce::FileChooser& fc) { auto t = fc.getResult(); if (t != juce::File()) f.copyFileTo(t); }); }
            else if (r == 3) { chooser = std::make_unique<juce::FileChooser>("Import preset", juce::File(), "*.xml");
                chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f.existsAsFile()) { presetDir().createDirectory(); f.copyFileTo(presetDir().getChildFile(f.getFileName())); } }); }
            else if (r == 4) { dir.createDirectory(); dir.revealToUser(); }
        });
    }
    void showGuide()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Scratch Cat FX: quick guide",
            "Put Scratch Cat FX on the track you want to scratch. The deck plays the live sound of that track.\n\n"
            "Live: touch the platter (or the waveform) and drag: you scratch the last minute of what just played. Let go to play on, press DAW LIVE to come back.\n"
            "Capture: grabs the last bars (set the length next to it) into the deck as a loop, lined up to your DAW's beat. Now you have cues, loops, brake, backspin, roll, censor, stutter and transform on it. Load file does the same with a track from your disk.\n"
            "Crossfader: left is the scratch deck, right is your DAW's sound playing straight.\n"
            "Mixer: trim, 3 band EQ with kills, colour filter, deck fader, master. Three effects can sit on the deck or on the master, then PurrOne colour (drive, transistor, fuzz).\n"
            "Every switch and knob can be automated or mapped in your DAW.");
    }
    void showSettings()
    {
        juce::PopupMenu m; m.addItem(1, "Info bubbles (hover tips)", true, proc.tipsOn()); m.addItem(2, "Pause drawing while in the background", true, pauseBg); m.addSeparator(); m.addItem(3, "Reset window size (100%)");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&ed), [this](int r)
        {
            if (r == 1) { proc.setTipsOn(!proc.tipsOn()); applyTips(); }
            else if (r == 2) { pauseBg = !pauseBg; gate.setPauseInBackground(pauseBg); }
            else if (r == 3) ed.setSize(kBaseW, kBaseH);
        });
    }
    void showAbout()
    {
        juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::InfoIcon).withTitle("Scratch Cat FX")
            .withMessage("Scratch Cat FX by Studio 87.\nFree and open source (AGPL-3.0-or-later).\nIt collects no data and never touches the network.\nhttps://github.com/SpeakingColors/scratchcatfx-vst\n\nIf it helps your music, you can support the next plugins on Ko-fi.")
            .withButton("Support on Ko-fi").withButton("Close"), [](int r) { if (r == 1) juce::URL("https://ko-fi.com/studio87").launchInDefaultBrowser(); });
    }

    // ---- per frame ----
    void frame()
    {
        wave.trk = proc.shownTrack();
        const int src = proc.view.src.load(); const String fl = proc.fileLabel();
        if (src != lastSrcKind || fl != lastFileName || proc.hasTrackKind(1) != capOk || proc.hasTrackKind(2) != fileOk)
        {
            capOk = proc.hasTrackKind(1); fileOk = proc.hasTrackKind(2); lastFileName = fl; lastSrcKind = src;
            srcBox.clear(juce::dontSendNotification); srcBox.addItem("DAW input (live)", 1); srcBox.addItem("Captured loop", 2); srcBox.addItem(fl.isEmpty() ? String("File") : "File: " + fl, 3);
            srcBox.setItemEnabled(2, capOk); srcBox.setItemEnabled(3, fileOk); srcBox.setSelectedId(src + 1, juce::dontSendNotification);
        }
        const String st = proc.statusLine(); if (st != statusCache) { statusCache = st; repaint(Rectangle<int>(700, 80, 960, 50)); }
        const bool live = proc.view.src.load() == 0 && proc.view.live.load(); liveBtn->setToggleState(live, juce::dontSendNotification);
        const bool pl = proc.view.playing.load(); playBtn->lit = pl; playBtn->repaint(); cueBtn->repaint();
        for (int i = 0; i < 8; ++i) { const bool set = proc.view.hot[(size_t) i].load() >= 0; if (pads[i]->isSet != set || pads[i]->delMode != delBtn->getToggleState()) { pads[i]->isSet = set; pads[i]->delMode = delBtn->getToggleState(); pads[i]->repaint(); } }
        for (int i = 0; i < 4; ++i) zoomBtn[i]->setToggleState(i == zoomIdx, juce::dontSendNotification);
        const int rg = juce::jlimit(0, 2, (int) std::lround(proc.get("range"))); for (int i = 0; i < 3; ++i) rangeBtn[i]->setToggleState(i == rg, juce::dontSendNotification);
        if (!proc.view.loopOn.load()) lastBeat = -1;
        for (int i = 0; i < 6; ++i) beatBtn[i]->setToggleState(i == lastBeat, juce::dontSendNotification);
        // tempo fader follows the deck while Sync is on
        if (proc.get("sync") > 0.5f && proc.view.src.load() != 0) { static const double rng[3] = { 0.08, 0.16, 0.5 }; tempoFader->getProperties().set("disp", ((proc.view.tempoPct.load() / 100.0) / rng[rg] + 1.0) * 0.5); }
        else tempoFader->getProperties().remove("disp");
        tempoFader->repaint();
        if (hostKnob != nullptr && (proc.view.hostBpm.load() != lastHost || proc.view.hostHasBpm.load() != lastHostHas)) { lastHost = proc.view.hostBpm.load(); lastHostHas = proc.view.hostHasBpm.load(); hostKnob->repaint(); }
        // meters
        const float pl_ = proc.engine.peakL.load(), pr_ = proc.engine.peakR.load();
        if (pl_ >= holdL) { holdL = pl_; holdLc = 25; } else if (holdLc > 0) --holdLc; else holdL *= 0.93f;
        if (pr_ >= holdR) { holdR = pr_; holdRc = 25; } else if (holdRc > 0) --holdRc; else holdR *= 0.93f;
        wave.repaint(); platter.repaint();
        repaint(Rectangle<int>(1262, 290, 395, 145)); repaint(Rectangle<int>(1262, 418, 110, 22)); repaint(Rectangle<int>(1140, 686, 120, 22)); repaint(Rectangle<int>(1590, 436, 70, 20));
    }
    bool capOk = false, fileOk = false;

    static float dbOf(float g) { return 20.f * std::log10(std::max(1e-5f, g)); }
    void paint(juce::Graphics& g) override
    {
        g.setColour(kGround); g.fillRect(kCheek, 0, kBaseW - 2 * kCheek, kBaseH);
        // header
        catui::drawCatHead(g, { 44.f, 15.f, 40.f, 38.f }, kInk, kGround);
        txt(g, "SCRATCH CAT FX", { 92, 10, 300, 36 }, 29.f, kInk, juce::Justification::centredLeft, true, 0.14f);
        txt(g, "STUDIO 87", { 93, 40, 200, 16 }, 12.5f, Colour(0xffFFD100), juce::Justification::centredLeft, false, 0.3f);
        g.setColour(kLine); g.drawHorizontalLine(64, 44.f, 1656.f);
        // rig row
        g.setColour(kPanel); g.fillRoundedRectangle(44.f, 76.f, 1612.f, 63.f, 4.f); g.setColour(kLine); g.drawRoundedRectangle(44.f, 76.f, 1612.f, 63.f, 4.f, 1.f);
        txt(g, "DAW INPUT", { 55, 98, 100, 18 }, 12.5f, kDim, juce::Justification::centredLeft, true, 0.22f);
        txt(g, "SOURCE", { 160, 82, 100, 14 }, 10.5f, kDim, juce::Justification::centredLeft, true, 0.22f);
        txt(g, "LENGTH", { 880, 82, 100, 14 }, 10.5f, kDim, juce::Justification::centredLeft, true, 0.22f);
        txt(g, statusCache, { 990, 98, 655, 18 }, 12.5f, kDim, juce::Justification::centredRight, false, 0.02f, true);
        // zoom label
        txt(g, "ZOOM", { 44, 349, 50, 18 }, 11.5f, kDim, juce::Justification::centredLeft, true, 0.2f);
        // deck panel
        g.setColour(kPanel); g.fillRoundedRectangle(44.f, 380.f, 1203.f, 404.f, 4.f); g.setColour(kLine); g.drawRoundedRectangle(44.f, 380.f, 1203.f, 404.f, 4.f, 1.f);
        g.setColour(kOrange); g.fillRect(44.f, 382.f, 3.f, 400.f);
        g.setColour(kLine); g.drawVerticalLine(1152, 394.f, 774.f);
        g.setColour(kSeg.withAlpha(0.7f));       // outlines of the segmented groups
        for (auto rr : { Rectangle<float>(303.5f, 413.5f, 299.f, 30.f), Rectangle<float>(303.5f, 524.5f, 299.f, 30.f), Rectangle<float>(303.5f, 720.5f, 299.f, 30.f), Rectangle<float>(1153.5f, 712.5f, 89.f, 24.f) }) g.drawRoundedRectangle(rr, 4.f, 1.2f);
        txt(g, "TEMPO", { 1148, 386, 100, 14 }, 10.5f, kDim, juce::Justification::centred, true, 0.2f);
        { const double pct = proc.view.tempoPct.load(); txt(g, String(pct, 1) + "%", { 1148, 688, 100, 18 }, 12.5f, kOrange, juce::Justification::centred, false, 0.f, true); }
        // mixer panel
        g.setColour(kPanel); g.fillRoundedRectangle(1257.f, 147.f, 398.f, 707.f, 4.f); g.setColour(kLine); g.drawRoundedRectangle(1257.f, 147.f, 398.f, 707.f, 4.f, 1.f);
        txt(g, "MIXER", { 1265, 154, 200, 18 }, 13.f, kInk, juce::Justification::centredLeft, true, 0.25f);
        txt(g, "KILL", { 1265, 264, 40, 16 }, 11.f, kDim, juce::Justification::centredLeft, true, 0.2f);
        txt(g, "DECK", { 1258, 292, 60, 14 }, 10.5f, kDim, juce::Justification::centred, true, 0.2f); txt(g, "MASTER", { 1306, 292, 70, 14 }, 10.5f, kDim, juce::Justification::centred, true, 0.2f);
        const float fd = proc.get("fader"); txt(g, fd <= -59.5f ? String("-inf dB") : fmtDb(fd), { 1250, 420, 76, 16 }, 11.5f, kOrange, juce::Justification::centred, false, 0.f, true);
        txt(g, fmtDb(proc.get("master")), { 1302, 420, 76, 16 }, 11.5f, kOrange, juce::Justification::centred, false, 0.f, true);
        txt(g, "CROSS", { 1265, 433, 60, 16 }, 11.f, kDim, juce::Justification::centredLeft, true, 0.2f);
        { const float x = proc.get("xf"); txt(g, std::fabs(x) < 0.03f ? "mid" : x < 0 ? "Scratch" : "DAW", { 1595, 433, 50, 16 }, 12.f, kOrange, juce::Justification::centredRight, false, 0.f, true); }
        txt(g, "CURVE", { 1265, 453, 90, 14 }, 10.5f, kDim, juce::Justification::centredLeft, true, 0.2f); txt(g, "FX ON", { 1368, 453, 90, 14 }, 10.5f, kDim, juce::Justification::centredLeft, true, 0.2f);
        g.setColour(kLine); g.drawHorizontalLine(512, 1265.f, 1647.f);
        txt(g, "EFFECTS", { 1265, 513, 200, 18 }, 13.f, kInk, juce::Justification::centredLeft, true, 0.25f);
        for (int k = 0; k < 3; ++k) txt(g, "TYPE", { 1327, 530 + 51 * k, 60, 14 }, 10.5f, kDim, juce::Justification::centredLeft, true, 0.2f);
        g.setColour(kLine); g.drawHorizontalLine(690, 1265.f, 1647.f);
        txt(g, "PURRONE COLOUR", { 1265, 692, 250, 18 }, 13.f, kInk, juce::Justification::centredLeft, true, 0.25f);
        // meters: deck vu and the L/R output pair
        auto vu = Rectangle<float>(1372.f, 294.f, 36.f, 134.f); g.setColour(kDisp); g.fillRect(vu); g.setColour(kLine); g.drawRect(vu, 1.f);
        auto mt = Rectangle<float>(1418.f, 294.f, 228.f, 134.f); g.setColour(kDisp); g.fillRect(mt); g.setColour(kLine); g.drawRect(mt, 1.f);
        auto yOf = [](Rectangle<float> r, float db, float lo) { return r.getBottom() - 4.f - (r.getHeight() - 8.f) * (db - lo) / (0.f - lo); };
        static const int vl[6] = { 0, -6, -12, -24, -36, -48 };
        for (int d : vl) { const float y = yOf(vu, (float) d, -48.f); g.setColour(kGrid); g.drawHorizontalLine((int) y, vu.getX() + 1, vu.getRight() - 1); txt(g, String(d), { (int) vu.getX() + 2, (int) y - 6, 14, 12 }, 8.5f, kDim.withAlpha(0.7f), juce::Justification::centredLeft, false, 0.f, true); }
        { const float db = juce::jlimit(-48.f, 0.f, dbOf(proc.engine.vuDeck.load())); const float y = yOf(vu, db, -48.f); g.setColour(db > -1.f ? Colour(0xffff4060) : kOrange); g.fillRect(vu.getX() + 18.f, y, 14.f, vu.getBottom() - 4.f - y); }
        for (int d = 0; d >= -54; d -= 6) { const float y = yOf(mt, (float) d, -54.f); g.setColour(kGrid); g.drawHorizontalLine((int) y, mt.getX() + 1, mt.getRight() - 1); txt(g, String(d), { (int) mt.getCentreX() - 20, (int) y - 6, 40, 12 }, 9.f, kScale, juce::Justification::centred, false, 0.f, true); }
        auto bar = [&](float v, Rectangle<float> r, float hold) { const float db = juce::jlimit(-54.f, 0.f, dbOf(v)); const float y = yOf(mt, db, -54.f); g.setColour(db > -1.f ? Colour(0xffff4060) : kOrange.withAlpha(0.9f)); g.fillRect(r.getX(), y, r.getWidth(), mt.getBottom() - 4.f - y);
                                                           const float hy = yOf(mt, juce::jlimit(-54.f, 0.f, dbOf(hold)), -54.f); g.setColour(juce::Colours::white); g.fillRect(r.getX(), hy - 1.f, r.getWidth(), 2.f); };
        bar(proc.engine.peakL.load(), { mt.getX() + 4.f, 0.f, 104.f, 0.f }, holdL); bar(proc.engine.peakR.load(), { mt.getX() + 120.f, 0.f, 104.f, 0.f }, holdR);
        txt(g, "L", { (int) mt.getX() + 50, (int) mt.getBottom() - 14, 14, 12 }, 9.5f, kDim, juce::Justification::centred, true); txt(g, "R", { (int) mt.getX() + 166, (int) mt.getBottom() - 14, 14, 12 }, 9.5f, kDim, juce::Justification::centred, true);
        // footer
        g.setColour(kLine); g.drawHorizontalLine(865, 44.f, 1656.f);
        txt(g, String::fromUTF8("STUDIO 87  \xc2\xb7  SCRATCH CAT FX V") + String(JucePlugin_VersionString), { 44, 872, 700, 18 }, 11.5f, kDim, juce::Justification::centredLeft, true, 0.2f);
        txt(g, "WINDOW " + String((int) std::lround(100.0 * (double) ed.getWidth() / (double) kBaseW)) + " %", { 1400, 872, 256, 18 }, 11.5f, kDim, juce::Justification::centredRight, false, 0.2f);
    }
};

ScratchFxEditor::ScratchFxEditor(ScratchFxProcessor& p) : AudioProcessorEditor(&p)
{
    impl = std::make_unique<Impl>(*this, p);
    setLookAndFeel(&impl->laf); addAndMakeVisible(*impl); impl->sendLookAndFeelChange();
    setResizable(true, true);
    setResizeLimits(kMinW, (int) std::lround((double) kMinW * kBaseH / kBaseW), kMaxW, (int) std::lround((double) kMaxW * kBaseH / kBaseW));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio((double) kBaseW / (double) kBaseH);
    const int w = juce::jlimit(kMinW, kMaxW, p.getProp("uiw", kBaseW));
    setSize(w, (int) std::lround((double) w * kBaseH / kBaseW));
}
ScratchFxEditor::~ScratchFxEditor() { setLookAndFeel(nullptr); }
void ScratchFxEditor::paint(juce::Graphics& g)
{
    g.fillAll(kGround);
    catui::drawWoodCheeks(g, getLocalBounds(), (int) std::lround(kCheek * (double) getWidth() / kBaseW));
}
void ScratchFxEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int ck = (int) std::lround(kCheek * (double) getWidth() / kBaseW);
    if (e.x < ck || e.x >= getWidth() - ck) setSize(kBaseW, kBaseH);
}
void ScratchFxEditor::resized()
{
    if (impl == nullptr) return;
    const float s = (float) getWidth() / (float) kBaseW;
    impl->setBounds(0, 0, kBaseW, kBaseH);
    impl->setTransform(juce::AffineTransform::scale(s));
    if (auto* pr = dynamic_cast<ScratchFxProcessor*>(&processor)) pr->setProp("uiw", getWidth());
}
