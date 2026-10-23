# Roadmap: Scratch Cat FX

Scratch effect plugin.

## Done
- [x] Pure C++ engine with unit tests (pass through, scratch, brake, backspin, EQ, FX chain, level safety)
- [x] JUCE plugin shell with host parameters, CI (core tests, configure, build)

## Next
- [ ] Check the CI build artefacts load in a host and listen: scratch feel, brake and backspin times
- [x] cat-ui editor from the mockup (0.2.0), drawn through `catui::RenderGate` with the PurrOne wood
- [ ] Presets and state backup folders
- [ ] MIDI mapping (jog, touch, buttons)
- [x] Manual (PDF, built by the release workflow)
- [ ] Manual with screenshots from the real editor

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%
