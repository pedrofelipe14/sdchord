import { defaultValues, PARAM_COUNT, ParamId } from './params'
import { FIRST_CHORD_NOTE, FIRST_STRING_NOTE, STRINGS } from './chords'

type Message =
  | { type: 'noteOn'; note: number; velocity: number }
  | { type: 'noteOff'; note: number }
  | { type: 'cc'; cc: number; value: number }
  | { type: 'allNotesOff' }
  | { type: 'param'; id: ParamId; value: number }
  | { type: 'loopButton' }
  | { type: 'loopClear' }

/** The chord looper's states, as in engine.h. */
export enum LoopState {
  Empty,
  Armed, // waiting for the first chord
  Recording,
  Playing,
  Stopped,
}

/** What the engine reports back for the UI. */
export interface EngineState {
  held: number // chord being held (root + 12 * type), or -1
  memory: number // chord the strings play
  glow: Float32Array // how loud each string rings
  rhythmStep: number // the rhythm machine's step in its bar, or -1 when stopped
  loopState: LoopState
  loopBeats: number // the loop's length in beats (while recording: how long so far)
  loopPos: number // where the loop is, in beats
}

export const silentState = (): EngineState => ({
  held: -1,
  memory: 0,
  glow: new Float32Array(STRINGS),
  rhythmStep: -1,
  loopState: LoopState.Empty,
  loopBeats: 0,
  loopPos: 0,
})

/** A tenth of a second of silence as a WAV file (8-bit mono), for the iPhone silent-switch workaround. */
function silentWavUrl(): string {
  const samples = 800
  const bytes = new Uint8Array(44 + samples)
  const view = new DataView(bytes.buffer)
  const text = (at: number, s: string) => [...s].forEach((c, i) => view.setUint8(at + i, c.charCodeAt(0)))
  text(0, 'RIFF')
  view.setUint32(4, 36 + samples, true)
  text(8, 'WAVEfmt ')
  view.setUint32(16, 16, true) // fmt chunk size
  view.setUint16(20, 1, true) // PCM
  view.setUint16(22, 1, true) // mono
  view.setUint32(24, 8000, true) // sample rate
  view.setUint32(28, 8000, true) // byte rate
  view.setUint16(32, 1, true) // block align
  view.setUint16(34, 8, true) // bits per sample
  text(36, 'data')
  view.setUint32(40, samples, true)
  bytes.fill(128, 44) // 8-bit silence is the midpoint
  return URL.createObjectURL(new Blob([bytes], { type: 'audio/wav' }))
}

/** Main-thread handle on the instrument running in the AudioWorklet.
    Browsers only allow audio after a user gesture, so start() is called from one. Whatever is
    played or set before the engine is up is queued and sent once it is. */
export class SDChordEngine {
  private node: AudioWorkletNode | null = null
  private starting: Promise<void> | null = null
  private queue: Message[] = []
  private values = defaultValues()
  private listeners = new Set<(state: EngineState) => void>()

  get running(): boolean {
    return this.node !== null
  }

  private context: AudioContext | null = null
  private wasmBytes: Promise<ArrayBuffer> | null = null
  private silentLoop: HTMLAudioElement | null = null

  /** Downloads the engine (and warms the cache for its worklet) ahead of time, so the first touch
      only has to switch the audio on. */
  preload(): void {
    const base = import.meta.env.BASE_URL
    this.wasmBytes ??= fetch(`${base}sdchord.wasm`).then((r) => r.arrayBuffer())
    void fetch(`${base}sdchord-processor.js`).catch(() => {})
  }

  start(): Promise<void> {
    this.starting ??= this.boot()
    return this.starting
  }

  /** Call from every user gesture (pointerup, touchend, click, keydown). Browsers only allow audio to
      start inside one, and on touch screens Safari does not count touching down, only lifting the
      finger, so the first attempt may be refused: this tries again until the audio is running. */
  unlock(): void {
    void this.start()
    if (this.context && this.context.state !== 'running') void this.context.resume().catch(() => {})
    this.ensurePlaybackSession()
  }

