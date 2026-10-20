#pragma once
#include "ScratchFx.h"

// Scratch Cat FX editor: the approved design mockup (docs/mockup/scratchcatfx.html), built with cat-ui.
// Designed on a 1700 x 900 canvas (wood cheeks included) and scaled with the window.
class ScratchFxEditor : public juce::AudioProcessorEditor
{
public:
    explicit ScratchFxEditor(ScratchFxProcessor&);
    ~ScratchFxEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;   // on a wood cheek: back to 100%
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScratchFxEditor)
};
