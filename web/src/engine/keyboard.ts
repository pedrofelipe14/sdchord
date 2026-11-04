// Computer keyboard: each row of keys is one chord type, left to right from C. Keys are matched by
// physical position (KeyboardEvent.code), so it works the same on any keyboard layout.

const ROWS: string[][] = [
  ['Digit1', 'Digit2', 'Digit3', 'Digit4', 'Digit5', 'Digit6', 'Digit7', 'Digit8', 'Digit9', 'Digit0', 'Minus', 'Equal'],
  ['KeyQ', 'KeyW', 'KeyE', 'KeyR', 'KeyT', 'KeyY', 'KeyU', 'KeyI', 'KeyO', 'KeyP', 'BracketLeft', 'BracketRight'],
  ['KeyA', 'KeyS', 'KeyD', 'KeyF', 'KeyG', 'KeyH', 'KeyJ', 'KeyK', 'KeyL', 'Semicolon', 'Quote', 'Backslash'],
  // The bottom row only has ten keys, so it stops at A minor 7.
  ['KeyZ', 'KeyX', 'KeyC', 'KeyV', 'KeyB', 'KeyN', 'KeyM', 'Comma', 'Period', 'Slash'],
]

const CODES = new Map<string, number>()
ROWS.forEach((row, type) => row.forEach((code, root) => CODES.set(code, root + 12 * type)))

/** The chord (root + 12 * type) a physical key plays, or undefined. */
export const chordForKey = (code: string): number | undefined => CODES.get(code)