  /** On iPhone, web audio is muted by the silent switch unless the page asks for media playback. */
  private ensurePlaybackSession(): void {
    const session = (navigator as Navigator & { audioSession?: { type: string } }).audioSession
    if (session) {
      session.type = 'playback' // Safari 16.4+
      return
    }
    // Older iOS: a looping silent <audio> element puts the page in the playback category too.
    if (!this.silentLoop && /iPad|iPhone|iPod/.test(navigator.userAgent)) {
      this.silentLoop = new Audio(silentWavUrl())
      this.silentLoop.loop = true
    }
    void this.silentLoop?.play().catch(() => {})
  }

  private async boot(): Promise<void> {
    // Create and resume the context right away, inside the user's gesture: Safari on iOS only lets
    // audio start there, not after the awaits below. If this gesture didn't count, unlock() retries.
    const context = new AudioContext({ latencyHint: 'interactive' })
    this.context = context
    void context.resume().catch(() => {})
    this.preload()
    const base = import.meta.env.BASE_URL
    const [wasmBytes] = await Promise.all([this.wasmBytes!, context.audioWorklet.addModule(`${base}sdchord-processor.js`)])

    const node = new AudioWorkletNode(context, 'sdchord', {
      numberOfInputs: 0,
      numberOfOutputs: 1,
      outputChannelCount: [2],
      processorOptions: { wasmBytes },
    })
    node.connect(context.destination)
    node.port.onmessage = ({ data }) => {
      if (data.type === 'state') for (const listener of this.listeners) listener(data as EngineState)
    }
    this.node = node

    // Bring the engine up to whatever the UI shows, then play what was queued meanwhile.
    for (let id = 0; id < PARAM_COUNT; id++) this.post({ type: 'param', id, value: this.values[id] })
    for (const message of this.queue.splice(0)) this.post(message)
  }

  /** Whether sound can actually be heard (the context exists and the browser let it run). */
  get audible(): boolean {
    return this.context?.state === 'running'
  }

  onState(listener: (state: EngineState) => void): () => void {
    this.listeners.add(listener)
    return () => this.listeners.delete(listener)
  }

  pressChord(code: number): void {
    this.send({ type: 'noteOn', note: FIRST_CHORD_NOTE + code, velocity: 1 })
  }

  releaseChord(code: number): void {
    this.send({ type: 'noteOff', note: FIRST_CHORD_NOTE + code })
  }

  pluck(string: number, velocity: number): void {
    this.send({ type: 'noteOn', note: FIRST_STRING_NOTE + string, velocity })
  }

  /** Raw MIDI from a connected controller: the same layout the plugin uses. */
  midi(status: number, data1: number, data2: number): void {
    const kind = status & 0xf0
    if (kind === 0x90 && data2 > 0) this.send({ type: 'noteOn', note: data1, velocity: data2 / 127 })
    else if (kind === 0x80 || kind === 0x90) this.send({ type: 'noteOff', note: data1 })
    else if (kind === 0xb0) this.send({ type: 'cc', cc: data1, value: data2 })
  }

  /** The looper's one button: record (from the first chord), close and play, stop, play again. */
  loopButton(): void {
    this.send({ type: 'loopButton' })
  }

  loopClear(): void {
    this.send({ type: 'loopClear' })
  }

  allNotesOff(): void {
    this.send({ type: 'allNotesOff' })
  }

  setParam(id: ParamId, value: number): void {
    this.values[id] = value
    this.post({ type: 'param', id, value }) // not queued: boot() sends every current value anyway
  }

  private send(message: Message): void {
    if (this.node) this.post(message)
    else {
      this.queue.push(message)
      void this.start()
    }
  }

  private post(message: Message): void {
    this.node?.port.postMessage(message)
  }
}
