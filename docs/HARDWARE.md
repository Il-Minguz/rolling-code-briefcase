# Hardware

Everything here is reconstructed from the surviving sketches, the 2024 school report, and
photographs of the assembled units. Where something could not be established from that
evidence it is marked **[unknown]** rather than guessed.

---

## 1. Units

The system is two physical objects and three microcontrollers.

### 1.1 The briefcase

An aluminium attaché case. Mounted through the shell: a 4×3 keypad, an SH1106 OLED, the round
GROW R557 fingerprint sensor, and a barrel key switch. Inside: the custom PCB carrying both
ESP32s, the E32 LoRa module, a solenoid lock, two buzzers, a panel voltmeter for the battery,
and a 9 V cell.

The case is my father's, and it still has its own original mechanical three-digit combination
lock on the lid, which works and is entirely unrelated to the electronics. Everything described
here was added around it.

### 1.2 The code transmitter

A hinged metal tin. Lid: the 16×2 LCD with its I²C backpack and a red status LED. Base: an
ESP32 dev board on a mini breadboard, the E32 module, a 9 V cell, a rocker switch, a relay
module, a trimmer, and an SMA antenna passing through the wall of the box.

The wiring here was never migrated to a PCB — the surviving photographs show it as a
breadboard build, taped and screwed into the tin.

> **Note on the relay** visible in the transmitter photographs (marked `6A 250VAC / 10A 12VDC`):
> nothing in `password_definitivo.ino` drives a relay, and the report does not mention one on
> the transmitter side. **[unknown]** — most likely a leftover from an earlier experiment, or
> used simply as a power switch.

---

## 2. Bill of materials

See [`BOM.md`](BOM.md).

---

## 3. Microcontrollers

Three ESP32-WROOM-32 development boards.

| Board | Location | Firmware |
|---|---|---|
| **A** | Briefcase, PCB position **U3** (38-pin devkit) | `valigia_definitivo.ino` |
| **B** | Briefcase, PCB position **U2** | `impronte_definitivo.ino` |
| **C** | Transmitter, on a breadboard | `password_definitivo.ino` |

The A/B to U2/U3 assignment is inferred from board pin counts in the PCB photograph and is
**[unknown]** with certainty.

### Why board B is an ESP32, not an Arduino Micro

`valigia_definitivo.ino` contains the comment *"se leggo che dall'arduino micro sono state
rilevate delle impronte giuste"*. This is a leftover from an earlier revision. The evidence
that board B is an ESP32:

- `impronte_definitivo.ino` uses `Serial2`; the Arduino Micro only has `Serial1`.
- It uses GPIO 25, 27 and 33, which do not exist on a Micro.
- The report's block diagram annotates the fingerprint sensor with *"dal secondo esp32: 16,17"*.
- The photograph of the finished PCB shows two ESP32-WROOM modules.

The report states the Micro was used only for the initial standalone testing of the sensor.

---

## 4. Pin mapping

### 4.1 ESP32 "A" — briefcase main controller

| Function | GPIO | Note |
|---|---|---|
| Keypad row R1 | 33 | |
| Keypad row R2 | 25 | |
| Keypad row R3 | 26 | |
| Keypad row R4 | 27 | |
| Keypad column C1 | 14 | |
| Keypad column C2 | **12** | ⚠ strapping pin (MTDI) — see §7 |
| Keypad column C3 | 13 | |
| OLED SDA / SCL | 21 / 22 | ESP32 I²C defaults; confirmed by the report's block diagram |
| Buzzer (`ezBuzzer`) | 15 | |
| LoRa UART2 RX / TX | 16 / 17 | |
| LoRa M0 | 4 | |
| LoRa M1 | 23 | |
| LoRa AUX | 5 | |
| `SUONO_OK` — unlock **and** success tune | **19** | drives the lock circuit *and* signals board B |
| `SUONO_noOK` — wrong code | **2** | ⚠ strapping pin / onboard LED on many devkits |
| `OK_IMPRONTE` — fingerprint match in | **34** | ⚠ input-only, no internal pull-up/pull-down |

