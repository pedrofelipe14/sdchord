#pragma once

// The whole instrument: hold a chord and it sounds (with its bass); the strum plate's strings play
// that chord's notes and keep the last chord after you let go. Driven by MIDI notes and CC1, so the
// plugin, the web build and the tests all play it the same way.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

#include "chords.h"
#include "effects.h"
#include "rhythm.h"
#include "voices.h"

namespace sdchord
{
/** How the auto-strum walks the strings on each step. */
enum class SeqPattern
{
    Up,
    Down,
    UpDown,
    Strum, // every string on each step, alternating down and up strokes
    Random,
};
constexpr int kSeqPatterns = 5;

/** The LFO's wave. Random holds a new value each cycle (sample and hold). */
enum class LfoShape
{
    Sine,
    Triangle,
    Square,
    Random,
};
constexpr int kLfoShapes = 4;

/** What the LFO moves. It acts on the chords and the strings; the bass and the drums stay put. */
enum class LfoTarget
{
    Vibrato, // pitch, up to 0.6 semitone either way
    Tremolo, // volume, down to silence at full depth
    Filter, // the chords' tone knob, swept up and down
    Pan, // left and right
};
constexpr int kLfoTargets = 4;

/** The chord looper, driven by one button: record, then play, then stop and play again. */
enum class LoopState
{
    Empty,
    Armed, // waiting for the first chord, which starts the recording
    Recording,
    Playing,
    Stopped, // a loop is kept, silent
};

/** Step lengths: quarter, eighth, eighth triplet, sixteenth. */
constexpr int kSeqRates = 4;
constexpr int kStepsPerBeat[kSeqRates] = { 1, 2, 3, 4 };

struct Params
{
    float chordLevel = 0.55f; // 0..1
    float chordTone = 0.35f; // 0..1, dark to bright
    float bassLevel = 0.5f; // 0..1
    float stringLevel = 0.8f; // 0..1
    float stringDecay = 1.8f; // seconds to fade out (0.2..8)
    float stringTone = 0.5f; // 0..1
    float chorus = 0.35f; // 0..1
    float reverb = 0.3f; // 0..1
    float wobble = 0.15f; // 0..1, tape-like pitch drift
    float master = 0.8f; // 0..1

    // Auto-strum: plays the strings by itself, in time, while a chord is held.
    bool seqOn = false;
    float seqBpm = 100.0f; // 40..240
    int seqPattern = 0; // SeqPattern
    int seqRate = 1; // index into kStepsPerBeat
    int seqRange = 8; // how many strings it uses, from the lowest (2..13)

    // Rhythm machine (the web version shows it). While it runs, it is the clock: the auto-strum
    // locks to its grid so both stay in time. It shares the auto-strum's tempo (seqBpm).
    bool rhythmOn = false;
    int rhythmPattern = 0; // index into rhythmPatterns()
    float rhythmLevel = 0.7f; // 0..1

    // LFO.
    float lfoDepth = 0.0f; // 0..1, 0 = off
    float lfoRate = 4.0f; // Hz (0.1..12)
    int lfoShape = 0; // LfoShape
    int lfoTarget = 0; // LfoTarget
};

class Engine
{
public:
    Params params;

    void prepare(double sr)
    {
        sampleRate = sr;
        for (auto& p : pads)
            p.prepare(sr);
        bass.prepare(sr);
        for (auto& s : strings)
            s.prepare(sr);
        chorus.prepare(sr);
        reverb.prepare(sr);
        rhythm.prepare(sr);
        smooth = coefFor(0.02, sr);
        lfoSmooth = coefFor(0.004, sr);
    }

    // ---- input ---------------------------------------------------------------------------------

