#pragma once

// The three sound sources: the sustained chord (organ-like), the bass, and the plucked strings
// of the strum plate (a small FM bell whose brightness fades faster than its body).

#include <cmath>
#include <cstdint>

namespace sdchord
{
constexpr double kTwoPi = 6.283185307179586;

inline double midiToHz(double note) { return 440.0 * std::pow(2.0, (note - 69.0) / 12.0); }

/** One-pole smoothing coefficient: reaches ~63 % of a step in `seconds`. */
inline float coefFor(double seconds, double sampleRate)
{
    return seconds <= 0.0 ? 1.0f : (float) (1.0 - std::exp(-1.0 / (seconds * sampleRate)));
}

/** Corrects the jump of a naive saw/pulse so it doesn't alias (polyBLEP). */
inline float polyBlep(double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return (float) (t + t - t * t - 1.0);
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return (float) (t * t + t + t + 1.0);
    }
    return 0.0f;
}

/** Cheap deterministic noise, so voices start at different phases without std::rand. */
struct Rng
{
    uint32_t state = 0x2545F491u;
    float next() // [0, 1)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (state >> 8) * (1.0f / 16777216.0f);
    }
};

/** One note of the sustained chord: a triangle with some pulse mixed in by "tone", softened by a low-pass. */
class PadVoice
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        attack = coefFor(0.012, sr);
        release = coefFor(0.18, sr);
    }

    void start(int note, float startPhase)
    {
        if (env < 1.0e-4f)
            phase = startPhase; // only when silent: retuning a sounding voice must not click
        hz = midiToHz(note);
        gate = true;
    }

    void stop() { gate = false; }
    bool active() const { return gate || env > 1.0e-5f; }

    float process(float pitchMul, float tone)
    {
        env += ((gate ? 1.0f : 0.0f) - env) * (gate ? attack : release);
        if (!active())
            return 0.0f;

        const double dt = hz * pitchMul / sampleRate;
        phase += dt;
        if (phase >= 1.0)
            phase -= 1.0;

        const float tri = 4.0f * std::fabs((float) phase - 0.5f) - 1.0f;
        float pulse = phase < 0.5 ? 1.0f : -1.0f;
        pulse += polyBlep(phase, dt);
        pulse -= polyBlep(std::fmod(phase + 0.5, 1.0), dt);

        const float raw = tri * (1.0f - 0.55f * tone) + pulse * 0.45f * tone;
        // Brighter tone opens the filter: ~600 Hz to ~6 kHz.
        const float cutoff = 600.0f * std::pow(10.0f, tone);
        lp += (raw - lp) * (float) (1.0 - std::exp(-kTwoPi * cutoff / sampleRate));
        return lp * env;
    }

private:
    double sampleRate = 48000.0, hz = 220.0, phase = 0.0;
    float env = 0.0f, attack = 0.0f, release = 0.0f, lp = 0.0f;
    bool gate = false;
};

/** The bass: a sine with a little filtered saw on top, so it still reads on small speakers. */
class BassVoice
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        attack = coefFor(0.006, sr);
        release = coefFor(0.12, sr);
    }

    void start(int note)
    {
        hz = midiToHz(note);
        gate = true;
    }

    void stop() { gate = false; }

    float process(float pitchMul)
    {
        env += ((gate ? 1.0f : 0.0f) - env) * (gate ? attack : release);
        if (!gate && env < 1.0e-5f)
            return 0.0f;

        const double dt = hz * pitchMul / sampleRate;
        phase += dt;
        if (phase >= 1.0)
            phase -= 1.0;

        float saw = (float) (2.0 * phase - 1.0);
        saw -= polyBlep(phase, dt);
        lp += (saw - lp) * (float) (1.0 - std::exp(-kTwoPi * 400.0 / sampleRate));
        return ((float) std::sin(kTwoPi * phase) * 0.8f + lp * 0.35f) * env;
    }

private:
    double sampleRate = 48000.0, hz = 55.0, phase = 0.0;
    float env = 0.0f, attack = 0.0f, release = 0.0f, lp = 0.0f;
    bool gate = false;
};

/** A plucked string of the strum plate: two-operator FM. The modulation index decays faster than
    the amplitude, so each pluck starts bright and rings out mellow. */
class StringVoice
{
public:
    void prepare(double sr) { sampleRate = sr; }

    void pluck(int note, float velocity, float decaySeconds, float brightness)
    {
        hz = midiToHz(note);
        amp = velocity;
        env = 0.0f;
        attackLeft = (int) (0.0015 * sampleRate); // 1.5 ms: no click, still percussive
        attackStep = attackLeft > 0 ? 1.0f / (float) attackLeft : 1.0f;
        decay = (float) std::exp(-6.9 / (decaySeconds * sampleRate)); // -60 dB after decaySeconds
        index = 0.4f + 2.6f * brightness;
        indexDecay = (float) std::exp(-1.0 / (0.12 * sampleRate));
        sounding = true;
    }

    bool active() const { return sounding; }
    /** How loud the string is now (for the plate's glow). */
    float level() const { return sounding ? env * amp : 0.0f; }

    float process(float pitchMul)
    {
        if (!sounding)
            return 0.0f;

        if (attackLeft > 0)
        {
            env = std::fmin(1.0f, env + attackStep);
            --attackLeft;
        }
        else
        {
            env *= decay;
            if (env < 1.0e-4f)
            {
                sounding = false;
                return 0.0f;
            }
        }

        const double inc = hz * pitchMul / sampleRate;
        carrier += inc;
        modulator += inc * 2.0; // ratio 2: hollow, glassy partials
        if (carrier >= 1.0)
            carrier -= 1.0;
        if (modulator >= 1.0)
            modulator -= 1.0;

        index *= indexDecay;
        const double mod = index * std::sin(kTwoPi * modulator);
        return (float) std::sin(kTwoPi * carrier + mod) * env * amp;
    }

private:
    double sampleRate = 48000.0, hz = 440.0, carrier = 0.0, modulator = 0.0;
    float amp = 0.0f, env = 0.0f, attackStep = 1.0f, decay = 0.999f, index = 1.0f, indexDecay = 0.999f;
    int attackLeft = 0;
    bool sounding = false;
};
} // namespace sdchord