### 4.2 ESP32 "B" — fingerprint and audio

| Function | GPIO | Note |
|---|---|---|
| R557 UART2 RX / TX | 16 / 17 | ESP32 defaults; `Serial2` never opened with explicit pins |
| R557 baud rate | — | 57600 (`finger.begin(57600)`) |
| Buzzer (`tone()`) | 5 | never `pinMode`'d; `tone()` configures it |
| `SUONO_OK` in (from A GPIO19) | 33 | |
| `SUONO_noOK` in (from A GPIO2) | 25 | |
| `OK_IMPRONTE` out (to A GPIO34) | 27 | |

### 4.3 ESP32 "C" — code transmitter

| Function | GPIO |
|---|---|
| LCD SDA / SCL | 21 / 22 |
| LoRa UART2 RX / TX | 16 / 17 |
| LoRa M0 | 4 |
| LoRa M1 | **18** |
| LoRa AUX | **19** |

Note that M1 and AUX differ between the two LoRa installations (23/5 on board A, 18/19 on
board C). Both are valid — they are different boards — but it is worth knowing when reading
the two sketches side by side.

### 4.4 Inter-MCU signalling inside the briefcase

Three single-bit GPIO lines, not a bus:

| Signal | From | To | Meaning |
|---|---|---|---|
| `SUONO_OK` | A GPIO19 | B GPIO33 | correct code / unlock — **also energises the lock driver** |
| `SUONO_noOK` | A GPIO2 | B GPIO25 | wrong code |
| `OK_IMPRONTE` | B GPIO27 | A GPIO34 | fingerprint matched |

---

## 5. The keypad

```text
Layout as defined in char keys[4][3]:
  1 2 3
  4 5 6
  7 8 9
  * 0 #

Rows    (pin_righe[4]):   R1→GPIO33   R2→GPIO25   R3→GPIO26   R4→GPIO27
Columns (pin_colonne[3]): C1→GPIO14   C2→GPIO12   C3→GPIO13
```

**Important caveat.** This is the combination that produced correct key readings *with the
harness as it was physically wired*. The wires leaving the keypad were reordered before they
reached the ESP32, so these arrays do not tell you which physical pin on the keypad connector
is "row 1". Working that out empirically is what made this part of the build so frustrating in
2024, and this documentation deliberately does not pretend to know the connector pinout.

Behaviour notes from the firmware:

- `#` validates the entry. There is no delete key.
- `*` has no special handling — it is appended to `input_password` like any other character.
- Input length is unbounded.
- Every keypress produces a 100 ms non-blocking beep via `ezBuzzer`.

---

## 6. Fingerprint sensor — GROW R557

Capacitive round module from Hangzhou Grow Technology, connected over UART at 3.3 V logic.
From the report's summary of the manufacturer's manual:

| Parameter | Value |
|---|---|
| Interface | UART, 3.3 V TTL |
| Supply | 3.3 V DC |
| Current | ~30 mA during capture, ~5 µA standby |
| Capacity | up to 120 templates |
| False accept | < 0.001 % |
| False reject | < 1 % |
| Capture time | < 0.2 s |
| Resolution | 508 dpi |
| Template generation | < 500 ms |

The module has an RGB ring used as status:

- **Red** — fingerprint not recognised
- **Green** — fingerprint recognised
- **Blue** — module powered, waiting for a finger

### The key switch

The sensor is powered through a barrel key switch mounted in the case. The firmware has **no
knowledge of the key at all** — verified: neither sketch declares or reads a key pin. The
switch is wired in series with the R557's supply, so with the key out the module is simply
unpowered and `getImage()` never succeeds. In the demonstration video the sensor's ring is dark
until the key is turned, then lights green.

This is, arguably, the most genuinely secure element in the whole design, and it is entirely
mechanical.

### Templates and the "binding" question

