import { useEffect, useMemo, useState } from 'react'
import { ChordGrid } from './components/ChordGrid'
import { Knob } from './components/Knob'
import { StrumPlate } from './components/StrumPlate'
import { chordName } from './engine/chords'
import { type EngineState, SDChordEngine, silentState } from './engine/engine'
import { chordForKey } from './engine/keyboard'
import { defaultValues, type Lang, PARAMS, ParamId, RHYTHMS } from './engine/params'
import { initialLang, saveLang, STRINGS } from './i18n'

const REPO = 'https://github.com/pedrofelipe14/sdchord'

type KnobKey = keyof (typeof STRINGS)['es']['knobs']
type SectionKey = keyof (typeof STRINGS)['es']['sections']

// The front panel, section by section, as on the plugin.
const SECTIONS: { title: SectionKey; knobs: [ParamId, KnobKey][] }[] = [
  { title: 'chords', knobs: [[ParamId.chordLevel, 'level'], [ParamId.chordTone, 'tone']] },
  { title: 'bass', knobs: [[ParamId.bassLevel, 'level']] },
  { title: 'strings', knobs: [[ParamId.stringLevel, 'level'], [ParamId.stringDecay, 'sustain'], [ParamId.stringTone, 'bright']] },
  { title: 'effects', knobs: [[ParamId.chorus, 'chorus'], [ParamId.reverb, 'reverb'], [ParamId.wobble, 'wobble']] },
  { title: 'output', knobs: [[ParamId.master, 'volume']] },
]
const SEQ_KNOBS: [ParamId, KnobKey][] = [
  [ParamId.seqBpm, 'tempo'],
  [ParamId.seqPattern, 'pattern'],
  [ParamId.seqRate, 'rate'],
  [ParamId.seqRange, 'range'],
]
const RHYTHM_KNOBS: [ParamId, KnobKey][] = [
  [ParamId.rhythmPattern, 'rhythm'],
  [ParamId.seqBpm, 'tempo'], // one tempo for the drums and the auto-strum
  [ParamId.rhythmLevel, 'volume'],
]

/** The rhythm panel starts hidden; once opened it stays open on the next visit. */
const loadShowRhythm = (): boolean => {
  try {
    return localStorage.getItem('sdchord-rhythm-panel') === '1'
  } catch {
    return false
  }
}

/** Hold mode defaults to on for touch screens, where holding a key and strumming at once is awkward. */
const loadLatch = (): boolean => {
  try {
    const saved = localStorage.getItem('sdchord-hold')
    if (saved !== null) return saved === '1'
  } catch {
    // storage blocked: fall back to the default
  }
  return matchMedia('(pointer: coarse)').matches
}