    void noteOn(int note, float velocity)
    {
        if (velocity <= 0.0f)
        {
            noteOff(note);
            return;
        }
        Chord chord;
        if (chordForNote(note, chord))
        {
            removeHeld(note);
            if (heldCount < (int) held.size())
                held[(size_t) heldCount++] = note;
            updateChord();
        }
        else if (const int s = stringForNote(note); s >= 0)
        {
            pluck(s, velocity);
        }
    }

    void noteOff(int note)
    {
        Chord chord;
        if (chordForNote(note, chord) && removeHeld(note))
            updateChord();
    }

    /** CC1 is a finger on the plate: moving it plucks every string it crosses. CC123 = all notes off. */
    void controlChange(int cc, int value)
    {
        if (cc == 1)
        {
            const float position = (float) value / 127.0f;
            if (lastStrum >= 0.0f)
                strumAcross(lastStrum, position, 0.8f);
            lastStrum = position;
        }
        else if (cc == 123)
        {
            allNotesOff();
        }
    }

    /** Plucks the strings whose centre lies between two plate positions (0 = lowest, 1 = highest). */
    void strumAcross(float from, float to, float velocity)
    {
        const float lo = std::min(from, to), hi = std::max(from, to);
        for (int s = 0; s < kStrings; ++s)
        {
            const float centre = ((float) s + 0.5f) / (float) kStrings;
            if (centre > lo && centre <= hi)
                pluck(s, velocity);
        }
    }

    void pluck(int string, float velocity)
    {
        if (string < 0 || string >= kStrings)
            return;
        const auto notes = stringNotes(memory);
        strings[(size_t) string].pluck(notes[(size_t) string], std::clamp(velocity, 0.05f, 1.0f),
                                       std::clamp(params.stringDecay, 0.2f, 8.0f), params.stringTone);
        ++plucks;
        lastString = string;
    }

    // ---- chord looper -------------------------------------------------------------------------
    //
    // Records the chord changes (not the strings), then plays them round and round so you can strum
    // and play the auto-strum over them. Each chord lasts until the next one. While the rhythm machine
    // runs, the loop starts on the nearest bar line and lasts whole bars, so it stays with the drums.
    // A chord held by hand sounds over the loop; letting go brings the loop back.

    /** The looper's one button: arm (the first chord starts recording), close the loop and play it,
        stop it, play it again from the top. */
    void loopButton()
    {
        switch (loopState)
        {
            case LoopState::Empty: setLoopState(LoopState::Armed); break;
            case LoopState::Armed: setLoopState(LoopState::Empty); break; // nothing was played
            case LoopState::Recording: closeLoop(); break;
            case LoopState::Playing:
                setLoopState(LoopState::Stopped);
                setLoopCode(-1);
                break;
            case LoopState::Stopped:
                setLoopState(LoopState::Playing);
                loopSeek(rhythmRunning ? std::fmod(clock, barBeats()) : 0.0); // from its first bar, on the drums' beat
                break;
        }
    }

    void loopClear()
    {
        setLoopState(LoopState::Empty);
        eventCount = 0;
        loopLength = 0.0;
        setLoopCode(-1);
    }

    void allNotesOff()
    {
        heldCount = 0;
        pendingCount = 0;
        updateChord();
        lastStrum = -1.0f;
    }

    // ---- state (readable from another thread, for the UI) -------------------------------------

    /** The chord being held, as root + 12 * type, or -1. */
    int heldChordCode() const { return uiHeld.load(std::memory_order_relaxed); }
    /** The chord the strings play: the last one held. */
    int stringChordCode() const { return uiMemory.load(std::memory_order_relaxed); }
    float stringGlow(int s) const { return uiGlow[(size_t) s].load(std::memory_order_relaxed); }

