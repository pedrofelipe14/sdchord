#pragma once

// Chords, their voicings and the MIDI layout of the instrument. No audio here, so it is easy to test.

#include <array>

namespace pedrochord
{
enum class ChordType
{
    Major,
    Minor,
    Seventh,
    MinorSeventh,
};

constexpr int kChordTypes = 4;
/** Strings on the strum plate. */
constexpr int kStrings = 13;

struct Chord
{
    int root = 0; // pitch class, 0 = C ... 11 = B
    ChordType type = ChordType::Major;

    bool operator==(const Chord& o) const { return root == o.root && type == o.type; }
    bool operator!=(const Chord& o) const { return !(*this == o); }
};

/** Semitones above the root. Triads leave the fourth slot unused. */
struct Intervals
{
    std::array<int, 4> steps {};
    int count = 0;
};

inline Intervals intervalsOf(ChordType type)
{
    switch (type)
    {
        case ChordType::Major: return { { 0, 4, 7, 0 }, 3 };
        case ChordType::Minor: return { { 0, 3, 7, 0 }, 3 };
        case ChordType::Seventh: return { { 0, 4, 7, 10 }, 4 };
        case ChordType::MinorSeventh: return { { 0, 3, 7, 10 }, 4 };
    }
    return { { 0, 4, 7, 0 }, 3 };
}

inline bool isChordTone(const Chord& chord, int midiNote)
{
    const auto iv = intervalsOf(chord.type);
    const int pc = ((midiNote - chord.root) % 12 + 12) % 12;
    for (int i = 0; i < iv.count; ++i)
        if (iv.steps[(size_t) i] == pc)
            return true;
    return false;
}

/** Close voicing for the sustained chord: the root sits between F3 and E4, so changes move little. */
inline std::array<int, 4> padNotes(const Chord& chord, int& count)
{
    const auto iv = intervalsOf(chord.type);
    const int base = 53 + ((chord.root - 5) % 12 + 12) % 12;
    std::array<int, 4> notes {};
    for (int i = 0; i < iv.count; ++i)
        notes[(size_t) i] = base + iv.steps[(size_t) i];
    count = iv.count;
    return notes;
}

/** The bass plays the root between F1 and E2. */
inline int bassNote(const Chord& chord) { return 29 + ((chord.root - 5) % 12 + 12) % 12; }

/** Each string is the next chord tone upwards, starting from the first one at or above C3. */
inline std::array<int, kStrings> stringNotes(const Chord& chord)
{
    std::array<int, kStrings> notes {};
    int note = 48;
    for (int s = 0; s < kStrings; ++note)
        if (isChordTone(chord, note))
            notes[(size_t) s++] = note;
    return notes;
}

// ---------------------------------------------------------------------------------------------
// MIDI layout, so a DAW can play everything:
//   C1-B1 (24-35)  major chord on that root     C2-B2 (36-47)  minor
//   C3-B3 (48-59)  seventh                      C4-B4 (60-71)  minor seventh
//   C5-C6 (72-84)  strings 1-13 of the strum plate
//   CC1 (mod wheel) sweeps across the strings, like a finger on the plate.

constexpr int kFirstChordNote = 24;
constexpr int kFirstStringNote = kFirstChordNote + 12 * kChordTypes; // 72

/** The chord a MIDI note selects, if it is in the chord zone. */
inline bool chordForNote(int note, Chord& out)
{
    if (note < kFirstChordNote || note >= kFirstStringNote)
        return false;
    out.root = (note - kFirstChordNote) % 12;
    out.type = static_cast<ChordType>((note - kFirstChordNote) / 12);
    return true;
}

inline int noteForChord(const Chord& chord) { return kFirstChordNote + 12 * static_cast<int>(chord.type) + chord.root; }

/** The string a MIDI note plucks, or -1. */
inline int stringForNote(int note)
{
    const int s = note - kFirstStringNote;
    return s >= 0 && s < kStrings ? s : -1;
}

inline int noteForString(int string) { return kFirstStringNote + string; }
} // namespace pedrochord
