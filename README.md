<div align="center">

# Pedrochord

**An omnichord-style electronic instrument as a VST3 plugin: hold a chord button, strum the plate, and every string plays a note of that chord.**

**English** · [Español](README.es.md)

[![Build](https://github.com/pedrofelipe14/pedrochord/actions/workflows/build.yml/badge.svg)](https://github.com/pedrofelipe14/pedrochord/actions/workflows/build.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![JUCE](https://img.shields.io/badge/JUCE-9-8DC63F)
![VST3](https://img.shields.io/badge/VST3-Standalone-555)
![pluginval](https://img.shields.io/badge/pluginval-strictness_10-2ea44f)

![Pedrochord playing A minor 7 after a strum](docs/screenshot.png)

</div>

## How it plays

- **Chord buttons:** 12 roots × 4 chord types (major, minor, 7, m7). While you hold one, its chord sounds as a soft organ-like pad with the root in the bass. Slide onto another button to change chords; let go and the previous held chord comes back.
- **Strum plate:** 13 strings, lowest at the bottom. Touch a string to pluck it or drag across them to strum; faster strums play louder. Each string plays the next tone of the current chord, so every strum is in tune. The strings keep the last chord after you let go of its button.
- **Sound:** strings are a two-operator FM voice whose brightness fades faster than its body, so they start glassy and ring out mellow. Controls cover levels, string sustain and brightness, chorus, reverb and *wobble*, a tape-like pitch drift.

- **Auto-strum:** a sequencer that plays the strings by itself, in time, while you hold a chord. It has five patterns (up, down, up and down, full strum alternating down and up strokes, random), quarter to sixteenth-note steps including triplets, and a range that sets how many strings it uses. The first step of each beat is accented. It follows the DAW's tempo with SYNC on. Space switches it on and off.

### Computer keyboard

`1`–`0` major · `Q`–`P` minor · `A`–`Ñ` seventh · `Z`–`M` minor seventh (from C, left to right, Latin American layout) · space: auto-strum.

### From a DAW (MIDI)

| Notes | Plays |
|---|---|
| C1–B1 | Major chord on that root |
| C2–B2 | Minor |
| C3–B3 | Seventh |
| C4–B4 | Minor seventh |
| C5–C6 | Strings 1–13 |
| CC1 (mod wheel) | Sweeps across the strings like a finger on the plate |

## How it's built

```
dsp/      the instrument: header-only C++17, no JUCE (it will also compile to WebAssembly)
  include/pedrochord/chords.h    chords, voicings, the MIDI layout
  include/pedrochord/voices.h    pad, bass and FM string voices
  include/pedrochord/effects.h   chorus and a small Freeverb-style reverb
  include/pedrochord/engine.h    held-chord stack, strum logic, the auto-strum sequencer, mixing
  tests/                       22 native tests
plugin/   JUCE wrapper and the editor, drawn as 1980s hardware (orange body, LED readout, ribbed plate)
tools/    an off-screen renderer that takes the screenshot above
```

- **The UI plays through MIDI too.** Chord buttons and the strum plate send the same note messages a DAW would, through a thread-safe queue. So mouse, touch and MIDI share a single code path, and the tests exercise it.
- **Sample-accurate.** The processor renders up to each MIDI event and then applies it, so a strum keeps its timing inside a block.
- **Nothing allocates on the audio thread.** Delay lines are fixed-size arrays, and the UI reads the engine's state (held chord, string glow) from atomics.
- **Validated like a DAW would load it:** CI runs [pluginval](https://github.com/Tracktion/pluginval) at its strictest level (10) on every push: different sample rates and block sizes, state save and restore, automation, parameter fuzzing, and opening the editor while audio runs.
- **Tested without a DAW:** the auto-strum's timing at every step length and the order of every pattern, chord voicings, the MIDI layout, the held-chord stack, strums crossing the right strings, decay times that don't depend on the sample rate, and a stress test with everything at maximum that stays finite and below full scale.

## Building

Needs CMake 3.22+ and Visual Studio 2022 (or another C++17 compiler). JUCE is a submodule.

```sh
git clone --recursive https://github.com/pedrofelipe14/pedrochord.git
cmake -B build
cmake --build build --config Release --target Pedrochord_All
# DSP tests only:
cmake -S dsp -B dsp/build && cmake --build dsp/build --config Release && dsp/build/Release/dsp_tests
```

## Roadmap

- [x] Auto-strum sequencer.
- [ ] A rhythm section: drum patterns with an auto-bass that follows the chord.
- [x] Computer-keyboard play: 1-0 major, Q-P minor, A-Ñ 7, Z-M m7 (Latin American layout).
- [ ] The web version: the same `dsp/` compiled to WebAssembly, playable on a phone.
