import { useRef, useState } from 'react'
import { type Lang, PARAMS, type ParamId, clampParam } from '../engine/params'

interface Props {
  id: ParamId
  value: number
  label: string
  lang: Lang
  onChange: (value: number) => void
}

const START = -135 // degrees, 0 = straight up
const END = 135
const RIBS = 28

const polar = (radius: number, degrees: number) => {
  const a = ((degrees - 90) * Math.PI) / 180
  return { x: 30 + radius * Math.cos(a), y: 30 + radius * Math.sin(a) }
}

/** A black knurled knob with its scale printed around it. Drag up/down (or scroll) to turn it,
    double-click to reset. Stepped parameters get one mark per position, like a rotary switch. */
export function Knob({ id, value, label, lang, onChange }: Props) {
  const spec = PARAMS[id]
  const [active, setActive] = useState(false)
  const drag = useRef<{ y: number; value: number } | null>(null)

  const positions = spec.step ? Math.round((spec.max - spec.min) / spec.step) : 0
  const stepped = positions > 0 && positions <= 12
  const marks = stepped ? positions : 10
  const angle = START + ((value - spec.min) / (spec.max - spec.min)) * (END - START)
  // Pixels of mouse travel for the whole range (stepped knobs click through positions sooner).
  const travel = stepped ? positions * 22 : 180

  const set = (v: number) => onChange(clampParam(id, v))

  return (
    <div className="knob">
      <svg
        viewBox="0 0 60 60"
        role="slider"
        aria-label={label}
        aria-valuemin={spec.min}
        aria-valuemax={spec.max}
        aria-valuenow={value}
        aria-valuetext={spec.format(value, lang)}
        tabIndex={-1}
        onPointerDown={(e) => {
          e.currentTarget.setPointerCapture(e.pointerId)
          drag.current = { y: e.clientY, value }
          setActive(true)
        }}
        onPointerMove={(e) => {
          if (!drag.current) return
          const delta = ((drag.current.y - e.clientY) / travel) * (spec.max - spec.min)
          set(drag.current.value + delta)
        }}
        onPointerUp={() => {
          drag.current = null
          setActive(false)
        }}
        onPointerCancel={() => {
          drag.current = null
          setActive(false)
        }}
        onWheel={(e) => set(value + (e.deltaY < 0 ? 1 : -1) * (spec.step ?? (spec.max - spec.min) / 50))}
        onDoubleClick={() => set(spec.def)}
      >
        {Array.from({ length: marks + 1 }, (_, i) => {
          const a = START + ((END - START) * i) / marks
          const major = stepped || i % 5 === 0
          const p1 = polar(major ? 23 : 25, a)
          const p2 = polar(29, a)
          return <line key={i} className="knob-tick" x1={p1.x} y1={p1.y} x2={p2.x} y2={p2.y} strokeWidth={major ? 1.4 : 0.9} />
        })}
        <circle cx="30" cy="32.5" r="19.5" className="knob-shadow" />
        <circle cx="30" cy="30" r="19.5" className="knob-skirt" />
        {Array.from({ length: RIBS }, (_, i) => {
          const a = (360 * i) / RIBS + angle
          const p1 = polar(15.5, a)
          const p2 = polar(19, a)
          return <line key={i} className="knob-rib" x1={p1.x} y1={p1.y} x2={p2.x} y2={p2.y} />
        })}
        <circle cx="30" cy="30" r="15.5" className="knob-cap" />
        {(() => {
          const p1 = polar(4, angle)
          const p2 = polar(14, angle)
          return <line className="knob-pointer" x1={p1.x} y1={p1.y} x2={p2.x} y2={p2.y} />
        })()}
      </svg>
      <span className="knob-label">{label}</span>
      {active && <span className="knob-value">{spec.format(value, lang)}</span>}
    </div>
  )
}
