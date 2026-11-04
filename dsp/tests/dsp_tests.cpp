// Native tests for the instrument's DSP core. They run on every push, before the same code is built
// into the plugin (and, later, WebAssembly).

#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "sdchord/engine.h"

using namespace sdchord;

namespace
{
struct TestCase
{
    const char* name;
    std::function<void()> run;
};

std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

struct Register
{
    Register(const char* name, std::function<void()> fn) { registry().push_back({ name, std::move(fn) }); }
};

int failures = 0;

void check(bool condition, const char* expr, const char* file, int line)
{
    if (!condition)
    {
        ++failures;
        std::printf("    FAILED: %s (%s:%d)\n", expr, file, line);
    }
}

#define TEST(name) \
    static void name(); \
    static Register reg_##name(#name, name); \
    static void name()
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

constexpr double sampleRate = 48000.0;

/** The engine is big (reverb buffers), so tests keep it on the heap. */
std::unique_ptr<Engine> makeEngine(double sr = sampleRate)
{
    auto e = std::make_unique<Engine>();
    e->prepare(sr);
    return e;
}

/** Renders `seconds` of audio and returns the RMS of both channels together. */
float render(Engine& e, double seconds, double sr = sampleRate, bool* finite = nullptr)
{
    const int total = (int) (seconds * sr);
    std::vector<float> l(256), r(256);
    double sum = 0.0;
    bool ok = true;
    for (int done = 0; done < total; done += 256)
    {
        const int n = std::min(256, total - done);
        e.process(l.data(), r.data(), n);
        for (int i = 0; i < n; ++i)
        {
            ok = ok && std::isfinite(l[(size_t) i]) && std::isfinite(r[(size_t) i]);
            sum += l[(size_t) i] * l[(size_t) i] + r[(size_t) i] * r[(size_t) i];
        }
    }
    if (finite)
        *finite = ok;
    return (float) std::sqrt(sum / (2.0 * std::max(1, total)));
}

const Chord cMajor { 0, ChordType::Major }, gMajor { 7, ChordType::Major }, aMinor { 9, ChordType::Minor };
} // namespace

// ---- chords and layout ------------------------------------------------------------------------

TEST(strings_climb_through_the_chord_tones)
{
    const auto notes = stringNotes(cMajor);
    CHECK(notes[0] == 48); // C3
    CHECK(notes[1] == 52); // E3
    CHECK(notes[2] == 55); // G3
    CHECK(notes[3] == 60);
    for (int s = 1; s < kStrings; ++s)
        CHECK(notes[(size_t) s] > notes[(size_t) (s - 1)]);
    for (int n : notes)
        CHECK(isChordTone(cMajor, n));
}

TEST(seventh_chords_put_the_seventh_on_the_strings)
{
    const Chord g7 { 7, ChordType::Seventh };
    const auto notes = stringNotes(g7);
    bool hasF = false;
    for (int n : notes)
        hasF = hasF || n % 12 == 5;
    CHECK(hasF);
    CHECK(notes[0] == 50); // D3 is the first G7 tone at or above C3
}

TEST(pad_voicing_stays_in_one_octave_range)
{
    for (int root = 0; root < 12; ++root)
        for (int t = 0; t < kChordTypes; ++t)
        {
            int count = 0;
            const auto notes = padNotes({ root, static_cast<ChordType>(t) }, count);
            CHECK(count == (t >= 2 ? 4 : 3));
            CHECK(notes[0] >= 53 && notes[0] <= 64); // root between F3 and E4
            CHECK(bassNote({ root, static_cast<ChordType>(t) }) % 12 == root);
        }
}

TEST(midi_layout_round_trips)
{
    Chord c;
    CHECK(chordForNote(24, c) && c == cMajor);
    CHECK(chordForNote(36 + 9, c) && c == aMinor);
    CHECK(chordForNote(48 + 7, c) && c.type == ChordType::Seventh && c.root == 7);
    CHECK(chordForNote(71, c) && c.type == ChordType::MinorSeventh && c.root == 11);
    CHECK(!chordForNote(72, c));
    CHECK(!chordForNote(23, c));
    CHECK(stringForNote(72) == 0);
    CHECK(stringForNote(84) == 12);
    CHECK(stringForNote(85) == -1);
    for (int root = 0; root < 12; ++root)
        for (int t = 0; t < kChordTypes; ++t)
        {
            const Chord chord { root, static_cast<ChordType>(t) };
            CHECK(chordForNote(noteForChord(chord), c) && c == chord);
        }
}