    Chord stringChord() const { return memory; }
    /** A chord is held, or the loop is playing one. */
    bool chordSounding() const { return heldCount > 0 || loopCode >= 0; }
    int pluckCount() const { return plucks; }
    /** The string plucked most recently (by hand or by the auto-strum), or -1. */
    int lastPluckedString() const { return lastString; }
    /** The rhythm machine's current step in its bar, or -1 while it is stopped. */
    int rhythmStep() const { return uiRhythmStep.load(std::memory_order_relaxed); }
    int drumHits() const { return rhythm.hitCount(); }
    /** The LFO's current value, -1..1 (0 while its depth is 0). */
    float lfoLevel() const { return uiLfo.load(std::memory_order_relaxed); }
    int loopStatus() const { return uiLoopState.load(std::memory_order_relaxed); } // LoopState
    /** The loop's length in beats; while recording, how long it has been recording. */
    float loopBeats() const { return uiLoopBeats.load(std::memory_order_relaxed); }
    /** Where the loop is, in beats from its start. */
    float loopPosition() const { return uiLoopPos.load(std::memory_order_relaxed); }

    static int codeOf(const Chord& c) { return c.root + 12 * static_cast<int>(c.type); }

    // ---- audio --------------------------------------------------------------------------------

    void process(float* left, float* right, int numSamples)
    {
        const float chordTarget = params.chordLevel * 0.17f;
        const float bassTarget = params.bassLevel * 0.45f;
        const float stringTarget = params.stringLevel * 0.24f;
        const float drumTarget = params.rhythmLevel * 0.5f;

        for (int i = 0; i < numSamples; ++i)
        {
            chordGain += (chordTarget - chordGain) * smooth;
            bassGain += (bassTarget - bassGain) * smooth;
            stringGain += (stringTarget - stringGain) * smooth;
            drumGain += (drumTarget - drumGain) * smooth;

            loopTick();
            rhythmTick();
            sequencerTick();
            advanceClock();
            const float pitchMul = wobbleStep();

            // The LFO bends the chords and strings only: the bass stays in tune and in place.
            const auto target = static_cast<LfoTarget>(std::clamp(params.lfoTarget, 0, kLfoTargets - 1));
            const float lfo = lfoStep() * params.lfoDepth; // -depth..depth
            float voicePitch = pitchMul, tone = params.chordTone, gain = 1.0f, pan = 0.0f;
            switch (target)
            {
                case LfoTarget::Vibrato: voicePitch *= std::exp2(lfo * 0.6f / 12.0f); break;
                case LfoTarget::Tremolo: gain = 1.0f - 0.5f * (params.lfoDepth + lfo); break;
                case LfoTarget::Filter: tone = std::clamp(tone + 0.5f * lfo, 0.0f, 1.0f); break;
                case LfoTarget::Pan: pan = lfo; break;
            }

            float pad = 0.0f;
            for (auto& p : pads)
                pad += p.process(voicePitch, tone);
            const float low = bass.process(pitchMul) * bassGain;

            // Strings spread across the stereo field, low on the left, high on the right.
            float sl = 0.0f, sr = 0.0f;
            for (int s = 0; s < kStrings; ++s)
            {
                auto& voice = strings[(size_t) s];
                if (!voice.active())
                    continue;
                const float v = voice.process(voicePitch);
                sl += v * panL[(size_t) s];
                sr += v * panR[(size_t) s];
            }

            const float gl = gain * (1.0f - std::max(pan, 0.0f)), gr = gain * (1.0f + std::min(pan, 0.0f));
            float l = (pad * chordGain + sl * stringGain) * gl;
            float r = (pad * chordGain + sr * stringGain) * gr;
            chorus.process(l, r, params.chorus);
            const float drums = rhythm.process() * drumGain;
            l += low + drums; // bass and drums stay dry and centred (only the reverb touches them)
            r += low + drums;
            reverb.process(l, r, params.reverb);

            left[i] = std::tanh(l * params.master * 1.2f);
            right[i] = std::tanh(r * params.master * 1.2f);
        }

        for (int s = 0; s < kStrings; ++s)
            uiGlow[(size_t) s].store(strings[(size_t) s].level(), std::memory_order_relaxed);
        uiLfo.store(lfoValue, std::memory_order_relaxed);
        publishLoop();
    }

