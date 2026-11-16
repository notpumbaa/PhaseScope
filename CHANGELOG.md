# Changelog

Patch notes for every PhaseScope release. The newest version is at the top.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

Nothing yet. Changes for the next version are collected here. Before a release, rename this section to `## [x.y.z] – YYYY-MM-DD` with the version from `CMakeLists.txt`, add a new empty `Unreleased` section above it and run the *Release* workflow.

## [1.0.0] – 2026-11-12

First public release.

### Features
- Goniometer with afterglow and auto-gain
- Full-band correlation meter with min hold
- Per-band correlation for bass (< 150 Hz), mids (150 Hz – 2.5 kHz) and highs (> 2.5 kHz)
- Mono loss meter (mono sum vs. stereo level) with max hold
- 10-second correlation history for full band and bass
- Adjustable integration time (50–3000 ms)
- Mono monitor switch
- macOS: AU, VST3 and Standalone, universal binary (Apple Silicon + Intel), macOS 12+
- Windows: VST3 and Standalone (64-bit)

[Unreleased]: https://github.com/notpumbaa/PhaseScope/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/notpumbaa/PhaseScope/releases/tag/v1.0.0
