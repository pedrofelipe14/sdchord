import { readFileSync } from 'node:fs'
import { describe, expect, it } from 'vitest'
import { chordName, COLUMN_ROOTS, noteName, STRINGS, stringNotes } from './chords'
import { chordForKey } from './keyboard'

describe('chords', () => {
  it('lays the buttons out around the circle of fifths, like the C++ side', () => {
    const header = readFileSync(new URL('../../../dsp/include/sdchord/chords.h', import.meta.url), 'utf8')
    const cpp = header.match(/kColumnRoots\[12\] = \{([^}]*)\}/)![1].split(',').map(Number)
    expect(COLUMN_ROOTS).toEqual(cpp)
    expect(COLUMN_ROOTS.map((r) => chordName(r)).join(' ')).toBe('Db Ab Eb Bb F C G D A E B F#')
  })

  it('names every chord', () => {
    expect(chordName(0)).toBe('C')
    expect(chordName(12 + 9)).toBe('Am')
    expect(chordName(24 + 7)).toBe('G7')
    expect(chordName(36 + 11)).toBe('Bm7')
    expect(chordName(-1)).toBe('-')
  })

  // The same cases as the C++ tests in dsp/tests/dsp_tests.cpp.
  it('lays the strings out like the engine does', () => {
    const c = stringNotes(0)
    expect(c.slice(0, 4)).toEqual([48, 52, 55, 60])
    expect(c).toHaveLength(STRINGS)
    expect(stringNotes(24 + 7)[0]).toBe(50) // G7 starts on D3
    expect(stringNotes(24 + 7).some((n) => n % 12 === 5)).toBe(true) // and has its F
    expect(noteName(48)).toBe('C3')
  })
})

describe('computer keyboard', () => {
  it('maps each row of keys to a chord type, in the buttons\' order', () => {
    expect(chordForKey('Digit1')).toBe(1) // Db
    expect(chordForKey('Digit6')).toBe(0) // C
    expect(chordForKey('Equal')).toBe(6) // F#
    expect(chordForKey('KeyQ')).toBe(12 + 1) // Dbm
    expect(chordForKey('KeyP')).toBe(12 + 4) // Em
    expect(chordForKey('Semicolon')).toBe(24 + 4) // E7 (Ñ on a Spanish layout)
    expect(chordForKey('Backslash')).toBe(24 + 6) // F#7
    expect(chordForKey('KeyZ')).toBe(36 + 1)
    expect(chordForKey('Slash')).toBe(36 + 4) // the bottom row ends on Em7
    expect(chordForKey('Space')).toBeUndefined()
    expect(chordForKey('Enter')).toBeUndefined()
  })
})