Templates live in the module's own flash, so enrolment travels with the sensor and not with
the host. See the corresponding section in [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) for why the
sensor appearing "bound" to one ESP32 is much more likely to be a changed stored parameter
(baud rate, handshake password, or device address) and/or 5 V over-drive during the early
Arduino Micro tests.

**Enrolment is not in any surviving file.** `impronte_definitivo.ino` only calls `getImage()`,
`image2Tz()` and `fingerSearch()` — it can match a finger but never register one. It does,
however, still print `"Adafruit Fingerprint sensor enrollment"` at startup, which is a leftover
from Adafruit's `enroll` example. That is the strongest available evidence for how the
templates were originally registered: with the stock Adafruit example, run once from a separate
sketch that no longer survives.

---

## 7. ESP32 pin hazards present in this design

None of these are known to have caused a failure in 2024, but all three are real and worth
flagging for anyone reading the schematic:

**GPIO12 as a keypad column.** GPIO12 is the MTDI strapping pin. If it is held high at reset,
the ESP32 configures the flash for 1.8 V and fails to boot. The `Keypad` library holds the
columns as `OUTPUT LOW` while scanning, but at the instant of reset the pin is still an input.
A key held down in column 2 during power-up, with the corresponding row driven high, can
prevent boot.

**GPIO2 as the "wrong code" output.** Also a strapping pin, and on most devkits it is tied to
the onboard LED. Using it as an output after boot is fine, but it adds a load during boot.

**GPIO34 as the fingerprint-match input.** GPIO 34–39 are input-only and have **no internal
pull-up or pull-down**. The line floats while board B is in reset or unpowered, so the state
read by board A during that window is undefined. In practice board B holds it low quickly
enough, but an external 100 kΩ pulldown would have made this deterministic.

---

## 8. Power

| | Briefcase | Transmitter |
|---|---|---|
| Battery | 9 V 6F22 | 9 V 6F22, **zinc-carbon** (VARTA Super Heavy Duty, visible in photographs) |
| Regulator | LM7805CT | LM7805CT |
| Filtering | 100 nF + 10 µF on input and output | same |
| Cooling | external heatsink with thermal paste | same |
| Monitoring | panel voltmeter module | resistive divider to an ESP32 ADC |
| Remembered draw | ~300 mA | ~150 mA |

The report's power schematic is labelled **+12 V** at the regulator input while the text and
photographs both show a 9 V battery. Treat the 12 V label as a drawing artefact.

The 7805 module itself survives as a small piece of perfboard carrying the TO-220 device on a
bracket, two 10 µF electrolytics, two 100 nF ceramics and two 3-pin headers.

### Why this supply was not adequate

Covered in detail in the README and in [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md). In short:

- The E32-433T30D draws **570–670 mA** while transmitting. A 6F22 cell has an internal
  resistance of several ohms and cannot deliver that without its terminal voltage collapsing.
- At 300 mA the LM7805 dissipates (9 − 5) × 0.3 = **1.2 W**, roughly 44 % of the total power
  drawn from the battery.
- The lock's 1.7 A is above the LM7805's 1.5 A rating regardless of everything else.

---

## 9. Lock and driver

| Item | Value | Source |
|---|---|---|
| Actuator | solenoid, model **[unknown]** | photographs / report |
| Measured draw | **1.7 A at 9 V** | bench-supply measurement, recalled |
| Nominal voltage | **[unknown]** | — |
| Driver | BDX53C Darlington, TO-220 | report |
| Base resistor | 1 kΩ, later 0 Ω | report / recollection |
| Lock supply | **separate** from the logic supply | recollection |
| Separate source | **18 V cordless-drill battery pack**, fitted as a stopgap and never replaced | recollection |

**BDX53C** (ST / onsemi) key figures:

