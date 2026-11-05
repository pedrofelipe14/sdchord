#pragma once

// A small preset rhythm machine, in the spirit of the 1970s-80s rhythm boxes: every drum is a
// synthesised analogue-style voice (no samples) and each rhythm is a fixed one-bar pattern.
// The engine's clock decides when each step plays; this file only knows what a step sounds like.

#include <array>
#include <cmath>

#include "voices.h"

namespace sdchord
{
enum Drum
{
    Kick,
    Snare,
    Rim,
    ClosedHat,
    OpenHat,
    LowConga,
    HighConga,
    Cowbell,
    kDrums,
};

struct RhythmPattern
{
    const char* name;
    int stepsPerBeat; // 4 = sixteenths; 3 = triplets (swing)
    int length; // steps in one bar
    /** One character per step for each drum: 'X' accented hit, 'x' normal hit, '.' rest. */
    const char* hits[kDrums];
};

constexpr int kRhythmPatterns = 8;

inline const std::array<RhythmPattern, kRhythmPatterns>& rhythmPatterns()
{
    // Columns:          Kick                Snare               Rim                 Closed hat          Open hat            Low conga           High conga          Cowbell
    static const std::array<RhythmPattern, kRhythmPatterns> patterns { {
        { "Rock", 4, 16, { "X.....x.X.......", "....X.......X...", "................", "X.x.X.x.X.x.X.x.", "................", "................", "................", "................" } },
        { "Disco", 4, 16, { "X...X...X...X...", "....X.......X...", "................", "x...x...x...x...", "..X...X...X...X.", "................", "................", "................" } },
        { "Balada", 4, 16, { "X......xX.......", "....X.......X...", "................", "XxxxXxxxXxxxXxxx", "................", "................", "................", "................" } },
        { "Bossa Nova", 4, 16, { "X..xX..xX..xX..x", "................", "X..X..X...X..X..", "xxxxxxxxxxxxxxxx", "................", "................", "................", "................" } },
        { "Cumbia", 4, 16, { "X.......X.......", "................", "................", "x.xxx.xxx.xxx.xx", "................", "..x.X.....x.X...", "x...x.x.x...x.x.", "....X.......X..." } },
        { "Rumba", 4, 16, { "X..x....X..x....", "................", "X..X..X...X.X...", "x.x.x.x.x.x.x.x.", "................", "......xX......xX", "..x.x.....x.x...", "................" } },
        { "Vals", 4, 12, { "X...........", "....x...x...", "............", "X.x.x.x.x.x.", "............", "............", "............", "............" } },
        { "Swing", 3, 12, { "x..x..x..x..", "............", "...X.....X..", "X..x.xX..x.x", "............", "............", "............", "............" } },
    } };
    return patterns;
}

/** One percussive voice. Each drum sets its own recipe in trigger(); process() mixes a pitched
    body (with a falling pitch), filtered noise and an optional square pair (the cowbell). */
class DrumVoice
{
public:
    void prepare(double sr) { sampleRate = sr; }

    void trigger(Drum drum, float velocity)
    {
        amp = velocity;
        t = 0.0;
        switch (drum)
        {
            case Kick: set(52.0, 110.0, 0.03, 0.13, 0.0, 0.0, 0.0); break;
            case Snare: set(185.0, 30.0, 0.02, 0.07, 0.75, 0.065, 1800.0); break;
            case Rim: set(1750.0, 0.0, 1.0, 0.012, 0.15, 0.008, 3000.0); break;
            case ClosedHat: set(0.0, 0.0, 1.0, 0.001, 0.6, 0.018, 7000.0); break;
            case OpenHat: set(0.0, 0.0, 1.0, 0.001, 0.55, 0.16, 7000.0); break;
            case LowConga: set(190.0, 25.0, 0.04, 0.12, 0.0, 0.0, 0.0); break;
            case HighConga: set(310.0, 30.0, 0.03, 0.08, 0.0, 0.0, 0.0); break;
            case Cowbell:
                set(0.0, 0.0, 1.0, 0.001, 0.0, 0.0, 0.0);
                bellDecay = 0.06;
                break;
            default: break;
        }
        bell = drum == Cowbell;
        sounding = true;
    }

    /** A closed hi-hat cuts an open one short, like on the real machines. */
    void choke() { chokeLeft = (int) (0.004 * sampleRate); }

    bool active() const { return sounding; }