// ---- engine -----------------------------------------------------------------------------------

TEST(silent_until_played)
{
    auto e = makeEngine();
    CHECK(render(*e, 0.5) == 0.0f);
}

TEST(holding_a_chord_sounds_and_letting_go_fades_out)
{
    auto e = makeEngine();
    e->noteOn(noteForChord(cMajor), 1.0f);
    CHECK(render(*e, 0.3) > 0.02f);
    CHECK(e->heldChordCode() == Engine::codeOf(cMajor));

    e->noteOff(noteForChord(cMajor));
    CHECK(e->heldChordCode() == -1);
    render(*e, 3.0); // release plus reverb tail
    CHECK(render(*e, 0.2) < 1.0e-4f);
}

TEST(the_newest_held_chord_wins_and_releasing_it_returns_to_the_previous)
{
    auto e = makeEngine();
    e->noteOn(noteForChord(cMajor), 1.0f);
    e->noteOn(noteForChord(gMajor), 1.0f);
    CHECK(e->heldChordCode() == Engine::codeOf(gMajor));
    e->noteOff(noteForChord(gMajor));
    CHECK(e->heldChordCode() == Engine::codeOf(cMajor));
    CHECK(e->chordSounding());
}

TEST(strings_remember_the_last_chord)
{
    auto e = makeEngine();
    CHECK(e->stringChord() == cMajor); // before any chord
    e->noteOn(noteForChord(aMinor), 1.0f);
    e->noteOff(noteForChord(aMinor));
    CHECK(!e->chordSounding());
    CHECK(e->stringChord() == aMinor);
    CHECK(e->stringChordCode() == Engine::codeOf(aMinor));
}

TEST(a_plucked_string_rings_then_dies_away)
{
    auto e = makeEngine();
    e->params.reverb = 0.0f;
    e->params.stringDecay = 1.0f;
    e->noteOn(noteForString(6), 1.0f);
    CHECK(render(*e, 0.05) > 0.02f);
    CHECK(e->stringGlow(6) > 0.1f);
    render(*e, 1.2);
    CHECK(render(*e, 0.1) < 1.0e-3f);
    CHECK(e->stringGlow(6) < 0.01f);
}

TEST(longer_decay_rings_longer)
{
    auto measure = [](float decay)
    {
        auto e = makeEngine();
        e->params.reverb = 0.0f;
        e->params.stringDecay = decay;
        e->noteOn(noteForString(3), 1.0f);
        render(*e, 0.5);
        return render(*e, 0.2);
    };
    CHECK(measure(4.0f) > 3.0f * measure(0.6f));
}

TEST(sweeping_the_mod_wheel_plucks_every_string_once)
{
    auto e = makeEngine();
    e->controlChange(1, 0); // finger lands
    CHECK(e->pluckCount() == 0);
    for (int v = 0; v <= 127; v += 3)
        e->controlChange(1, v);
    e->controlChange(1, 127);
    CHECK(e->pluckCount() == kStrings);

    for (int v = 127; v >= 0; v -= 7) // and back down
        e->controlChange(1, v);
    e->controlChange(1, 0);
    CHECK(e->pluckCount() == 2 * kStrings);
}

TEST(a_jump_across_the_plate_plucks_what_it_crosses)
{
    auto e = makeEngine();
    e->strumAcross(0.0f, 0.45f, 1.0f);
    CHECK(e->pluckCount() == 6); // centres at 1/26, 3/26, ... 11/26 are below 0.45
}

TEST(all_notes_off_releases_held_chords)
{
    auto e = makeEngine();
    e->noteOn(noteForChord(cMajor), 1.0f);
    e->noteOn(noteForChord(gMajor), 1.0f);
    e->controlChange(123, 0);
    CHECK(!e->chordSounding());
}

