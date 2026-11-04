import { type CSSProperties, useEffect, useRef, useState } from 'react'
import { STRINGS, noteName, stringNotes } from '../engine/chords'

interface Props {
  title: string
  hint: string
  hintWide: string
  memory: number // chord the strings play
  glow: Float32Array
  onPluck: (string: number, velocity: number) => void
}

/** Phones and narrow windows lay the plate out sideways, under the chord keys. */
const WIDE_QUERY = '(max-width: 860px)'

/** The strum plate. Tall (desktop): lowest string at the bottom. Wide (phone): lowest on the left.
    Touch a string to pluck it, drag across to strum; faster strums play louder. */
export function StrumPlate({ title, hint, hintWide, memory, glow, onPluck }: Props) {
  const area = useRef<HTMLDivElement>(null)
  const fingers = useRef(new Map<number, { position: number; time: number }>())
  const [wide, setWide] = useState(() => matchMedia(WIDE_QUERY).matches)

  useEffect(() => {
    const query = matchMedia(WIDE_QUERY)
    const update = () => setWide(query.matches)
    query.addEventListener('change', update)
    return () => query.removeEventListener('change', update)
  }, [])

  // 0 = lowest string, 1 = highest.
  const positionAt = (x: number, y: number): number => {
    const r = area.current!.getBoundingClientRect()
    const p = wide ? (x - r.left) / r.width : 1 - (y - r.top) / r.height
    return Math.min(1, Math.max(0, p))
  }

  const notes = stringNotes(memory)

  return (
    <div className={`plate-frame${wide ? ' wide' : ''}`}>
      <div className="plate-title">{title}</div>
      <div
        className="plate"
        ref={area}
        onPointerDown={(e) => {
          e.currentTarget.setPointerCapture(e.pointerId)
          const p = positionAt(e.clientX, e.clientY)
          fingers.current.set(e.pointerId, { position: p, time: performance.now() })
          onPluck(Math.min(STRINGS - 1, Math.floor(p * STRINGS)), 0.75)
        }}
        onPointerMove={(e) => {
          const finger = fingers.current.get(e.pointerId)
          if (!finger) return
          const p = positionAt(e.clientX, e.clientY)
          const now = performance.now()
          const speed = Math.abs(p - finger.position) / Math.max(0.001, (now - finger.time) / 1000) // plates per second
          const velocity = Math.min(1, Math.max(0.3, 0.35 + speed * 0.12))
          const lo = Math.min(finger.position, p)
          const hi = Math.max(finger.position, p)
          for (let s = 0; s < STRINGS; s++) {
            const centre = (s + 0.5) / STRINGS
            if (centre > lo && centre <= hi) onPluck(s, velocity)
          }
          fingers.current.set(e.pointerId, { position: p, time: now })
        }}
        onPointerUp={(e) => fingers.current.delete(e.pointerId)}
        onPointerCancel={(e) => fingers.current.delete(e.pointerId)}
      >
        {notes.map((note, s) => {
          const offset = `${((s + 0.5) / STRINGS) * 100}%`
          const level = Math.min(1, glow[s] * 1.6)
          return (
            <div
              key={s}
              className="string"
              style={{ [wide ? 'left' : 'bottom']: offset, '--ring': level.toFixed(3) } as CSSProperties}
            >
              <span className="string-note">{noteName(note)}</span>
            </div>
          )
        })}
      </div>
      <div className="plate-hint">{wide ? hintWide : hint}</div>
    </div>
  )
}