    float process(Rng& rng)
    {
        if (!sounding)
            return 0.0f;
        const double dt = 1.0 / sampleRate;
        float out = 0.0f;

        if (toneHz > 0.0)
        {
            // Body: a sine whose pitch falls from toneHz + sweepHz down to toneHz.
            const double hz = toneHz + sweepHz * std::exp(-t / sweepTau);
            phase += hz * dt;
            if (phase >= 1.0)
                phase -= 1.0;
            out += (float) (std::sin(kTwoPi * phase) * std::exp(-t / toneTau));
        }
        if (noiseMix > 0.0f)
        {
            // Noise through a one-pole high-pass: hiss for hats, rattle for the snare.
            const float n = rng.next() * 2.0f - 1.0f;
            low += (n - low) * hpCoef;
            out += (n - low) * noiseMix * (float) std::exp(-t / noiseTau);
        }
        if (bell)
        {
            // Two detuned squares, softened: the classic analogue cowbell.
            bellA += 545.0 * dt;
            bellB += 815.0 * dt;
            bellA -= std::floor(bellA);
            bellB -= std::floor(bellB);
            const float sq = (bellA < 0.5 ? 0.5f : -0.5f) + (bellB < 0.5 ? 0.5f : -0.5f);
            lp += (sq - lp) * 0.35f;
            out += lp * (float) std::exp(-t / bellDecay);
        }

        float gain = amp;
        if (chokeLeft > 0)
        {
            gain *= (float) chokeLeft / (float) (0.004 * sampleRate);
            if (--chokeLeft == 0)
                sounding = false;
        }

        t += dt;
        if (t > 8.0 * std::fmax(toneTau, std::fmax(noiseTau, bell ? bellDecay : 0.0)))
            sounding = false; // below -70 dB
        return out * gain;
    }

private:
    void set(double hz, double sweep, double sweepT, double toneT, double noise, double noiseT, double hpHz)
    {
        toneHz = hz;
        sweepHz = sweep;
        sweepTau = sweepT;
        toneTau = toneT;
        noiseMix = (float) noise;
        noiseTau = noiseT > 0.0 ? noiseT : 0.001;
        hpCoef = hpHz > 0.0 ? (float) (1.0 - std::exp(-kTwoPi * hpHz / sampleRate)) : 0.0f;
        chokeLeft = 0;
        phase = 0.0;
    }

    double sampleRate = 48000.0, t = 0.0, phase = 0.0;
    double toneHz = 0.0, sweepHz = 0.0, sweepTau = 0.03, toneTau = 0.1, noiseTau = 0.05, bellDecay = 0.06;
    double bellA = 0.0, bellB = 0.25;
    float amp = 0.0f, noiseMix = 0.0f, hpCoef = 0.0f, low = 0.0f, lp = 0.0f;
    int chokeLeft = 0;
    bool bell = false, sounding = false;
};

/** The drum kit: plays one step of a pattern when told to, and mixes its voices. */
class RhythmBox
{
public:
    void prepare(double sr)
    {
        for (auto& v : voices)
            v.prepare(sr);
    }

    /** Plays whatever the pattern has at this step. */
    void playStep(const RhythmPattern& pattern, int step)
    {
        for (int d = 0; d < kDrums; ++d)
        {
            const char c = pattern.hits[d][step % pattern.length];
            if (c != 'x' && c != 'X')
                continue;
            if (d == ClosedHat && voices[OpenHat].active())
                voices[OpenHat].choke();
            voices[(size_t) d].trigger(static_cast<Drum>(d), c == 'X' ? 1.0f : 0.6f);
            ++hits;
        }
    }

    float process()
    {
        float out = 0.0f;
        for (int d = 0; d < kDrums; ++d)
            if (voices[(size_t) d].active())
                out += voices[(size_t) d].process(rng) * kLevels[d];
        return out;
    }

    /** Stops every drum that is still ringing, with a few milliseconds' fade so it doesn't click. */
    void stopAll()
    {
        for (auto& v : voices)
            if (v.active())
                v.choke();
    }

    int hitCount() const { return hits; }
    bool sounding(Drum d) const { return voices[(size_t) d].active(); }

private:
    // Balance between the drums, so the default kit sits under the chords.
    static constexpr float kLevels[kDrums] = { 1.0f, 0.55f, 0.35f, 0.22f, 0.2f, 0.5f, 0.45f, 0.3f };

    std::array<DrumVoice, kDrums> voices {};
    Rng rng { 0x1234567u };
    int hits = 0;
};
} // namespace sdchord