TEST(everything_at_once_stays_finite_and_below_full_scale)
{
    auto e = makeEngine();
    e->params = { 1, 1, 1, 1, 8, 1, 1, 1, 1, 1 };
    e->noteOn(noteForChord({ 2, ChordType::MinorSeventh }), 1.0f);
    for (int s = 0; s < kStrings; ++s)
        e->noteOn(noteForString(s), 1.0f);

    std::vector<float> l(4800), r(4800);
    bool finite = true;
    float peak = 0.0f;
    for (int block = 0; block < 20; ++block)
    {
        e->process(l.data(), r.data(), 4800);
        for (size_t i = 0; i < l.size(); ++i)
        {
            finite = finite && std::isfinite(l[i]) && std::isfinite(r[i]);
            peak = std::max(peak, std::max(std::fabs(l[i]), std::fabs(r[i])));
        }
    }
    CHECK(finite);
    CHECK(peak < 1.0f);
}

TEST(decay_time_does_not_depend_on_the_sample_rate)
{
    auto measure = [](double sr)
    {
        auto e = makeEngine(sr);
        e->params.reverb = 0.0f;
        e->params.chorus = 0.0f;
        e->params.wobble = 0.0f;
        e->params.stringDecay = 2.0f;
        e->noteOn(noteForString(5), 1.0f);
        render(*e, 0.8, sr);
        return render(*e, 0.1, sr);
    };
    const float at44 = measure(44100.0), at96 = measure(96000.0);
    CHECK(std::fabs(at44 - at96) < 0.15f * at44);
}

TEST(wobble_off_keeps_pitch_exact)
{
    // With wobble at 0 a string is a steady tone: count zero crossings over one second.
    auto e = makeEngine();
    e->params = { 0, 0, 0, 1, 8, 0, 0, 0, 0, 1 };
    e->noteOn(noteForString(0), 1.0f); // C3, 130.81 Hz
    std::vector<float> l(48000), r(48000);
    e->process(l.data(), r.data(), 48000);
    int crossings = 0;
    for (size_t i = 1; i < l.size(); ++i)
        crossings += (l[i - 1] < 0.0f) != (l[i] < 0.0f);
    // FM adds extra crossings early on; the fundamental sets the floor.
    CHECK(crossings >= 2 * 130 - 2);
}

// ---- auto-strum ------------------------------------------------------------------------------

namespace
{
/** Holds C major with the auto-strum on and records which string each step plucks. */
std::vector<int> stepsPlayed(Params p, int steps)
{
    auto e = makeEngine();
    e->params = p;
    e->params.seqOn = true;
    e->noteOn(noteForChord(cMajor), 1.0f);
    std::vector<int> played;
    std::vector<float> l(1), r(1);
    int seen = e->pluckCount();
    for (int i = 0; i < 48000 * 30 && (int) played.size() < steps; ++i)
    {
        e->process(l.data(), r.data(), 1);
        if (e->pluckCount() != seen)
        {
            seen = e->pluckCount();
            played.push_back(e->lastPluckedString());
        }
    }
    return played;
}
} // namespace

TEST(auto_strum_is_off_by_default)
{
    auto e = makeEngine();
    e->noteOn(noteForChord(cMajor), 1.0f);
    render(*e, 1.0);
    CHECK(e->pluckCount() == 0);
}

TEST(auto_strum_steps_in_time)
{
    auto count = [](float bpm, int rate)
    {
        auto e = makeEngine();
        e->params.seqOn = true;
        e->params.seqBpm = bpm;
        e->params.seqRate = rate;
        e->noteOn(noteForChord(cMajor), 1.0f);
        render(*e, 0.99);
        return e->pluckCount();
    };
    CHECK(count(120.0f, 1) == 4); // eighths at 120: steps at 0, 0.25, 0.5, 0.75 s
    CHECK(count(120.0f, 0) == 2); // quarters
    CHECK(count(120.0f, 2) == 6); // eighth triplets
    CHECK(count(120.0f, 3) == 8); // sixteenths
    CHECK(count(60.0f, 1) == 2);
}

TEST(auto_strum_patterns_walk_the_strings)
{
    Params p;
    p.seqRange = 4;
    p.seqPattern = (int) SeqPattern::Up;
    CHECK((stepsPlayed(p, 6) == std::vector<int> { 0, 1, 2, 3, 0, 1 }));
    p.seqPattern = (int) SeqPattern::Down;
    CHECK((stepsPlayed(p, 6) == std::vector<int> { 3, 2, 1, 0, 3, 2 }));
    p.seqPattern = (int) SeqPattern::UpDown;
    CHECK((stepsPlayed(p, 8) == std::vector<int> { 0, 1, 2, 3, 2, 1, 0, 1 }));
}

