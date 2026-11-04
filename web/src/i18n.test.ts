import { describe, expect, it } from 'vitest'
import { STRINGS } from './i18n'

describe('translations', () => {
  it('have the same texts in Spanish and English', () => {
    const shape = (o: object): unknown =>
      Object.fromEntries(
        Object.entries(o).map(([k, v]) => [k, Array.isArray(v) ? v.length : typeof v === 'object' && v ? shape(v) : typeof v]),
      )
    expect(shape(STRINGS.en)).toEqual(shape(STRINGS.es))
  })
})