    Engine()
    {
        for (int s = 0; s < kStrings; ++s)
        {
            const double pan = -0.6 + 1.2 * s / (kStrings - 1);
            const double angle = (pan + 1.0) * kTwoPi / 8.0;
            panL[(size_t) s] = (float) std::cos(angle);
            panR[(size_t) s] = (float) std::sin(angle);
        }
        for (auto& g : uiGlow)
            g.store(0.0f);
    }

private:
    /** Returns whether the note was held. */
    bool removeHeld(int note)
    {
        int w = 0;
        for (int r = 0; r < heldCount; ++r)
            if (held[(size_t) r] != note)
                held[(size_t) w++] = held[(size_t) r];
        const bool was = w != heldCount;
        heldCount = w;
        return was;
    }

    /** The chord held by hand, or -1. */
    int liveCode() const
    {
        Chord chord;
        return heldCount > 0 && chordForNote(held[(size_t) (heldCount - 1)], chord) ? codeOf(chord) : -1;
    }

    /** The newest held chord sounds; letting it go falls back to the one held before it, or to the loop. */
    void updateChord()
    {
        const int live = liveCode();
        if (live >= 0)
            recordChord(live);
        playChord(live >= 0 ? live : loopCode);
    }

    void playChord(int code)
    {
        if (code < 0)
        {
            for (auto& p : pads)
                p.stop();
            bass.stop();
            uiHeld.store(-1, std::memory_order_relaxed);
            return;
        }

        const Chord chord { code % 12, static_cast<ChordType>(code / 12) };
        memory = chord;

        int count = 0;
        const auto notes = padNotes(chord, count);
        for (int i = 0; i < (int) pads.size(); ++i)
        {
            if (i < count)
                pads[(size_t) i].start(notes[(size_t) i], rng.next());
            else
                pads[(size_t) i].stop();
        }
        bass.start(bassNote(chord));

        uiHeld.store(codeOf(chord), std::memory_order_relaxed);
        uiMemory.store(codeOf(chord), std::memory_order_relaxed);
    }

    // ---- chord looper internals ---------------------------------------------------------------

    void setLoopState(LoopState s)
    {
        loopState = s;
        publishLoop();
    }

    void publishLoop()
    {
        uiLoopState.store(static_cast<int>(loopState), std::memory_order_relaxed);
        uiLoopBeats.store((float) (loopState == LoopState::Recording ? loopTime - loopStart : loopLength), std::memory_order_relaxed);
        uiLoopPos.store((float) loopPos, std::memory_order_relaxed);
    }

    /** The current rhythm's bar, in beats (3 for the waltz, 4 for the rest). */
    double barBeats() const
    {
        const auto& pattern = rhythmPatterns()[(size_t) std::clamp(params.rhythmPattern, 0, kRhythmPatterns - 1)];
        return (double) pattern.length / pattern.stepsPerBeat;
    }

    /** While recording, notes each change of the held chord with when it happened. */
    void recordChord(int code)
    {
        if (loopState == LoopState::Armed)
        {
            setLoopState(LoopState::Recording);
            eventCount = 0;
            loopStart = loopTime;
            loopOnBars = rhythmRunning;
            if (loopOnBars) // start on the nearest bar line, so a chord pressed a bit early still counts as beat 1
            {
                const double bar = barBeats(), phase = std::fmod(clock, bar);
                loopStart += phase < bar / 2 ? -phase : bar - phase;
            }
        }
        if (loopState != LoopState::Recording || eventCount == (int) events.size())
            return;
        if (eventCount > 0 && events[(size_t) (eventCount - 1)].code == code)
            return;
        events[(size_t) eventCount++] = { loopTime - loopStart, code };
    }