TEST(random_pattern_stays_in_range_and_never_repeats_a_string)
{
    Params p;
    p.seqRange = 5;
    p.seqPattern = (int) SeqPattern::Random;
    p.seqBpm = 240.0f;
    p.seqRate = 3;
    const auto played = stepsPlayed(p, 60);
    CHECK(played.size() == 60);
    for (size_t i = 0; i < played.size(); ++i)
    {
        CHECK(played[i] >= 0 && played[i] < 5);
        if (i > 0)
            CHECK(played[i] != played[i - 1]);
    }
}

TEST(strum_pattern_sweeps_down_then_up)
{
    Params p;
    p.seqRange = 5;
    p.seqPattern = (int) SeqPattern::Strum;
    p.seqBpm = 60.0f;
    p.seqRate = 0; // one stroke per second
    CHECK((stepsPlayed(p, 10) == std::vector<int> { 0, 1, 2, 3, 4, 4, 3, 2, 1, 0 }));
}

TEST(auto_strum_stops_when_the_chord_is_released_and_restarts_on_the_beat)
{
    auto e = makeEngine();
    e->params.seqOn = true;
    e->params.seqBpm = 120.0f;
    e->noteOn(noteForChord(cMajor), 1.0f);
    render(*e, 0.6); // steps at 0, 0.25, 0.5
    CHECK(e->pluckCount() == 3);
    e->noteOff(noteForChord(cMajor));
    render(*e, 1.0);
    CHECK(e->pluckCount() == 3);

    e->noteOn(noteForChord(gMajor), 1.0f); // first step again, immediately, from the lowest string
    render(*e, 0.01);
    CHECK(e->pluckCount() == 4);
    CHECK(e->lastPluckedString() == 0);
}

// ---- rhythm machine ---------------------------------------------------------------------------

TEST(rhythm_patterns_are_well_formed)
{
    for (const auto& p : rhythmPatterns())
    {
        CHECK(p.stepsPerBeat == 3 || p.stepsPerBeat == 4);
        CHECK(p.length % p.stepsPerBeat == 0);
        bool anyKick = false;
        for (int d = 0; d < kDrums; ++d)
        {
            const std::string row = p.hits[d];
            CHECK((int) row.size() == p.length);
            CHECK(row.find_first_not_of("xX.") == std::string::npos);
            anyKick = anyKick || (d == Kick && row.find_first_of("xX") != std::string::npos);
        }
        CHECK(anyKick);
    }
}

TEST(rhythm_machine_is_off_by_default)
{
    auto e = makeEngine();
    CHECK(render(*e, 0.5) == 0.0f);
    CHECK(e->drumHits() == 0);
    CHECK(e->rhythmStep() == -1);
}

TEST(rhythm_machine_plays_in_time_on_its_own)
{
    auto steps = [](int pattern)
    {
        auto e = makeEngine();
        e->params.rhythmOn = true;
        e->params.rhythmPattern = pattern;
        e->params.seqBpm = 120.0f;
        std::vector<float> l(1), r(1);
        int last = -1, count = 0;
        for (int i = 0; i < (int) (0.99 * sampleRate); ++i)
        {
            e->process(l.data(), r.data(), 1);
            if (e->rhythmStep() != last)
            {
                last = e->rhythmStep();
                ++count;
            }
        }
        return count;
    };
    CHECK(steps(0) == 8); // Rock: sixteenths at 120 BPM, 8 in a second
    CHECK(steps(7) == 6); // Swing: triplets, 6 in a second

    auto e = makeEngine();
    e->params.rhythmOn = true;
    CHECK(render(*e, 0.5) > 0.01f); // drums sound with no chord held
    CHECK(e->drumHits() > 0);
}

TEST(stopping_the_rhythm_machine_lets_the_drums_ring_out)
{
    auto e = makeEngine();
    e->params.reverb = 0.0f;
    e->params.rhythmOn = true;
    render(*e, 1.0);
    e->params.rhythmOn = false;
    render(*e, 0.01);
    CHECK(e->rhythmStep() == -1);
    const int hits = e->drumHits();
    render(*e, 1.5);
    CHECK(e->drumHits() == hits);
    CHECK(render(*e, 0.1) < 1.0e-4f);
}

