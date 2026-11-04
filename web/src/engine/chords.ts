// The chord layout and voicings, mirroring dsp/include/sdchord/chords.h (tests check the same cases).

export const CHORD_TYPES = 4
export const STRINGS = 13
/** MIDI notes: 24 + code selects a chord (code = root + 12 * type); 72 + s plucks string s. */
export const FIRST_CHORD_NOTE = 24
export const FIRST_STRING_NOTE = FIRST_CHORD_NOTE + 12 * CHORD_TYPES

export const ROOT_NAMES = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
/** The buttons' left-to-right order, as on the original instrument: around the circle of fifths
    (Db Ab Eb Bb F C G D A E B F#). Mirrors kColumnRoots in chords.h; MIDI stays chromatic. */
export const COLUMN_ROOTS = [1, 8, 3, 10, 5, 0, 7, 2, 9, 4, 11, 6]
const SUFFIXES = ['', 'm', '7', 'm7']
const INTERVALS = [
  [0, 4, 7],
  [0, 3, 7],
  [0, 4, 7, 10],
  [0, 3, 7, 10],
]

export const chordName = (code: number): string => (code < 0 ? '-' : ROOT_NAMES[code % 12] + SUFFIXES[Math.floor(code / 12)])

export const noteName = (midi: number): string => ROOT_NAMES[midi % 12] + String(Math.floor(midi / 12) - 1)

/** Each string is the next chord tone upwards, starting from the first one at or above C3. */
export function stringNotes(code: number): number[] {
  const root = Math.max(0, code) % 12
  const steps = INTERVALS[Math.floor(Math.max(0, code) / 12)]
  const notes: number[] = []
  for (let note = 48; notes.length < STRINGS; note++) {
    if (steps.includes((((note - root) % 12) + 12) % 12)) notes.push(note)
  }
  return notes
}
