# Roadmap: Scratch Cat FX

Scratch effect plugin.

## Done
- [x] Pure C++ engine with unit tests (pass through, scratch, brake, backspin, EQ, FX chain, level safety)
- [x] JUCE plugin shell with host parameters, CI (core tests, configure, build)

## Next
- [ ] Check the CI build artefacts load in a host and listen: scratch feel, brake and backspin times
- [ ] cat-ui editor from the mockup (wood cheeks, platter, scope with dB scale, info bubbles) drawn through `catui::RenderGate`
- [ ] Presets and state backup folders
- [ ] MIDI mapping (jog, touch, buttons)
- [ ] Manual

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%