export default function App() {
  const engine = useMemo(() => new SDChordEngine(), [])
  const [lang, setLang] = useState<Lang>(initialLang)
  const [values, setValues] = useState(defaultValues)
  const [state, setState] = useState<EngineState>(silentState)
  const [running, setRunning] = useState(false)
  const [midiInputs, setMidiInputs] = useState(0)
  const [showRhythm, setShowRhythm] = useState(loadShowRhythm)
  const [latch, setLatch] = useState(loadLatch)
  const t = STRINGS[lang]

  useEffect(() => {
    document.documentElement.lang = lang
    saveLang(lang)
  }, [lang])

  useEffect(() => engine.onState(setState), [engine])
  useEffect(() => engine.preload(), [engine])

  // Every gesture (lifting a finger, a click, a key) tries to switch the audio on: Safari on iPhone
  // ignores touching down, so the press that starts a chord can't do it alone.
  useEffect(() => {
    const unlock = () => engine.unlock()
    const events = ['pointerup', 'touchend', 'click', 'keydown'] as const
    for (const name of events) window.addEventListener(name, unlock, { capture: true, passive: true })
    return () => {
      for (const name of events) window.removeEventListener(name, unlock, { capture: true })
    }
  }, [engine])

  useEffect(() => {
    try {
      localStorage.setItem('sdchord-rhythm-panel', showRhythm ? '1' : '0')
    } catch {
      // not remembered, still works
    }
  }, [showRhythm])

  useEffect(() => {
    try {
      localStorage.setItem('sdchord-hold', latch ? '1' : '0')
    } catch {
      // not remembered, still works
    }
  }, [latch])

  // Space and Enter flip seqOn and rhythmOn in the UI state; this sends them on to the engine.
  const seqOnValue = values[ParamId.seqOn]
  const rhythmOnValue = values[ParamId.rhythmOn]
  useEffect(() => engine.setParam(ParamId.seqOn, seqOnValue), [engine, seqOnValue])
  useEffect(() => {
    engine.setParam(ParamId.rhythmOn, rhythmOnValue)
    if (rhythmOnValue) void engine.start().then(() => setRunning(true)) // the drums need audio running
  }, [engine, rhythmOnValue])

  const setParam = (id: ParamId, value: number) => {
    engine.setParam(id, value)
    setValues((v) => v.map((old, i) => (i === id ? value : old)))
  }
  const started = () => setRunning(true)

  // Computer keyboard: chords by physical key position; space toggles the auto-strum.
  useEffect(() => {
    const held = new Map<string, number>()
    const down = (e: KeyboardEvent) => {
      if (e.ctrlKey || e.metaKey || e.altKey) return
      const toggle = e.code === 'Space' ? ParamId.seqOn : e.code === 'Enter' ? ParamId.rhythmOn : undefined
      if (toggle !== undefined) {
        e.preventDefault()
        if (!e.repeat) setValues((v) => v.map((old, i) => (i === toggle ? 1 - old : old)))
        return
      }
      const code = chordForKey(e.code)
      if (code === undefined) return
      e.preventDefault()
      if (e.repeat || held.has(e.code)) return
      held.set(e.code, code)
      engine.pressChord(code)
      started()
    }
    const up = (e: KeyboardEvent) => {
      const code = held.get(e.code)
      if (code === undefined) return
      held.delete(e.code)
      engine.releaseChord(code)
    }
    const releaseAll = () => {
      for (const code of held.values()) engine.releaseChord(code)
      held.clear()
    }
    window.addEventListener('keydown', down)
    window.addEventListener('keyup', up)
    window.addEventListener('blur', releaseAll) // switching tabs or windows mid-chord must not leave it stuck
    return () => {
      window.removeEventListener('keydown', down)
      window.removeEventListener('keyup', up)
      window.removeEventListener('blur', releaseAll)
      releaseAll()
    }
  }, [engine])

  // MIDI controllers, where the browser supports Web MIDI. Asking needs no permission prompt for notes.
  useEffect(() => {
    if (!navigator.requestMIDIAccess) return
    let access: MIDIAccess | null = null
    const listen = () => {
      if (!access) return
      let count = 0
      access.inputs.forEach((input) => {
        count++
        input.onmidimessage = (m) => {
          const d = m.data
          if (d && d.length >= 3) {
            engine.midi(d[0], d[1], d[2])
            started()
          }
        }
      })
      setMidiInputs(count)
    }
    navigator.requestMIDIAccess().then((a) => {
      access = a
      a.onstatechange = listen
      listen()
    }, () => {})
    return () => {
      access?.inputs.forEach((input) => (input.onmidimessage = null))
      if (access) access.onstatechange = null
    }
  }, [engine])

  const holding = state.held >= 0
  const seqOn = values[ParamId.seqOn] > 0.5
  const rhythmOn = values[ParamId.rhythmOn] > 0.5
  const rhythm = RHYTHMS[values[ParamId.rhythmPattern]]
  const beats = rhythm.length / rhythm.stepsPerBeat
  const onBeat = state.rhythmStep >= 0 && state.rhythmStep % rhythm.stepsPerBeat === 0

  return (
    <main className="stage">
      <div className="body">
        <header className="top">
          <div className="brand">
            <h1>SDChord</h1>
            <p className="subtitle">{t.subtitle}</p>
          </div>
          <div className="readout-group">
            <div className="led-window chord-readout">
              <span className={holding ? 'lit' : 'dim'}>{chordName(holding ? state.held : state.memory)}</span>
            </div>
            <span className="print small">{t.chord}</span>
          </div>
          <div className="power">
            <span className={`led big${running ? ' on' : ''}`} />
            <span className="print small">{t.power}</span>
          </div>
          {/* Hold mode: tapped chords keep sounding until another is tapped. On by default on touch screens. */}
          <button className={`key panel-toggle${latch ? ' down' : ''}`} aria-pressed={latch} onClick={() => setLatch((v) => !v)}>
            <span className={`led${latch ? ' on' : ''}`} />
            <span className="key-label">{t.holdKey}</span>
          </button>
          {/* Shows or hides the rhythm machine's panel. The LED says whether the drums are playing, even hidden. */}
          <button
            className={`key panel-toggle${showRhythm ? ' down' : ''}`}
            aria-expanded={showRhythm}
            aria-controls="rhythm-panel"
            onClick={() => setShowRhythm((v) => !v)}
          >
            <span className={`led${rhythmOn ? ' on' : ''}`} />
            <span className="key-label">{t.sections.rhythm} {showRhythm ? '▴' : '▾'}</span>
          </button>
          <div className="lang" role="radiogroup" aria-label="Language">
            {(['es', 'en'] as const).map((l) => (
              <button key={l} role="radio" aria-checked={lang === l} className={lang === l ? 'on' : ''} onClick={() => setLang(l)}>
                {l.toUpperCase()}
              </button>
            ))}
          </div>
        </header>
        <div className="stripes" aria-hidden="true">
          <i />
          <i />
          <i />
        </div>

        <div className="layout">
          <div className="controls">
            <section className="recess grid-panel">
              <ChordGrid
                rowNames={t.rows}
                held={state.held}
                memory={state.memory}
                latch={latch}
                onPress={(code) => {
                  engine.pressChord(code)
                  started()
                }}
                onRelease={(code) => engine.releaseChord(code)}
              />
            </section>
            {!running && <p className="start-hint">{t.tapToStart}</p>}

            <section className="recess knob-panel">
              {SECTIONS.map((s) => (
                <div className="knob-section" key={s.title}>
                  <h2 className={s.knobs.length > 1 ? 'bracket' : ''}>
                    <span>{t.sections[s.title]}</span>
                  </h2>
                  <div className="knob-row">
                    {s.knobs.map(([id, label]) => (
                      <Knob key={id} id={id} value={values[id]} label={t.knobs[label]} lang={lang} onChange={(v) => setParam(id, v)} />
                    ))}
                  </div>
                </div>
              ))}
            </section>

            <section className="recess seq-panel">
              <h2 className="bracket wide">
                <span>{t.sections.auto}</span>
              </h2>
              <div className="seq-row">
                <button
                  className={`key toggle${seqOn ? ' down' : ''}`}
                  aria-pressed={seqOn}
                  onClick={() => setParam(ParamId.seqOn, seqOn ? 0 : 1)}
                >
                  <span className={`led${seqOn ? ' on' : ''}`} />
                  <span className="key-label">{t.autoKey}</span>
                </button>
                <div className={`led-window seq-readout${seqOn ? '' : ' off'}`}>
                  <span className="line1">{PARAMS[ParamId.seqPattern].format(values[ParamId.seqPattern], lang).toUpperCase()}</span>
                  <span className="line2">
                    {PARAMS[ParamId.seqRate].format(values[ParamId.seqRate], lang)} · {Math.round(values[ParamId.seqBpm])} BPM
                  </span>
                </div>
                <div className="knob-row">
                  {SEQ_KNOBS.map(([id, label]) => (
                    <Knob key={id} id={id} value={values[id]} label={t.knobs[label]} lang={lang} onChange={(v) => setParam(id, v)} />
                  ))}
                </div>
              </div>
            </section>

            {showRhythm && (
            <section className="recess rhythm-panel" id="rhythm-panel">
              <h2 className="bracket wide">
                <span>{t.sections.rhythm}</span>
              </h2>
              <div className="seq-row">
                <button
                  className={`key toggle${rhythmOn ? ' down' : ''}`}
                  aria-pressed={rhythmOn}
                  onClick={() => setValues((v) => v.map((old, i) => (i === ParamId.rhythmOn ? 1 - old : old)))}
                >
                  {/* Blinks on every beat while the drums play. */}
                  <span className={`led${rhythmOn && onBeat ? ' on' : ''}`} />
                  <span className="key-label">{t.startKey}</span>
                </button>
                <div className={`led-window seq-readout${rhythmOn ? '' : ' off'}`}>
                  <span className="line1">{rhythm.name[lang].toUpperCase()}</span>
                  <span className="line2">
                    {beats}/4 · {Math.round(values[ParamId.seqBpm])} BPM
                  </span>
                </div>
                <div className="knob-row">
                  {RHYTHM_KNOBS.map(([id, label]) => (
                    <Knob key={id} id={id} value={values[id]} label={t.knobs[label]} lang={lang} onChange={(v) => setParam(id, v)} />
                  ))}
                </div>
              </div>
              {/* One LED per step, grouped by beat, like the step lights on a drum machine. */}
              <div className="steps" aria-hidden="true">
                {Array.from({ length: beats }, (_, beat) => (
                  <div className="beat" key={beat}>
                    {Array.from({ length: rhythm.stepsPerBeat }, (_, i) => {
                      const step = beat * rhythm.stepsPerBeat + i
                      return <span key={i} className={`led${rhythmOn && state.rhythmStep === step ? ' on' : ''}`} />
                    })}
                  </div>
                ))}
              </div>
              <p className="panel-note">{t.rhythmHint}</p>
            </section>
            )}
          </div>

          <StrumPlate
            title={t.plate}
            hint={t.plateHint}
            hintWide={t.plateHintWide}
            memory={state.memory}
            glow={state.glow}
            onPluck={(s, v) => {
              engine.pluck(s, v)
              started()
            }}
          />
        </div>

        <footer className="help">
          <p>{t.keyboardHelp}</p>
          <p>{t.midiHelp}{midiInputs > 0 && <strong> · {t.midiConnected(midiInputs)}</strong>}</p>
        </footer>
      </div>
      <p className="about">
        {t.about} · <a href={REPO}>{t.source}</a>
      </p>
    </main>
  )
}
