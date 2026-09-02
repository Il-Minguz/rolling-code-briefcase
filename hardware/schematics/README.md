# Schematics

Extracted from the original report (`docs/original-report/`):

- **Power supply** — LM7805CT with 100 nF / 10 µF on input and output. Note that the drawing
  is labelled +12 V at the input while the text and photographs both show a 9 V battery.
- **Lock driver** — BDX53C Darlington internal equivalent circuit, showing the integrated
  base-emitter resistors (R1 ≈ 8.4 kΩ, R2 ≈ 0.3 kΩ) and the freewheel diode.

The LED + photoresistor workaround was never drawn. See `docs/KNOWN_ISSUES.md` H-1.
