// Computer keyboard: each row of keys is one chord type, in the buttons' order (Db Ab Eb ... F#). Keys are matched by
// physical position (KeyboardEvent.code), so it works the same on any keyboard layout.

import { COLUMN_ROOTS } from './chords'

const ROWS: string[][] = [
  ['Digit1', 'Digit2', 'Digit3', 'Digit4', 'Digit5', 'Digit6', 'Digit7', 'Digit8', 'Digit9', 'Digit0', 'Minus', 'Equal'],
  ['KeyQ', 'KeyW', 'KeyE', 'KeyR', 'KeyT', 'KeyY', 'KeyU', 'KeyI', 'KeyO', 'KeyP', 'BracketLeft', 'BracketRight'],
  ['KeyA', 'KeyS', 'KeyD', 'KeyF', 'KeyG', 'KeyH', 'KeyJ', 'KeyK', 'KeyL', 'Semicolon', 'Quote', 'Backslash'],
  // The bottom row only has ten keys, so it stops at E minor 7 (no Bm7 or F#m7 there).
  ['KeyZ', 'KeyX', 'KeyC', 'KeyV', 'KeyB', 'KeyN', 'KeyM', 'Comma', 'Period', 'Slash'],
]

const CODES = new Map<string, number>()
ROWS.forEach((row, type) => row.forEach((code, column) => CODES.set(code, COLUMN_ROOTS[column] + 12 * type)))

/** The chord (root + 12 * type) a physical key plays, or undefined. */
export const chordForKey = (code: string): number | undefined => CODES.get(code)
