import { existsSync, readFileSync } from 'node:fs'
import { describe, expect, it } from 'vitest'
import { FIRST_CHORD_NOTE, FIRST_STRING_NOTE } from './chords'
import { ParamId, RHYTHMS } from './params'

// Runs the real compiled engine (public/pedrochord.wasm, built by scripts/build-wasm.sh) the same way
// the AudioWorklet does.
const wasmUrl = new URL('../../public/pedrochord.wasm', import.meta.url)

interface Engine {
  memory: WebAssembly.Memory
  _initialize?: () => void
  pc_init(sampleRate: number): void
  pc_note_on(note: number, velocity: number): void
  pc_note_off(note: number): void
  pc_control_change(cc: number, value: number): void
  pc_set_param(id: number, value: number): void
  pc_process(frames: number): void
  pc_left(): number
  pc_held_chord(): number
  pc_string_chord(): number
  pc_string_glow(string: number): number
  pc_pluck_count(): number
  pc_rhythm_step(): number
  pc_drum_hits(): number
  pc_rhythm_count(): number
  pc_rhythm_length(pattern: number): number
  pc_rhythm_steps_per_beat(pattern: number): number
}

function load(): Engine {
  const module = new WebAssembly.Module(readFileSync(wasmUrl))
  const imports: Record<string, Record<string, () => number>> = {}
  for (const imp of WebAssembly.Module.imports(module)) {
    imports[imp.module] ??= {}
    if (imp.kind === 'function') imports[imp.module][imp.name] = () => 0
  }
  const engine = new WebAssembly.Instance(module, imports).exports as unknown as Engine
  engine._initialize?.()
  engine.pc_init(48000)
  return engine
}

function renderRms(engine: Engine, blocks: number): number {
  const memory = new Float32Array(engine.memory.buffer)
  const left = engine.pc_left() >> 2
  let sum = 0
  for (let b = 0; b < blocks; b++) {
    engine.pc_process(128)
    for (let i = 0; i < 128; i++) sum += memory[left + i] ** 2
  }
  return Math.sqrt(sum / (blocks * 128))
}

const A_MINOR = 12 + 9

describe.skipIf(!existsSync(wasmUrl))('compiled engine (pedrochord.wasm)', () => {
  it('only needs imports the worklet can stub', () => {
    const module = new WebAssembly.Module(readFileSync(wasmUrl))
    for (const imp of WebAssembly.Module.imports(module)) expect(imp.module).toBe('wasi_snapshot_preview1')
  })

  it('is silent at rest and plays a held chord', () => {
    const engine = load()
    expect(renderRms(engine, 10)).toBe(0)
    engine.pc_note_on(FIRST_CHORD_NOTE + A_MINOR, 1)
    expect(renderRms(engine, 40)).toBeGreaterThan(0.01)
    expect(engine.pc_held_chord()).toBe(A_MINOR)
    engine.pc_note_off(FIRST_CHORD_NOTE + A_MINOR)
    expect(engine.pc_held_chord()).toBe(-1)
    expect(engine.pc_string_chord()).toBe(A_MINOR) // the strings remember it
  })

  it('plucks strings and reports how loud they ring', () => {
    const engine = load()
    engine.pc_note_on(FIRST_STRING_NOTE + 4, 1)
    renderRms(engine, 4)
    expect(engine.pc_string_glow(4)).toBeGreaterThan(0.1)
    expect(engine.pc_string_glow(0)).toBe(0)
  })

  it('strums with the mod wheel', () => {
    const engine = load()
    for (let v = 0; v <= 127; v += 4) engine.pc_control_change(1, v)
    engine.pc_control_change(1, 127)
    expect(engine.pc_pluck_count()).toBe(13)
  })

  it('agrees with the page on the rhythm patterns', () => {
    const engine = load()
    expect(engine.pc_rhythm_count()).toBe(RHYTHMS.length)
    RHYTHMS.forEach((r, i) => {
      expect(engine.pc_rhythm_length(i)).toBe(r.length)
      expect(engine.pc_rhythm_steps_per_beat(i)).toBe(r.stepsPerBeat)
    })
  })

  it('plays the drums and reports the step', () => {
    const engine = load()
    expect(engine.pc_rhythm_step()).toBe(-1)
    engine.pc_set_param(ParamId.rhythmOn, 1)
    expect(renderRms(engine, 100)).toBeGreaterThan(0.01)
    expect(engine.pc_drum_hits()).toBeGreaterThan(0)
    expect(engine.pc_rhythm_step()).toBeGreaterThanOrEqual(0)
    engine.pc_set_param(ParamId.rhythmOn, 0)
    renderRms(engine, 1)
    expect(engine.pc_rhythm_step()).toBe(-1)
  })

  it('runs the auto-strum in time while a chord is held', () => {
    const engine = load()
    engine.pc_set_param(ParamId.seqOn, 1)
    engine.pc_set_param(ParamId.seqBpm, 120)
    engine.pc_set_param(ParamId.seqRate, 1) // eighths: 4 steps a second
    engine.pc_note_on(FIRST_CHORD_NOTE, 1)
    renderRms(engine, 370) // ~0.99 s
    expect(engine.pc_pluck_count()).toBe(4)
  })
})