| Parameter | Value |
|---|---|
| Vceo | 100 V |
| Ic continuous | 8 A |
| hFE min | **750** at Vce = 3 V, Ic = 3 A |
| Vce(sat) max | **2 V** at Ic = 3 A, Ib = 12 mA |
| Vbe(sat) max | **2.5 V** at the same conditions |
| Internal base-emitter resistors | R1 ≈ 8.4 kΩ, R2 ≈ 0.3 kΩ |
| Freewheel diode | integrated, Vf 1.8 V at 3 A |

The report's reasoning for choosing a Darlington — that it multiplies current gain and can
therefore switch a solenoid a microcontroller cannot drive directly — is correct as far as it
goes. What it misses is that the same stacked structure that gives the gain also stacks two
base-emitter drops, which is fatal on a 3.3 V rail. See the README for the full calculation.

### The LED + photoresistor coupler

```
ESP32 GPIO ──► LED ──┐
                     │   optical coupling, taped over
                     ▼
 separate V+ ──[ LDR ]── B  BDX53C  C ── solenoid ── separate V+
                            E
                            └── GND of the separate supply
```

The GPIO drives only the LED (~10 mA). Base current for the Darlington comes from the lock's
own 18 V supply through the illuminated LDR, which removes the 3.3 V headroom problem entirely:

```
I_base ≈ (18 V − 1.6 V) / ~2 kΩ ≈ 8 mA
required β = 3.4 A / 8 mA ≈ 425      vs   750 guaranteed
```

That is the only configuration in this project in which the Darlington could actually
saturate. The optical coupling was the mechanism; **the 18 V rail was the fix.**

Running an 18 V pack into a solenoid characterised at 9 V also means roughly double the
current and four times the power in the coil — on the order of 60 W while energised. It
survived because it was only ever energised for about a second at a time. Held on
continuously, the coil would have been the first thing to fail.

No photograph of this circuit survives among the project files.

---

## 10. PCB

Designed in EasyEDA, after migrating from KiCad.

### Revision 1 — SMD

Fully routed with SMD passives, two ESP32 modules, a DIP-package IC, tactile switches, screw
terminal and two buzzers. **Never manufactured** — quoted at over $120.

Renders survive in the original report.

### Revision 2 — through-hole (the one that was built)

- Two layers, ground pour on **both** faces, explicitly for EMI reasons.
- Both briefcase ESP32s on a single board (`U2`, `U3`).
- Screw terminal for the lock, on-board buzzers, headers `H2`, `H4`, `H5`, `H6` for the
  keypad, OLED, fingerprint sensor and LoRa module.
- Q1: TO-220 footprint for the Darlington, R2 / R3 through-hole resistors, C1 / C3 filtering.
- Silkscreen: `5803346A_Y2_240518`, *"Made by Minguz in Italy"*.
- Back-side easter egg: a stick figure pointing at an insect, captioned `LOOK, A BUG!`.
- **$2 for the boards, $15 shipping.**

Known layout complaints, from the original report:

- The USB connector sits too close to the first buzzer, making it awkward to plug a cable in
  to reprogram the board.
- The heatsink arrangement for the LM7805 is functional but crude; the report suggests a fan,
  which is the wrong fix — a switching regulator removes the heat instead of moving it.

Source files: the EasyEDA project is **[unknown]**, but the **Gerbers are recoverable from
the JLCPCB order history**, which is enough to make the board reproducible.

---

## 11. Antennas

Quarter-wave whip antennas on SMA connectors. The report's calculation is correct:

```
λ  = c / f = 3×10⁸ / 433×10⁶ ≈ 0.693 m
λ/4 ≈ 0.173 m = 173 mm
```

On the transmitter, the antenna passes through the wall of the tin box and stands outside it.

On the briefcase, the antenna was likewise mounted **outside** the aluminium shell. Enclosure
shielding is therefore not an explanation for the link behaviour — which is worth stating,
because it is the first thing anyone reasonable would suspect.

A further point the original report does not address: a quarter-wave monopole needs a ground
plane to work against. Mounted on a dev board with a few centimetres of copper, its real
efficiency is well below the textbook figure.