    /** Ends the recording: the loop gets its length (whole bars with the drums, whole beats with the
        auto-strum) and plays on from where it is, without a gap. */
    void closeLoop()
    {
        const double elapsed = loopTime - loopStart;
        const double unit = loopOnBars ? barBeats() : params.seqOn ? 1.0 : 0.0;
        loopLength = unit > 0.0 ? std::max(1.0, std::round(elapsed / unit)) * unit : std::max(elapsed, 0.25);

        // Changes after the end are dropped; one pressed just before the first bar line wraps to the end.
        int kept = 0;
        for (int i = 0; i < eventCount; ++i)
        {
            auto e = events[(size_t) i];
            if (e.pos >= loopLength)
                continue;
            if (e.pos < 0.0)
                e.pos += loopLength;
            events[(size_t) kept++] = e;
        }
        eventCount = kept;
        std::sort(events.begin(), events.begin() + eventCount, [](const LoopEvent& a, const LoopEvent& b) { return a.pos < b.pos; });
        if (eventCount == 0)
        {
            loopClear();
            return;
        }

        // The loop takes over from the hand: let go of the held chords (hold mode would keep them).
        heldCount = 0;
        loopSeek(elapsed - std::floor(elapsed / loopLength) * loopLength);
        setLoopState(LoopState::Playing);
    }

    /** Jumps the loop to `pos` beats and sounds the chord that belongs there. */
    void loopSeek(double pos)
    {
        loopPos = pos;
        nextEvent = 0;
        while (nextEvent < eventCount && events[(size_t) nextEvent].pos <= pos)
            ++nextEvent;
        // Before the first change, the last chord of the loop is still sounding.
        const int at = nextEvent > 0 ? nextEvent - 1 : eventCount - 1;
        loopCode = -2; // force the chord to start
        setLoopCode(events[(size_t) at].code);
    }

    void setLoopCode(int code)
    {
        if (code == loopCode)
            return;
        loopCode = code;
        if (heldCount == 0) // a chord held by hand stays on top
            playChord(code);
    }

    void loopTick()
    {
        const double step = 1.0 / samplesPerBeat();
        loopTime += step;
        if (loopState != LoopState::Playing)
            return;
        loopPos += step;
        if (loopPos >= loopLength)
        {
            loopPos -= loopLength;
            nextEvent = 0;
        }
        while (nextEvent < eventCount && events[(size_t) nextEvent].pos <= loopPos)
            setLoopCode(events[(size_t) nextEvent++].code);
    }

    // ---- auto-strum ---------------------------------------------------------------------------

    /** Runs once per sample: fires due strum plucks, and steps the sequencer while a chord is held. */
    void sequencerTick()
    {
        for (int i = 0; i < pendingCount;)
        {
            auto& p = pending[(size_t) i];
            if (--p.delay > 0)
            {
                ++i;
                continue;
            }
            pluck(p.string, p.velocity);
            p = pending[(size_t) --pendingCount];
        }

        if (!params.seqOn || !chordSounding())
        {
            seqRunning = false;
            return;
        }
        const int rate = std::clamp(params.seqRate, 0, kSeqRates - 1);
        const int perBeat = kStepsPerBeat[rate];

        if (rhythmRunning)
        {
            // Locked to the drums: steps land on the clock's grid. A chord pressed between grid
            // points waits for the next one, so the strings never drift from the beat.
            const double gridPos = clock * perBeat;
            if (!seqRunning)
            {
                seqRunning = true;
                seqStep = 0;
                nextGrid = (long long) std::ceil(gridPos - 1e-9);
            }
            if (gridPos >= (double) nextGrid)
            {
                fireStep(nextGrid % perBeat == 0);
                ++seqStep;
                nextGrid = (long long) std::floor(gridPos) + 1;
            }
            return;
        }

        if (!seqRunning) // a chord was just pressed: start on the beat, right now
        {
            seqRunning = true;
            seqStep = 0;
            seqCountdown = 0.0;
        }
        if (seqCountdown <= 0.0)
        {
            fireStep(seqStep % perBeat == 0);
            ++seqStep;
            seqCountdown += samplesPerBeat() / perBeat;
        }
        seqCountdown -= 1.0;
    }

