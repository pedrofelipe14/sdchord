import { describe, expect, it } from 'vitest'
import { chordName, noteName, STRINGS, stringNotes } from './chords'
import { chordForKey } from './keyboard'

describe('chords', () => {
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
  it('maps each row of keys to a chord type, from C', () => {
    expect(chordForKey('Digit1')).toBe(0) // C
    expect(chordForKey('Equal')).toBe(11) // B
    expect(chordForKey('KeyQ')).toBe(12) // Cm
    expect(chordForKey('KeyP')).toBe(12 + 9) // Am
    expect(chordForKey('Semicolon')).toBe(24 + 9) // A7 (Ñ on a Spanish layout)
    expect(chordForKey('Backslash')).toBe(24 + 11)
    expect(chordForKey('KeyZ')).toBe(36)
    expect(chordForKey('Slash')).toBe(36 + 9) // the bottom row ends on Am7
    expect(chordForKey('Space')).toBeUndefined()
    expect(chordForKey('Enter')).toBeUndefined()
  })
})
