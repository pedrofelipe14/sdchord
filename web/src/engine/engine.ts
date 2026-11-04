import { defaultValues, PARAM_COUNT, ParamId } from './params'
import { FIRST_CHORD_NOTE, FIRST_STRING_NOTE, STRINGS } from './chords'

type Message =
  | { type: 'noteOn'; note: number; velocity: number }
  | { type: 'noteOff'; note: number }
  | { type: 'cc'; cc: number; value: number }
  | { type: 'allNotesOff' }
  | { type: 'param'; id: ParamId; value: number }

/** What the engine reports back for the UI. */
export interface EngineState {
  held: number // chord being held (root + 12 * type), or -1
  memory: number // chord the strings play
  glow: Float32Array // how loud each string rings
}

export const silentState = (): EngineState => ({ held: -1, memory: 0, glow: new Float32Array(STRINGS) })

/** Main-thread handle on the instrument running in the AudioWorklet.
    Browsers only allow audio after a user gesture, so start() is called from one. Whatever is
    played or set before the engine is up is queued and sent once it is. */
export class PedrochordEngine {
  private node: AudioWorkletNode | null = null
  private starting: Promise<void> | null = null
  private queue: Message[] = []
  private values = defaultValues()
  private listeners = new Set<(state: EngineState) => void>()

  get running(): boolean {
    return this.node !== null
  }

  start(): Promise<void> {
    this.starting ??= this.boot()
    return this.starting
  }

  private async boot(): Promise<void> {
    const context = new AudioContext({ latencyHint: 'interactive' })
    const base = import.meta.env.BASE_URL
    const [wasmBytes] = await Promise.all([
      fetch(`${base}pedrochord.wasm`).then((r) => r.arrayBuffer()),
      context.audioWorklet.addModule(`${base}pedrochord-processor.js`),
    ])

    const node = new AudioWorkletNode(context, 'pedrochord', {
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
    await context.resume()
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
