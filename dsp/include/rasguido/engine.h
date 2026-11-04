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
#include "voices.h"

namespace rasguido
{
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
        smooth = coefFor(0.02, sr);
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
        if (chordForNote(note, chord))
        {
            removeHeld(note);
            updateChord();
        }
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
    }

    void allNotesOff()
    {
        heldCount = 0;
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
    bool chordSounding() const { return heldCount > 0; }
    int pluckCount() const { return plucks; }

    static int codeOf(const Chord& c) { return c.root + 12 * static_cast<int>(c.type); }

    // ---- audio --------------------------------------------------------------------------------

    void process(float* left, float* right, int numSamples)
    {
        const float chordTarget = params.chordLevel * 0.17f;
        const float bassTarget = params.bassLevel * 0.45f;
        const float stringTarget = params.stringLevel * 0.24f;

        for (int i = 0; i < numSamples; ++i)
        {
            chordGain += (chordTarget - chordGain) * smooth;
            bassGain += (bassTarget - bassGain) * smooth;
            stringGain += (stringTarget - stringGain) * smooth;

            const float pitchMul = wobbleStep();

            float pad = 0.0f;
            for (auto& p : pads)
                pad += p.process(pitchMul, params.chordTone);
            const float low = bass.process(pitchMul) * bassGain;

            // Strings spread across the stereo field, low on the left, high on the right.
            float sl = 0.0f, sr = 0.0f;
            for (int s = 0; s < kStrings; ++s)
            {
                auto& voice = strings[(size_t) s];
                if (!voice.active())
                    continue;
                const float v = voice.process(pitchMul);
                sl += v * panL[(size_t) s];
                sr += v * panR[(size_t) s];
            }

            float l = pad * chordGain + sl * stringGain;
            float r = pad * chordGain + sr * stringGain;
            chorus.process(l, r, params.chorus);
            l += low; // the bass stays dry and centred
            r += low;
            reverb.process(l, r, params.reverb);

            left[i] = std::tanh(l * params.master * 1.2f);
            right[i] = std::tanh(r * params.master * 1.2f);
        }

        for (int s = 0; s < kStrings; ++s)
            uiGlow[(size_t) s].store(strings[(size_t) s].level(), std::memory_order_relaxed);
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
    void removeHeld(int note)
    {
        int w = 0;
        for (int r = 0; r < heldCount; ++r)
            if (held[(size_t) r] != note)
                held[(size_t) w++] = held[(size_t) r];
        heldCount = w;
    }

    /** The newest held chord sounds; letting it go falls back to the one held before it. */
    void updateChord()
    {
        if (heldCount == 0)
        {
            for (auto& p : pads)
                p.stop();
            bass.stop();
            uiHeld.store(-1, std::memory_order_relaxed);
            return;
        }

        Chord chord;
        chordForNote(held[(size_t) (heldCount - 1)], chord);
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
    int plucks = 0;

    float smooth = 0.001f, chordGain = 0.0f, bassGain = 0.0f, stringGain = 0.0f;
    double wowPhase = 0.0;
    float drift = 0.0f, driftTarget = 0.0f;
    int driftCountdown = 0;

    std::atomic<int> uiHeld { -1 }, uiMemory { 0 };
    std::array<std::atomic<float>, kStrings> uiGlow;
};
} // namespace rasguido