    double samplesPerBeat() const { return 60.0 / std::clamp((double) params.seqBpm, 20.0, 400.0) * sampleRate; }

    // ---- rhythm machine and its clock -------------------------------------------------------

    /** Plays the drums for the current step when the clock reaches it. */
    void rhythmTick()
    {
        if (!params.rhythmOn)
        {
            if (rhythmRunning)
            {
                rhythmRunning = false;
                rhythm.stopAll(); // cut the drums, don't let them ring on
                seqRunning = false; // the auto-strum goes back to its own timing
                uiRhythmStep.store(-1, std::memory_order_relaxed);
            }
            return;
        }

        const int index = std::clamp(params.rhythmPattern, 0, kRhythmPatterns - 1);
        const auto& pattern = rhythmPatterns()[(size_t) index];
        if (!rhythmRunning) // start: bar 1, beat 1, right now
        {
            rhythmRunning = true;
            seqRunning = false; // re-lock the auto-strum to the new clock
            clock = 0.0;
            nextDrumStep = 0;
            currentPattern = index;
            if (loopState == LoopState::Playing)
                loopSeek(0.0); // the loop starts over with the drums
        }
        else if (index != currentPattern)
        {
            // A new rhythm takes over from the next step of its own grid (triplets or sixteenths).
            currentPattern = index;
            nextDrumStep = (long long) std::ceil(clock * pattern.stepsPerBeat - 1e-9);
        }

        if (clock * pattern.stepsPerBeat >= (double) nextDrumStep)
        {
            const int step = (int) (nextDrumStep % pattern.length);
            rhythm.playStep(pattern, step);
            uiRhythmStep.store(step, std::memory_order_relaxed);
            ++nextDrumStep;
        }
    }

    void advanceClock()
    {
        if (rhythmRunning)
            clock += 1.0 / samplesPerBeat();
    }

    void fireStep(bool onBeat)
    {
        const int range = std::clamp(params.seqRange, 2, kStrings);
        const float velocity = onBeat ? 0.95f : 0.7f; // accent the first step of each beat

        switch (static_cast<SeqPattern>(std::clamp(params.seqPattern, 0, kSeqPatterns - 1)))
        {
            case SeqPattern::Up: pluck(seqStep % range, velocity); break;
            case SeqPattern::Down: pluck(range - 1 - seqStep % range, velocity); break;
            case SeqPattern::UpDown:
            {
                const int period = 2 * range - 2;
                const int pos = seqStep % period;
                pluck(pos < range ? pos : period - pos, velocity);
                break;
            }
            case SeqPattern::Strum:
            {
                // A quick sweep across the strings, 9 ms apart: down strokes on even steps, softer up strokes on odd ones.
                const bool down = seqStep % 2 == 0;
                const int spread = std::max(1, (int) (0.009 * sampleRate));
                for (int i = 0; i < range && pendingCount < (int) pending.size(); ++i)
                    pending[(size_t) pendingCount++] = { 1 + i * spread, down ? i : range - 1 - i, down ? velocity : velocity * 0.75f };
                break;
            }
            case SeqPattern::Random:
            {
                int s = (int) (rng.next() * (float) range);
                if (s == lastString && range > 1)
                    s = (s + 1 + (int) (rng.next() * (float) (range - 1))) % range; // never the same string twice
                pluck(s, velocity);
                break;
            }
        }
    }

