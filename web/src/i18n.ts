import type { Lang } from './engine/params'

/** Every text on the page, in both languages (a test checks they stay in step). */
export const STRINGS = {
  es: {
    subtitle: 'ARPA ELECTRÓNICA DE ACORDES',
    chord: 'ACORDE',
    power: 'ON',
    rows: ['MAYOR', 'MENOR', '7', 'm7'],
    plate: 'RASGUEO',
    plateHint: '▲ agudo · grave ▼',
    plateHintWide: '◀ grave · agudo ▶',
    sections: { chords: 'ACORDES', bass: 'BAJO', strings: 'CUERDAS', effects: 'EFECTOS', output: 'SALIDA', auto: 'RASGUEO AUTOMÁTICO', rhythm: 'RITMO', lfo: 'LFO', loop: 'LOOP DE ACORDES' },
    knobs: {
      level: 'NIVEL',
      tone: 'TONO',
      sustain: 'SUSTAIN',
      bright: 'BRILLO',
      chorus: 'CHORUS',
      reverb: 'REVERB',
      wobble: 'WOBBLE',
      volume: 'VOLUMEN',
      tempo: 'TEMPO',
      pattern: 'PATRÓN',
      rate: 'FIGURA',
      range: 'CUERDAS',
      rhythm: 'RITMO',
      target: 'DESTINO',
      shape: 'FORMA',
      speed: 'VELOCIDAD',
      depth: 'CANTIDAD',
    },
    autoKey: 'AUTO',
    startKey: 'START',
    holdKey: 'FIJAR',
    // The loop button says what pressing it does next; the window says what the loop is doing.
    loopKeys: ['GRABAR', 'CANCELAR', 'LISTO', 'PARAR', 'PLAY'],
    loopStatus: ['VACÍO', 'TOCÁ UN ACORDE', '● GRABANDO', 'SONANDO', 'PARADO'],
    clearKey: 'BORRAR',
    loopBeats: (n: number) => (n === 1 ? '1 tiempo' : `${n} tiempos`),
    loopBars: (n: number) => (n === 1 ? '1 compás' : `${n} compases`),
    loopHint: 'Grabá una vuelta de acordes y seguí tocando encima. Con el ritmo prendido dura compases enteros.',
    rhythmHint: 'Comparte el tempo con el rasgueo automático, que se engancha a la batería.',
    keyboardHelp: 'TECLADO: 1–0 mayor · Q–P menor · A–Ñ 7 · Z–M m7 · espacio: rasgueo auto · enter: ritmo',
    midiHelp: 'MIDI: C1 mayor · C2 menor · C3 7 · C4 m7 · C5–C6 cuerdas · rueda de modulación = rasgueo',
    midiConnected: (n: number) => (n === 1 ? '1 controlador MIDI conectado' : `${n} controladores MIDI conectados`),
    tapToStart: 'Tocá un acorde para empezar',
    about: 'Motor de audio en C++ compilado a WebAssembly · el mismo que el plugin VST3',
    source: 'Código en GitHub',
  },
  en: {
    subtitle: 'ELECTRONIC CHORD HARP',
    chord: 'CHORD',
    power: 'ON',
    rows: ['MAJOR', 'MINOR', '7', 'm7'],
    plate: 'STRUM',
    plateHint: '▲ high · low ▼',
    plateHintWide: '◀ low · high ▶',
    sections: { chords: 'CHORDS', bass: 'BASS', strings: 'STRINGS', effects: 'EFFECTS', output: 'OUTPUT', auto: 'AUTO-STRUM', rhythm: 'RHYTHM', lfo: 'LFO', loop: 'CHORD LOOP' },
    knobs: {
      level: 'LEVEL',
      tone: 'TONE',
      sustain: 'SUSTAIN',
      bright: 'BRIGHT',
      chorus: 'CHORUS',
      reverb: 'REVERB',
      wobble: 'WOBBLE',
      volume: 'VOLUME',
      tempo: 'TEMPO',
      pattern: 'PATTERN',
      rate: 'RATE',
      range: 'STRINGS',
      rhythm: 'RHYTHM',
      target: 'TARGET',
      shape: 'SHAPE',
      speed: 'SPEED',
      depth: 'DEPTH',
    },
    autoKey: 'AUTO',
    startKey: 'START',
    holdKey: 'HOLD',
    loopKeys: ['REC', 'CANCEL', 'DONE', 'STOP', 'PLAY'],
    loopStatus: ['EMPTY', 'PLAY A CHORD', '● RECORDING', 'LOOPING', 'STOPPED'],
    clearKey: 'CLEAR',
    loopBeats: (n: number) => (n === 1 ? '1 beat' : `${n} beats`),
    loopBars: (n: number) => (n === 1 ? '1 bar' : `${n} bars`),
    loopHint: 'Record a round of chords and keep playing over it. With the rhythm on it lasts whole bars.',
    rhythmHint: 'Shares its tempo with the auto-strum, which locks to the drums.',
    keyboardHelp: 'KEYBOARD: 1–0 major · Q–P minor · A–L 7 · Z–M m7 · space: auto-strum · enter: rhythm',
    midiHelp: 'MIDI: C1 major · C2 minor · C3 7 · C4 m7 · C5–C6 strings · mod wheel = strum',
    midiConnected: (n: number) => (n === 1 ? '1 MIDI controller connected' : `${n} MIDI controllers connected`),
    tapToStart: 'Press a chord to start',
    about: 'C++ audio engine compiled to WebAssembly · the same one as the VST3 plugin',
    source: 'Source on GitHub',
  },
} satisfies Record<Lang, unknown>

export type Strings = (typeof STRINGS)['es']

export function initialLang(): Lang {
  try {
    const saved = localStorage.getItem('sdchord-lang')
    if (saved === 'es' || saved === 'en') return saved
  } catch {
    // storage blocked: use the browser language
  }
  return navigator.language.toLowerCase().startsWith('es') ? 'es' : 'en'
}

export function saveLang(lang: Lang): void {
  try {
    localStorage.setItem('sdchord-lang', lang)
  } catch {
    // fine if it can't be remembered
  }
}
