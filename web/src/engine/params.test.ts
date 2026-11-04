import { readFileSync } from 'node:fs'
import { describe, expect, it } from 'vitest'
import { clampParam, defaultValues, PARAM_COUNT, PARAMS, ParamId } from './params'

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
  })
})