TEST(auto_strum_locks_to_the_drums)
{
    auto e = makeEngine();
    e->params.rhythmOn = true;
    e->params.seqBpm = 120.0f; // a beat every 0.5 s
    e->params.seqOn = true;
    e->params.seqRate = 1; // eighths: grid every 0.25 s
    render(*e, 0.1); // the drums are 0.2 beats in
    e->noteOn(noteForChord(cMajor), 1.0f);
    render(*e, 0.14); // up to 0.24 s: still before the next eighth
    CHECK(e->pluckCount() == 0);
    render(*e, 0.02); // 0.26 s: the strum lands on the grid
    CHECK(e->pluckCount() == 1);
    render(*e, 0.5);
    CHECK(e->pluckCount() == 3); // 0.5 s and 0.75 s
}

TEST(drums_and_everything_else_stay_below_full_scale)
{
    auto e = makeEngine();
    e->params = { 1, 1, 1, 1, 8, 1, 1, 1, 1, 1 };
    e->params.rhythmOn = true;
    e->params.rhythmLevel = 1.0f;
    e->params.seqOn = true;
    e->params.seqPattern = (int) SeqPattern::Strum;
    e->noteOn(noteForChord({ 2, ChordType::MinorSeventh }), 1.0f);
    for (int pattern = 0; pattern < kRhythmPatterns; ++pattern)
    {
        e->params.rhythmPattern = pattern;
        std::vector<float> l(24000), r(24000);
        e->process(l.data(), r.data(), 24000);
        for (size_t i = 0; i < l.size(); ++i)
        {
            CHECK(std::isfinite(l[i]) && std::isfinite(r[i]));
            CHECK(std::fabs(l[i]) < 1.0f && std::fabs(r[i]) < 1.0f);
        }
    }
}

// ---- LFO ---------------------------------------------------------------------------------------

namespace
{
/** A held C major with nothing else moving the sound (no bass, wobble, chorus or reverb), and the LFO set up. */
std::unique_ptr<Engine> lfoEngine(LfoTarget target, LfoShape shape, float rate, float depth)
{
    auto e = makeEngine();
    e->params.bassLevel = 0.0f;
    e->params.wobble = 0.0f;
    e->params.chorus = 0.0f;
    e->params.reverb = 0.0f;
    e->params.lfoTarget = static_cast<int>(target);
    e->params.lfoShape = static_cast<int>(shape);
    e->params.lfoRate = rate;
    e->params.lfoDepth = depth;
    e->noteOn(noteForChord(cMajor), 1.0f);
    return e;
}

/** Renders some seconds into separate left and right buffers. */
void renderInto(Engine& e, double seconds, std::vector<float>& l, std::vector<float>& r)
{
    const int total = (int) (seconds * sampleRate);
    l.assign((size_t) total, 0.0f);
    r.assign((size_t) total, 0.0f);
    for (int done = 0; done < total; done += 256)
        e.process(l.data() + done, r.data() + done, std::min(256, total - done));
}

float rms(const std::vector<float>& x, size_t from, size_t to)
{
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
        sum += x[i] * x[i];
    return (float) std::sqrt(sum / (double) (to - from));
}

/** The loudest and quietest 10 ms windows of a buffer. */
std::pair<float, float> loudestAndQuietest(const std::vector<float>& x)
{
    const size_t w = (size_t) (0.01 * sampleRate);
    float hi = 0.0f, lo = 1.0e9f;
    for (size_t i = 0; i + w <= x.size(); i += w)
    {
        const float v = rms(x, i, i + w);
        hi = std::max(hi, v);
        lo = std::min(lo, v);
    }
    return { hi, lo };
}

int zeroCrossings(const std::vector<float>& x, size_t from, size_t to)
{
    int n = 0;
    for (size_t i = from + 1; i < to; ++i)
        n += (x[i - 1] < 0.0f) != (x[i] < 0.0f);
    return n;
}
} // namespace

