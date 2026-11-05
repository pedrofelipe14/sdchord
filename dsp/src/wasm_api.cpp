// C entry points the AudioWorklet calls. Built as a standalone .wasm with no JS glue (see
// scripts/build-wasm.sh): the worklet instantiates it directly and copies audio out of two buffers.

#include <emscripten/emscripten.h>

#include "sdchord/engine.h"

namespace
{
constexpr int maxBlock = 1024;

sdchord::Engine engine;
float outLeft[maxBlock];
float outRight[maxBlock];

/** Parameter ids shared with web/src/engine/params.ts (a test checks they match). */
enum ParamId
{
    chordLevel,
    chordTone,
    bassLevel,
    stringLevel,
    stringDecay,
    stringTone,
    chorus,
    reverb,
    wobble,
    master,
    seqOn,
    seqBpm,
    seqPattern,
    seqRate,
    seqRange,
    rhythmOn,
    rhythmPattern,
    rhythmLevel,
    lfoDepth,
    lfoRate,
    lfoShape,
    lfoTarget,
};
} // namespace

extern "C"
{
EMSCRIPTEN_KEEPALIVE void sdc_init(float sampleRate) { engine.prepare(sampleRate); }

EMSCRIPTEN_KEEPALIVE void sdc_note_on(int note, float velocity) { engine.noteOn(note, velocity); }
EMSCRIPTEN_KEEPALIVE void sdc_note_off(int note) { engine.noteOff(note); }
EMSCRIPTEN_KEEPALIVE void sdc_control_change(int cc, int value) { engine.controlChange(cc, value); }
EMSCRIPTEN_KEEPALIVE void sdc_all_notes_off() { engine.allNotesOff(); }
EMSCRIPTEN_KEEPALIVE void sdc_loop_button() { engine.loopButton(); }
EMSCRIPTEN_KEEPALIVE void sdc_loop_clear() { engine.loopClear(); }

EMSCRIPTEN_KEEPALIVE void sdc_set_param(int id, float value)
{
    auto& p = engine.params;
    switch (id)
    {
        case chordLevel: p.chordLevel = value; break;
        case chordTone: p.chordTone = value; break;
        case bassLevel: p.bassLevel = value; break;
        case stringLevel: p.stringLevel = value; break;
        case stringDecay: p.stringDecay = value; break;
        case stringTone: p.stringTone = value; break;
        case chorus: p.chorus = value; break;
        case reverb: p.reverb = value; break;
        case wobble: p.wobble = value; break;
        case master: p.master = value; break;
        case seqOn: p.seqOn = value > 0.5f; break;
        case seqBpm: p.seqBpm = value; break;
        case seqPattern: p.seqPattern = (int) value; break;
        case seqRate: p.seqRate = (int) value; break;
        case seqRange: p.seqRange = (int) value; break;
        case rhythmOn: p.rhythmOn = value > 0.5f; break;
        case rhythmPattern: p.rhythmPattern = (int) value; break;
        case rhythmLevel: p.rhythmLevel = value; break;
        case lfoDepth: p.lfoDepth = value; break;
        case lfoRate: p.lfoRate = value; break;
        case lfoShape: p.lfoShape = (int) value; break;
        case lfoTarget: p.lfoTarget = (int) value; break;
        default: break;
    }
}

/** Renders up to maxBlock frames into the buffers returned by sdc_left/sdc_right. */
EMSCRIPTEN_KEEPALIVE void sdc_process(int numFrames)
{
    if (numFrames > maxBlock)
        numFrames = maxBlock;
    engine.process(outLeft, outRight, numFrames);
}

EMSCRIPTEN_KEEPALIVE float* sdc_left() { return outLeft; }
EMSCRIPTEN_KEEPALIVE float* sdc_right() { return outRight; }

// State for the UI.
EMSCRIPTEN_KEEPALIVE int sdc_held_chord() { return engine.heldChordCode(); }
EMSCRIPTEN_KEEPALIVE int sdc_string_chord() { return engine.stringChordCode(); }
EMSCRIPTEN_KEEPALIVE float sdc_string_glow(int string) { return engine.stringGlow(string); }
EMSCRIPTEN_KEEPALIVE int sdc_pluck_count() { return engine.pluckCount(); }
EMSCRIPTEN_KEEPALIVE int sdc_rhythm_step() { return engine.rhythmStep(); }
EMSCRIPTEN_KEEPALIVE int sdc_drum_hits() { return engine.drumHits(); }
EMSCRIPTEN_KEEPALIVE int sdc_loop_state() { return engine.loopStatus(); }
EMSCRIPTEN_KEEPALIVE float sdc_loop_beats() { return engine.loopBeats(); }
EMSCRIPTEN_KEEPALIVE float sdc_loop_position() { return engine.loopPosition(); }

// The rhythm patterns' shape, so the page can lay out its step LEDs (a test checks web/src agrees).
EMSCRIPTEN_KEEPALIVE int sdc_rhythm_count() { return sdchord::kRhythmPatterns; }
EMSCRIPTEN_KEEPALIVE int sdc_rhythm_length(int pattern) { return sdchord::rhythmPatterns()[(size_t) pattern].length; }
EMSCRIPTEN_KEEPALIVE int sdc_rhythm_steps_per_beat(int pattern) { return sdchord::rhythmPatterns()[(size_t) pattern].stepsPerBeat; }
}
