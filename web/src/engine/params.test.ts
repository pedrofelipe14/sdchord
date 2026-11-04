import { readFileSync } from 'node:fs'
import { describe, expect, it } from 'vitest'
import { clampParam, defaultValues, fromPosition, PARAM_COUNT, PARAMS, ParamId, toPosition } from './params'

const wasmApi = readFileSync(new URL('../../../dsp/src/wasm_api.cpp', import.meta.url), 'utf8')
const engineHeader = readFileSync(new URL('../../../dsp/include/sdchord/engine.h', import.meta.url), 'utf8')

describe('parameters', () => {
  it('use the same ids, in the same order, as the C++ side', () => {
    const body = wasmApi.match(/enum ParamId\s*\{([^}]*)\}/)![1]
    const cpp = body.split(',').map((s) => s.trim()).filter(Boolean)
    const ts = Array.from({ length: PARAM_COUNT }, (_, i) => ParamId[i])
    expect(ts).toEqual(cpp)
  })

  it('start at the C++ engine defaults', () => {
    for (let id = 0; id < PARAM_COUNT; id++) {
      const name = ParamId[id]
      const match = engineHeader.match(new RegExp(`\\b${name} = ([^;]+);`))
      expect(match, name).not.toBeNull()
      const cppDefault = match![1].replace(/f$/, '')
      const expected = cppDefault === 'true' ? 1 : cppDefault === 'false' ? 0 : Number(cppDefault)
      expect(defaultValues()[id], name).toBeCloseTo(expected)
    }
  })

  it('clamp and snap values', () => {
    expect(clampParam(ParamId.master, 3)).toBe(1)
    expect(clampParam(ParamId.seqPattern, 2.4)).toBe(2)
    expect(clampParam(ParamId.seqRange, 40)).toBe(13)
    expect(PARAMS[ParamId.seqRate].format(2, 'es')).toBe('1/8 T')
    expect(PARAMS[ParamId.seqPattern].format(4, 'en')).toBe('Random')
    expect(PARAMS[ParamId.lfoTarget].format(1, 'es')).toBe('Trémolo')
    expect(PARAMS[ParamId.lfoRate].format(0.25, 'en')).toBe('0.25 Hz')
  })

  it('give tapered knobs more travel at the low end, and round-trip positions', () => {
    expect(toPosition(ParamId.master, 0.5)).toBeCloseTo(0.5) // linear
    expect(fromPosition(ParamId.lfoRate, 0.5)).toBeLessThan(3) // half way is a slow wobble, not 6 Hz
    for (const v of [0.1, 0.7, 4, 12]) expect(fromPosition(ParamId.lfoRate, toPosition(ParamId.lfoRate, v))).toBeCloseTo(v)
  })
})
