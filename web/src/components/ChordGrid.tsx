import { useEffect, useRef, useState } from 'react'
import { CHORD_TYPES, COLUMN_ROOTS, ROOT_NAMES, chordName } from '../engine/chords'

interface Props {
  rowNames: string[]
  held: number // from the engine
  memory: number
  /** Hold mode: a tapped chord keeps sounding until another one is tapped (or the same one again). */
  latch: boolean
  /** Changes when the engine let go of every chord (the loop took over): hold mode forgets its chord. */
  forget: number
  onPress: (code: number) => void
  onRelease: (code: number) => void
}

/** 12 roots x 4 chord types as raised keys. Hold one to play its chord; slide onto another to switch.
    Every finger (pointer) is tracked on its own, so two thumbs can play at once. */
export function ChordGrid({ rowNames, held, memory, latch, forget, onPress, onRelease }: Props) {
  const fingers = useRef(new Map<number, number>()) // pointerId -> chord code under it
  const latched = useRef<number | undefined>(undefined) // the chord hold mode keeps sounding
  const [down, setDown] = useState<number[]>([])

  const show = () => setDown([...new Set([...fingers.current.values(), ...(latched.current === undefined ? [] : [latched.current])])])

  /** Hold mode: switch the sounding chord to `code`, or silence it when `code` is undefined. */
  const latchTo = (code: number | undefined) => {
    if (latched.current === code) return
    if (latched.current !== undefined) onRelease(latched.current)
    latched.current = code
    if (code !== undefined) onPress(code)
  }

  // Turning hold mode off lets go of the held chord.
  useEffect(() => {
    if (!latch) {
      latchTo(undefined)
      show()
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [latch])

  useEffect(() => {
    latched.current = undefined // already released in the engine: no release to send
    show()
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [forget])

  const codeAt = (x: number, y: number): number | undefined => {
    const el = document.elementFromPoint(x, y)?.closest<HTMLElement>('[data-code]')
    return el ? Number(el.dataset.code) : undefined
  }

  /** Normal mode: the chord under each finger sounds while the finger stays down. */
  const move = (pointer: number, code: number | undefined) => {
    const current = fingers.current.get(pointer)
    if (current === code) return
    if (current !== undefined) onRelease(current)
    if (code === undefined) fingers.current.delete(pointer)
    else {
      fingers.current.set(pointer, code)
      onPress(code)
    }
    show()
  }

  return (
    <div
      className="chord-grid"
      onPointerDown={(e) => {
        const code = codeAt(e.clientX, e.clientY)
        if (code === undefined) return
        e.currentTarget.setPointerCapture(e.pointerId) // keep getting moves while sliding across keys
        if (latch) {
          fingers.current.set(e.pointerId, code)
          latchTo(latched.current === code ? undefined : code) // tapping the sounding chord again stops it
          show()
        } else move(e.pointerId, code)
      }}
      onPointerMove={(e) => {
        if (!fingers.current.has(e.pointerId)) return
        const code = codeAt(e.clientX, e.clientY)
        if (code === undefined || code === fingers.current.get(e.pointerId)) return
        if (latch) {
          fingers.current.set(e.pointerId, code)
          latchTo(code) // sliding onto another chord switches to it
          show()
        } else move(e.pointerId, code)
      }}
      onPointerUp={(e) => {
        if (latch) {
          fingers.current.delete(e.pointerId)
          show()
        } else move(e.pointerId, undefined)
      }}
      onPointerCancel={(e) => {
        if (latch) {
          fingers.current.delete(e.pointerId)
          show()
        } else move(e.pointerId, undefined)
      }}
      onContextMenu={(e) => e.preventDefault()}
    >
      {Array.from({ length: CHORD_TYPES }, (_, type) => (
        <div className="chord-row" key={type}>
          <span className="row-name">{rowNames[type]}</span>
          {COLUMN_ROOTS.map((root) => {
            const code = root + 12 * type
            const pressed = held === code || down.includes(code)
            const remembered = held < 0 && !pressed && memory === code
            return (
              <div key={code} data-code={code} className={`key${pressed ? ' down' : ''}`} role="button" aria-pressed={pressed}>
                <span className={`led${pressed ? ' on' : remembered ? ' dim' : ''}`} />
                {/* On phones the chord type goes small under the root (see styles.css). */}
                <span className="key-label">
                  {ROOT_NAMES[root]}
                  <span className="suffix">{chordName(code).slice(ROOT_NAMES[root].length)}</span>
                </span>
              </div>
            )
          })}
        </div>
      ))}
    </div>
  )
}