TEST(lfo_at_zero_depth_changes_nothing)
{
    auto plain = makeEngine();
    auto lfo = makeEngine();
    lfo->params.lfoTarget = static_cast<int>(LfoTarget::Pan);
    lfo->params.lfoShape = static_cast<int>(LfoShape::Random);
    lfo->params.lfoRate = 7.0f;
    for (auto* e : { plain.get(), lfo.get() })
        e->noteOn(noteForChord(aMinor), 1.0f);
    std::vector<float> l1, r1, l2, r2;
    renderInto(*plain, 0.5, l1, r1);
    renderInto(*lfo, 0.5, l2, r2);
    CHECK(l1 == l2 && r1 == r2);
}

TEST(lfo_tremolo_pulses_the_volume)
{
    // Same engine with and without the LFO: window by window, the level ratio is the tremolo's gain.
    std::vector<float> l, r, sl, sr;
    auto still = lfoEngine(LfoTarget::Tremolo, LfoShape::Sine, 4.0f, 0.0f);
    auto pulsing = lfoEngine(LfoTarget::Tremolo, LfoShape::Sine, 4.0f, 1.0f);
    renderInto(*still, 1.0, sl, sr);
    renderInto(*pulsing, 1.0, l, r);
    const size_t w = (size_t) (0.01 * sampleRate);
    float hi = 0.0f, lo = 1.0f;
    for (size_t i = (size_t) (0.1 * sampleRate); i + w <= l.size(); i += w)
    {
        const float gain = rms(l, i, i + w) / rms(sl, i, i + w);
        hi = std::max(hi, gain);
        lo = std::min(lo, gain);
    }
    CHECK(hi > 0.95f && hi < 1.02f); // never louder than without it
    CHECK(lo < 0.05f); // full depth dips to near silence
}

TEST(lfo_vibrato_bends_the_pitch_both_ways)
{
    // One plucked string under a 1 Hz square wave: 0.6 semitone up for half a second, then 0.6 down,
    // so the zero-crossing rate of the first half is 2^(1.2/12) = 1.072 times the second's.
    auto e = lfoEngine(LfoTarget::Vibrato, LfoShape::Square, 1.0f, 1.0f);
    e->params.chordLevel = 0.0f;
    e->params.stringDecay = 8.0f;
    e->noteOn(kFirstStringNote + 4, 1.0f);
    std::vector<float> l, r;
    renderInto(*e, 1.0, l, r);
    const size_t q = (size_t) (0.05 * sampleRate), half = (size_t) (0.5 * sampleRate);
    const float ratio = (float) zeroCrossings(l, q, half - q) / (float) zeroCrossings(l, half + q, 2 * half - q);
    CHECK(ratio > 1.05f && ratio < 1.095f);
}

TEST(lfo_filter_sweeps_the_brightness)
{
    // High-frequency content measured as the RMS of the sample-to-sample difference, relative to the level.
    auto brightness = [](const std::vector<float>& x, size_t from, size_t to)
    {
        double diff = 0.0;
        for (size_t i = from + 1; i < to; ++i)
            diff += (x[i] - x[i - 1]) * (x[i] - x[i - 1]);
        return (float) std::sqrt(diff / (double) (to - from)) / rms(x, from, to);
    };
    auto e = lfoEngine(LfoTarget::Filter, LfoShape::Square, 1.0f, 1.0f);
    e->params.chordTone = 0.5f; // swept from 1 (first half) to 0 (second half)
    std::vector<float> l, r;
    renderInto(*e, 1.0, l, r);
    const size_t q = (size_t) (0.05 * sampleRate), half = (size_t) (0.5 * sampleRate);
    CHECK(brightness(l, q, half - q) > 1.5f * brightness(l, half + q, 2 * half - q));
}

TEST(lfo_pan_moves_the_sound_from_side_to_side)
{
    auto e = lfoEngine(LfoTarget::Pan, LfoShape::Square, 1.0f, 1.0f);
    std::vector<float> l, r;
    renderInto(*e, 1.0, l, r);
    const size_t q = (size_t) (0.05 * sampleRate), half = (size_t) (0.5 * sampleRate);
    CHECK(rms(r, q, half - q) > 10.0f * rms(l, q, half - q)); // first half: right
    CHECK(rms(l, half + q, 2 * half - q) > 10.0f * rms(r, half + q, 2 * half - q)); // then left
}

int main()
{
    for (const auto& t : registry())
    {
        const int before = failures;
        t.run();
        std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", t.name);
    }
    std::printf("\n%zu tests, %d failed checks\n", registry().size(), failures);
    return failures == 0 ? 0 : 1;
}
