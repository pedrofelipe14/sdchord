<div align="center">

# Pedrochord

**Un instrumento electrónico estilo omnichord en formato VST3: mantenés un botón de acorde, rasgueás la franja y cada cuerda toca una nota de ese acorde.**

[English](README.md) · **Español**

[![Build](https://github.com/pedrofelipe14/pedrochord/actions/workflows/build.yml/badge.svg)](https://github.com/pedrofelipe14/pedrochord/actions/workflows/build.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![JUCE](https://img.shields.io/badge/JUCE-9-8DC63F)
![VST3](https://img.shields.io/badge/VST3-Standalone-555)
![pluginval](https://img.shields.io/badge/pluginval-strictness_10-2ea44f)
![WebAssembly](https://img.shields.io/badge/WebAssembly-654FF0?logo=webassembly&logoColor=white)

![Pedrochord tocando La menor 7 después de un rasgueo](docs/screenshot.png)

**▶ [Tocalo en el navegador](https://pedrofelipe14.github.io/pedrochord/)**: el mismo motor en C++ compilado a WebAssembly, en la compu o en el celu.

</div>

## Cómo se toca

- **Botones de acorde:** 12 tonalidades × 4 tipos (mayor, menor, 7 y m7). Mientras mantenés uno, suena su acorde como un colchón suave tipo órgano, con la fundamental en el bajo. Deslizá hasta otro botón para cambiar de acorde; si lo soltás, vuelve el que tenías apretado antes.
- **Franja de rasgueo:** 13 cuerdas, la más grave abajo. Tocá una para pulsarla o arrastrá sobre varias para rasguear; cuanto más rápido, más fuerte. Cada cuerda toca la siguiente nota del acorde, así que todo rasgueo suena afinado. Las cuerdas se quedan con el último acorde aunque sueltes el botón.
- **Sonido:** las cuerdas son una voz FM de dos operadores cuyo brillo se apaga antes que el cuerpo: arrancan cristalinas y se van apagando suaves. Hay controles de niveles, sustain y brillo de las cuerdas, chorus, reverb y *wobble*, una desafinación lenta tipo cinta.

- **Rasgueo automático:** un secuenciador que toca las cuerdas solo, a tempo, mientras mantenés un acorde. Tiene cinco patrones (arriba, abajo, ida y vuelta, rasgueo completo alternando bajada y subida, y al azar), figuras de negra a semicorchea con tresillos, y un rango que define cuántas cuerdas usa. El primer paso de cada tiempo va acentuado. Con SYNC sigue el tempo del DAW. La barra espaciadora lo prende y lo apaga.

- **Caja de ritmos (versión web):** ocho ritmos de un compás, preprogramados al estilo de las cajas de ritmo viejas (rock, disco, balada, bossa nova, cumbia, rumba, vals y swing). Cada sonido de batería es sintetizado, sin samples: bombo, redoblante, aro, hi-hat abierto y cerrado, dos congas y cencerro. Comparte el tempo con el rasgueo automático y, mientras suena, marca el pulso: los rasgueos se enganchan a su grilla. Enter la arranca y la frena.

### Teclado de la compu

`1`–`0` mayor · `Q`–`P` menor · `A`–`Ñ` séptima · `Z`–`M` menor séptima (desde C, de izquierda a derecha, teclado latinoamericano) · espacio: rasgueo automático · enter: caja de ritmos (web).

### Desde un DAW (MIDI)

| Notas | Toca |
|---|---|
| C1–B1 | Acorde mayor en esa tonalidad |
| C2–B2 | Menor |
| C3–B3 | Séptima |
| C4–B4 | Menor séptima |
| C5–C6 | Cuerdas 1 a 13 |
| CC1 (rueda de modulación) | Recorre las cuerdas como un dedo sobre la franja |

## Cómo está hecho

```
dsp/      el instrumento: C++17 header-only, sin JUCE, así lo comparten el plugin y la web
  include/pedrochord/chords.h    acordes, voicings y el mapeo MIDI
  include/pedrochord/voices.h    voces de colchón, bajo y cuerdas FM
  include/pedrochord/effects.h   chorus y una reverb chica estilo Freeverb
  include/pedrochord/engine.h    pila de acordes apretados, lógica de rasgueo, el secuenciador, el reloj compartido y la mezcla
  include/pedrochord/rhythm.h    la caja de ritmos: voces de batería sintetizadas y los patrones
  tests/                       28 tests nativos
plugin/   la capa JUCE y el editor, dibujado como hardware de los 80 (cuerpo naranja, display de LEDs, placa ranurada)
tools/    un renderizador fuera de pantalla que saca la captura de arriba
dsp/src/wasm_api.cpp + scripts/build-wasm.sh   una API en C sobre el motor, compilada a un .wasm de 23 KB sin JS generado
web/      la versión para navegador: React + TypeScript, con el motor en un AudioWorklet
```

- **La interfaz también toca por MIDI.** Los botones y la franja mandan los mismos mensajes de nota que mandaría un DAW, por una cola segura entre hilos. Así, mouse, touch y MIDI comparten un solo camino de código, y los tests lo cubren.
- **Precisión de muestra.** El procesador renderiza hasta cada evento MIDI y recién ahí lo aplica, así que un rasgueo mantiene su tiempo dentro de un bloque.
- **Nada reserva memoria en el hilo de audio.** Las líneas de delay son arrays de tamaño fijo, y la interfaz lee el estado del motor (acorde apretado, brillo de cada cuerda) desde atomics.
- **Validado como lo cargaría un DAW:** el CI corre [pluginval](https://github.com/Tracktion/pluginval) en su nivel más estricto (10) en cada push: distintos sample rates y tamaños de bloque, guardar y restaurar estado, automatización, valores aleatorios en los parámetros y abrir el editor mientras procesa audio.
- **Un motor, dos casas.** El navegador corre exactamente el mismo código de `dsp/`: el AudioWorklet instancia el .wasm directamente, le manda notas y parámetros como mensajes, y recibe el acorde apretado y el nivel de cada cuerda para dibujarlos. La página tiene teclas de acorde multitouch por las que se puede deslizar el dedo, una franja que en el celu se pone horizontal, el teclado de la compu por posición física de la tecla (sirve para cualquier distribución) y controladores Web MIDI. Sus 14 tests incluyen el motor compilado y un chequeo de que los IDs de parámetros coincidan con los del C++.
- **Testeado sin DAW:** el tempo del rasgueo automático en cada figura y el orden de cada patrón, los voicings, el mapeo MIDI, la pila de acordes, que un rasgueo pulse las cuerdas correctas, que el decay no dependa del sample rate, y una prueba de estrés con todo al máximo que no da valores inválidos ni llega a saturar.

## Compilar

Hace falta CMake 3.22+ y Visual Studio 2022 (u otro compilador C++17). JUCE es un submódulo.

```sh
git clone --recursive https://github.com/pedrofelipe14/pedrochord.git
cmake -B build
cmake --build build --config Release --target Pedrochord_All
# Versión web (necesita Emscripten y Node):
./scripts/build-wasm.sh && cd web && npm install && npm run dev
# Solo los tests del DSP:
cmake -S dsp -B dsp/build && cmake --build dsp/build --config Release && dsp/build/Release/dsp_tests
```

## Próximos pasos

- [x] Rasgueo automático (secuenciador).
- [x] Caja de ritmos con patrones preprogramados, sincronizada con el rasgueo automático (versión web).
- [ ] Un bajo automático que siga al acorde a tempo con la batería.
- [x] Tocar con el teclado de la compu: 1-0 mayor, Q-P menor, A-Ñ 7, Z-M m7 (teclado latinoamericano).
- [x] La versión web: el mismo `dsp/` compilado a WebAssembly, para tocar desde el celular.
