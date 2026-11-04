// Every control of the instrument, in the order of the ParamId enum in dsp/src/wasm_api.cpp
// (a test checks they match). Defaults are the C++ engine's defaults.

export enum ParamId {
  chordLevel,
  chordTone,
  bassLevel,
  stringLevel,
  stringDecay,
  stringTone,
  chorus,
  reverb,
  wobble,
  master,
  seqOn,
  seqBpm,
  seqPattern,
  seqRate,
  seqRange,
  rhythmOn,
  rhythmPattern,
  rhythmLevel,
}

export const PARAM_COUNT = Object.keys(ParamId).length / 2

export type Lang = 'es' | 'en'

export interface ParamSpec {
  min: number
  max: number
  def: number
  /** Snap to this step (choices and integers use 1). */
  step?: number
  format: (value: number, lang: Lang) => string
}

export const PATTERNS: Record<Lang, string[]> = {
  es: ['Arriba', 'Abajo', 'Ida y vuelta', 'Rasgueo', 'Al azar'],
  en: ['Up', 'Down', 'Up & down', 'Strum', 'Random'],
}
export const RATES = ['1/4', '1/8', '1/8 T', '1/16']

/** The rhythm machine's presets, in engine order (dsp/include/sdchord/rhythm.h; a test checks the shape). */
export const RHYTHMS: { name: Record<Lang, string>; length: number; stepsPerBeat: number }[] = [
  { name: { es: 'Rock', en: 'Rock' }, length: 16, stepsPerBeat: 4 },
  { name: { es: 'Disco', en: 'Disco' }, length: 16, stepsPerBeat: 4 },
  { name: { es: 'Balada', en: 'Ballad' }, length: 16, stepsPerBeat: 4 },
  { name: { es: 'Bossa Nova', en: 'Bossa Nova' }, length: 16, stepsPerBeat: 4 },
  { name: { es: 'Cumbia', en: 'Cumbia' }, length: 16, stepsPerBeat: 4 },
  { name: { es: 'Rumba', en: 'Rumba' }, length: 16, stepsPerBeat: 4 },
  { name: { es: 'Vals', en: 'Waltz' }, length: 12, stepsPerBeat: 4 },
  { name: { es: 'Swing', en: 'Swing' }, length: 12, stepsPerBeat: 3 },
]

const percent = (v: number) => `${Math.round(v * 100)} %`
const level = (def: number): ParamSpec => ({ min: 0, max: 1, def, format: percent })

export const PARAMS: Record<ParamId, ParamSpec> = {
  [ParamId.chordLevel]: level(0.55),
  [ParamId.chordTone]: level(0.35),
  [ParamId.bassLevel]: level(0.5),
  [ParamId.stringLevel]: level(0.8),
  [ParamId.stringDecay]: { min: 0.2, max: 8, def: 1.8, format: (v) => `${v.toFixed(1)} s` },
  [ParamId.stringTone]: level(0.5),
  [ParamId.chorus]: level(0.35),
  [ParamId.reverb]: level(0.3),
  [ParamId.wobble]: level(0.15),
  [ParamId.master]: level(0.8),
  [ParamId.seqOn]: { min: 0, max: 1, def: 0, step: 1, format: (v) => (v ? 'ON' : 'OFF') },
  [ParamId.seqBpm]: { min: 40, max: 240, def: 100, step: 1, format: (v) => `${Math.round(v)} BPM` },
  [ParamId.seqPattern]: { min: 0, max: 4, def: 0, step: 1, format: (v, lang) => PATTERNS[lang][Math.round(v)] },
  [ParamId.seqRate]: { min: 0, max: 3, def: 1, step: 1, format: (v) => RATES[Math.round(v)] },
  [ParamId.seqRange]: { min: 2, max: 13, def: 8, step: 1, format: (v) => String(Math.round(v)) },
  [ParamId.rhythmOn]: { min: 0, max: 1, def: 0, step: 1, format: (v) => (v ? 'ON' : 'OFF') },
  [ParamId.rhythmPattern]: { min: 0, max: RHYTHMS.length - 1, def: 0, step: 1, format: (v, lang) => RHYTHMS[Math.round(v)].name[lang] },
  [ParamId.rhythmLevel]: level(0.7),
}

export const defaultValues = (): number[] => Array.from({ length: PARAM_COUNT }, (_, id) => PARAMS[id as ParamId].def)

/** Clamps a value into the parameter's range and snaps it to its step. */
export function clampParam(id: ParamId, value: number): number {
  const spec = PARAMS[id]
  const clamped = Math.min(spec.max, Math.max(spec.min, value))
  return spec.step ? Math.round(clamped / spec.step) * spec.step : clamped
}
