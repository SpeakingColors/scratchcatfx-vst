# Scratch Cat FX 🐈

<img src="assets/icon.svg" width="96" align="right" alt="icon">

Scratch effect plugin (VST3 and standalone): scratch, brake and backspin the sound coming from your DAW, with EQ, colour and a 3 slot FX chain.

## Status
Early. What exists today:
- A tested pure C++ engine (`src/core`): the DAW input is recorded, the idle sound passes straight through (bit exact at neutral settings), and the scratch deck plays it back with vinyl style speed, brake, backspin and the moves. Core is shared with [Scratch Cat Duo](https://github.com/SpeakingColors/scratchcatduo-vst).
- A JUCE plugin shell (`src/fx`) with all controls as host parameters (hand on platter, platter speed, brake, backspin, go live, trim, EQ, colour, master, 3 FX slots). It uses JUCE's generic editor.
- Not yet: the cat-ui editor from the [mockup](docs/mockup/) (wood cheeks, scope, platter), MIDI mapping, presets, a real listening pass. Nothing here has been tried by ear in a host yet.

Run the engine tests: `sh tests/run.sh` (needs only g++).

<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

☕ Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming. 💜

## Docs
- [Interactive mockups](docs/README.md)
- [Roadmap](ROADMAP.md)

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Not affiliated with or endorsed by any hardware maker named here. All trademarks belong to their owners.
