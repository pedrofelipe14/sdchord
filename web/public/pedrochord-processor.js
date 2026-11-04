// Runs the C++ engine (compiled to pedrochord.wasm) on the browser's audio thread.
// The main thread sends the .wasm bytes once, then notes and parameter changes as messages.
// About 45 times a second it gets back what the UI shows: the held chord, how loud each string rings,
// and where the rhythm machine is in its bar.

const STRINGS = 13
const STATE_EVERY_BLOCKS = 4 // 4 x 128 frames ≈ 11 ms at 48 kHz: quick enough for the step LEDs

class PedrochordProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super()
    const module = new WebAssembly.Module(options.processorOptions.wasmBytes)

    // The engine needs no host functions; whatever libc stubs the toolchain left
    // behind (e.g. WASI's proc_exit) get harmless no-ops.
    const imports = {}
    for (const imp of WebAssembly.Module.imports(module)) {
      imports[imp.module] ??= {}
      if (imp.kind === 'function') imports[imp.module][imp.name] = () => 0
    }

    const { exports } = new WebAssembly.Instance(module, imports)
    exports._initialize?.() // runs C++ static constructors in a reactor-style module
    exports.pc_init(sampleRate)
    this.engine = exports
    // Memory never grows (ALLOW_MEMORY_GROWTH=0), so these views stay valid.
    const memory = new Float32Array(exports.memory.buffer)
    this.left = memory.subarray(exports.pc_left() >> 2)
    this.right = memory.subarray(exports.pc_right() >> 2)
    this.blocks = 0

    this.port.onmessage = ({ data }) => this.handle(data)
  }

  handle(msg) {
    const e = this.engine
    switch (msg.type) {
      case 'noteOn': e.pc_note_on(msg.note, msg.velocity); break
      case 'noteOff': e.pc_note_off(msg.note); break
      case 'cc': e.pc_control_change(msg.cc, msg.value); break
      case 'allNotesOff': e.pc_all_notes_off(); break
      case 'param': e.pc_set_param(msg.id, msg.value); break
    }
  }

  process(_inputs, outputs) {
    const [outL, outR] = outputs[0]
    const frames = outL.length
    const e = this.engine
    e.pc_process(frames)
    outL.set(this.left.subarray(0, frames))
    if (outR) outR.set(this.right.subarray(0, frames))

    if (++this.blocks % STATE_EVERY_BLOCKS === 0) {
      const glow = new Float32Array(STRINGS)
      for (let s = 0; s < STRINGS; s++) glow[s] = e.pc_string_glow(s)
      this.port.postMessage(
        { type: 'state', held: e.pc_held_chord(), memory: e.pc_string_chord(), glow, rhythmStep: e.pc_rhythm_step() },
        [glow.buffer],
      )
    }
    return true
  }
}

registerProcessor('pedrochord', PedrochordProcessor)
