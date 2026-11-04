#pragma once

// Stereo chorus and a small Freeverb-style reverb. Buffers are fixed-size so nothing allocates
// while audio runs (and the WebAssembly build needs no heap growth).

#include <array>
#include <cmath>

#include "voices.h"

namespace sdchord
{
/** Two modulated delay taps, a quarter cycle apart: one per side. */
class Chorus
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        buffer.fill(0.0f);
        write = 0;
    }

    void process(float& left, float& right, float amount)
    {
        const float mono = 0.5f * (left + right);
        buffer[write] = mono;

        lfo += 0.55 / sampleRate;
        if (lfo >= 1.0)
            lfo -= 1.0;

        const float wetL = tap(lfo);
        const float wetR = tap(lfo + 0.25);
        write = (write + 1) & (kSize - 1);

        const float mix = 0.5f * amount;
        left = left * (1.0f - mix) + wetL * mix;
        right = right * (1.0f - mix) + wetR * mix;
    }

private:
    static constexpr int kSize = 8192; // ~170 ms at 48 kHz, plenty for 7-25 ms

    float tap(double phase) const
    {
        const double ms = 14.0 + 6.0 * std::sin(kTwoPi * phase);
        const double delay = ms * 0.001 * sampleRate;
        double readPos = (double) write - delay;
        while (readPos < 0.0)
            readPos += kSize;
        const int i0 = (int) readPos;
        const float frac = (float) (readPos - i0);
        const float a = buffer[(size_t) (i0 & (kSize - 1))];
        const float b = buffer[(size_t) ((i0 + 1) & (kSize - 1))];
        return a + (b - a) * frac;
    }

    std::array<float, kSize> buffer {};
    int write = 0;
    double sampleRate = 48000.0, lfo = 0.0;
};

/** Freeverb, trimmed to 4 combs and 2 allpasses per side. */
class Reverb
{
public:
    void prepare(double sr)
    {
        const double scale = sr / 44100.0;
        static constexpr int combTuning[4] = { 1116, 1188, 1277, 1356 };
        static constexpr int allpassTuning[2] = { 556, 441 };
        constexpr int spread = 23;
        for (int i = 0; i < 4; ++i)
        {
            combs[0][(size_t) i].setLength((int) (combTuning[i] * scale));
            combs[1][(size_t) i].setLength((int) ((combTuning[i] + spread) * scale));
        }
        for (int i = 0; i < 2; ++i)
        {
            allpasses[0][(size_t) i].setLength((int) (allpassTuning[i] * scale));
            allpasses[1][(size_t) i].setLength((int) ((allpassTuning[i] + spread) * scale));
        }
    }

    void process(float& left, float& right, float amount)
    {
        if (amount <= 0.0f)
            return;
        const float input = (left + right) * 0.015f;
        float out[2] = { 0.0f, 0.0f };
        for (int side = 0; side < 2; ++side)
        {
            for (auto& c : combs[(size_t) side])
                out[side] += c.process(input, 0.84f, 0.3f);
            for (auto& a : allpasses[(size_t) side])
                out[side] = a.process(out[side]);
        }
        const float wet = amount * 0.9f;
        left = left * (1.0f - 0.35f * amount) + out[0] * wet;
        right = right * (1.0f - 0.35f * amount) + out[1] * wet;
    }

private:
    static constexpr int kMax = 4096; // longest tuning at 96 kHz fits

    struct Comb
    {
        std::array<float, kMax> buf {};
        int length = 1000, pos = 0;
        float store = 0.0f;
        void setLength(int n)
        {
            length = n < kMax ? n : kMax;
            buf.fill(0.0f);
            pos = 0;
            store = 0.0f;
        }
        float process(float in, float feedback, float damp)
        {
            const float out = buf[(size_t) pos];
            store = out * (1.0f - damp) + store * damp;
            buf[(size_t) pos] = in + store * feedback;
            if (++pos >= length)
                pos = 0;
            return out;
        }
    };

    struct Allpass
    {
        std::array<float, kMax> buf {};
        int length = 500, pos = 0;
        void setLength(int n)
        {
            length = n < kMax ? n : kMax;
            buf.fill(0.0f);
            pos = 0;
        }
        float process(float in)
        {
            const float delayed = buf[(size_t) pos];
            buf[(size_t) pos] = in + delayed * 0.5f;
            if (++pos >= length)
                pos = 0;
            return delayed - in;
        }
    };

    std::array<std::array<Comb, 4>, 2> combs {};
    std::array<std::array<Allpass, 2>, 2> allpasses {};
};
} // namespace sdchord
