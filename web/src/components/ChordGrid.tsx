import { useRef, useState } from 'react'
import { CHORD_TYPES, ROOT_NAMES, chordName } from '../engine/chords'

interface Props {
  rowNames: string[]
  held: number // from the engine
  memory: number
  onPress: (code: number) => void
  onRelease: (code: number) => void
}

/** 12 roots x 4 chord types as raised keys. Hold one to play its chord; slide onto another to switch.
    Every finger (pointer) is tracked on its own, so two thumbs can play at once. */
export function ChordGrid({ rowNames, held, memory, onPress, onRelease }: Props) {
  const fingers = useRef(new Map<number, number>()) // pointerId -> chord code
  const [down, setDown] = useState<number[]>([])

  const codeAt = (x: number, y: number): number | undefined => {
    const el = document.elementFromPoint(x, y)?.closest<HTMLElement>('[data-code]')
    return el ? Number(el.dataset.code) : undefined
  }

  const move = (pointer: number, code: number | undefined) => {
    const current = fingers.current.get(pointer)
    if (current === code) return
    if (current !== undefined) onRelease(current)
    if (code === undefined) fingers.current.delete(pointer)
    else {
      fingers.current.set(pointer, code)
      onPress(code)
    }
    setDown([...fingers.current.values()])
  }

  return (
    <div
      className="chord-grid"
      onPointerDown={(e) => {
        const code = codeAt(e.clientX, e.clientY)
        if (code === undefined) return
        e.currentTarget.setPointerCapture(e.pointerId) // keep getting moves while sliding across keys
        move(e.pointerId, code)
      }}
      onPointerMove={(e) => {
        if (!fingers.current.has(e.pointerId)) return
        const code = codeAt(e.clientX, e.clientY)
        if (code !== undefined) move(e.pointerId, code)
      }}
      onPointerUp={(e) => move(e.pointerId, undefined)}
      onPointerCancel={(e) => move(e.pointerId, undefined)}
      onContextMenu={(e) => e.preventDefault()}
    >
      {Array.from({ length: CHORD_TYPES }, (_, type) => (
        <div className="chord-row" key={type}>
          <span className="row-name">{rowNames[type]}</span>
          {Array.from({ length: 12 }, (_, root) => {
            const code = root + 12 * type
            const pressed = held === code || down.includes(code)
            const remembered = held < 0 && !pressed && memory === code
            return (
              <div key={code} data-code={code} className={`key${pressed ? ' down' : ''}`} role="button" aria-pressed={pressed}>
                <span className={`led${pressed ? ' on' : remembered ? ' dim' : ''}`} />
                {/* On phones only the root shows: the row already says which kind of chord it is. */}
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