    /** Tape-like pitch: a slow wow plus a wandering drift, up to a quarter semitone. */
    float wobbleStep()
    {
        if (params.wobble <= 0.0f)
            return 1.0f;
        wowPhase += 0.7 / sampleRate;
        if (wowPhase >= 1.0)
            wowPhase -= 1.0;
        if (--driftCountdown <= 0)
        {
            driftTarget = rng.next() * 2.0f - 1.0f;
            driftCountdown = (int) (0.4 * sampleRate);
        }
        drift += (driftTarget - drift) * (float) (1.0 / (0.3 * sampleRate));
        const float semitones = params.wobble * 0.25f * (0.6f * (float) std::sin(kTwoPi * wowPhase) + 0.4f * drift);
        return std::exp2(semitones / 12.0f);
    }

    /** The LFO's next value, -1..1, slightly smoothed so square and random waves don't click. */
    float lfoStep()
    {
        if (params.lfoDepth <= 0.0f)
            return lfoValue = 0.0f;
        lfoPhase += std::clamp(params.lfoRate, 0.05f, 30.0f) / sampleRate;
        const auto shape = static_cast<LfoShape>(std::clamp(params.lfoShape, 0, kLfoShapes - 1));
        if (lfoPhase >= 1.0)
        {
            lfoPhase -= 1.0;
            if (shape == LfoShape::Random) // only then: other shapes leave the random auto-strum untouched
                lfoHeld = rng.next() * 2.0f - 1.0f;
        }
        const float p = (float) lfoPhase;
        float raw = 0.0f;
        switch (shape)
        {
            case LfoShape::Sine: raw = (float) std::sin(kTwoPi * lfoPhase); break;
            case LfoShape::Triangle: raw = p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f; break;
            case LfoShape::Square: raw = p < 0.5f ? 1.0f : -1.0f; break;
            case LfoShape::Random: raw = lfoHeld; break;
        }
        lfoValue += (raw - lfoValue) * lfoSmooth;
        return lfoValue;
    }

    double sampleRate = 48000.0;
    std::array<PadVoice, 4> pads {};
    BassVoice bass;
    std::array<StringVoice, kStrings> strings {};
    std::array<float, kStrings> panL {}, panR {};
    Chorus chorus;
    Reverb reverb;
    Rng rng;

    std::array<int, 16> held {};
    int heldCount = 0;
    Chord memory {}; // C major until the first chord
    float lastStrum = -1.0f;
    int plucks = 0, lastString = -1;

    struct PendingPluck
    {
        int delay, string;
        float velocity;
    };
    std::array<PendingPluck, 64> pending {};
    int pendingCount = 0;
    bool seqRunning = false;
    int seqStep = 0;
    double seqCountdown = 0.0;
    long long nextGrid = 0;

    RhythmBox rhythm;
    bool rhythmRunning = false;
    double clock = 0.0; // beats since the rhythm machine started
    long long nextDrumStep = 0;
    int currentPattern = 0;
    float drumGain = 0.0f;

    float smooth = 0.001f, chordGain = 0.0f, bassGain = 0.0f, stringGain = 0.0f;
    double wowPhase = 0.0;
    float drift = 0.0f, driftTarget = 0.0f;
    int driftCountdown = 0;
    double lfoPhase = 0.0;
    float lfoValue = 0.0f, lfoHeld = 0.0f, lfoSmooth = 0.01f;

    struct LoopEvent
    {
        double pos; // beats from the loop's start
        int code; // chord
    };
    std::array<LoopEvent, 256> events {};
    int eventCount = 0, nextEvent = 0, loopCode = -1;
    LoopState loopState = LoopState::Empty;
    double loopTime = 0.0; // beats, always running: the looper's own clock
    double loopStart = 0.0, loopLength = 0.0, loopPos = 0.0;
    bool loopOnBars = false;

    std::atomic<int> uiHeld { -1 }, uiMemory { 0 }, uiRhythmStep { -1 }, uiLoopState { 0 };
    std::atomic<float> uiLfo { 0.0f }, uiLoopBeats { 0.0f }, uiLoopPos { 0.0f };
    std::array<std::atomic<float>, kStrings> uiGlow;
};
} // namespace sdchord
