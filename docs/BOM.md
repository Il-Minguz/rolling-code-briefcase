# Bill of Materials

Reconstructed from the surviving sketches, the 2024 school report and photographs of the
assembled units. Quantities are for the complete system (briefcase + code transmitter).

Entries marked **[unknown]** could not be established from the surviving material and are
deliberately not guessed.

---

## Briefcase unit

| Qty | Part | Notes |
|---:|---|---|
| 2 | ESP32-WROOM-32 dev board | `U2` and `U3` on the custom PCB |
| 1 | EBYTE **E32-433T30D** | SX1278, 1 W, UART, SMA connector |
| 1 | 433 MHz quarter-wave SMA antenna | ~173 mm |
| 1 | **GROW R557** fingerprint module | Capacitive, 3.3 V UART, RGB ring, ~120 templates |
| 1 | OLED **SH1106** 128×64 I²C, address `0x3C` | |
| 1 | 4×3 matrix keypad | `1-9`, `*`, `0`, `#` |
| 1 | Barrel key switch + key | Powers the fingerprint sensor |
| 1 | Solenoid lock | **[unknown]** model; **1.7 A at 9 V** measured — but run from 18 V in the final build |
| 1 | **BDX53C** NPN Darlington, TO-220 | 100 V, 8 A, internal base resistors + freewheel diode |
| 1 | Base resistor 1 kΩ | Later replaced with 0 Ω — see KNOWN_ISSUES H-2 |
| 1 | LED | Part of the LED/LDR workaround |
| 1 | Photoresistor (LDR) | **[unknown]** model |
| 2 | Piezo buzzer | One on the PCB, one for melodies |
| 1 | **LM7805CT** regulator, TO-220 | With external heatsink and thermal paste |
| 2 | 10 µF electrolytic capacitor | Regulator input and output |
| 2 | 100 nF ceramic capacitor | Regulator input and output |
| 1 | Panel voltmeter module, 7-segment | Battery monitoring |
| 1 | 9 V 6F22 battery + clip | Zinc-carbon in the surviving photographs |
| 1 | Custom PCB rev. 2 (through-hole) | Two layers, `5803346A_Y2_240518` |
| 1 | 2-pin screw terminal | Lock output |
| — | Pin headers `H2`, `H4`, `H5`, `H6` | Keypad, OLED, fingerprint, LoRa |
| 1 | Aluminium attaché case | |
| 1 | **18 V cordless-drill battery pack** | Separate supply for the lock circuit; a stopgap that became permanent — see KNOWN_ISSUES H-4 |

---

## Code transmitter unit

| Qty | Part | Notes |
|---:|---|---|
| 1 | ESP32-WROOM-32 dev board | On a mini breadboard, never migrated to a PCB |
| 1 | EBYTE **E32-433T30D** | Marked `…08-V1.1` in the photographs |
| 1 | 433 MHz quarter-wave SMA antenna | Mounted through the wall of the box |
| 1 | LCD **1602** + PCF8574 I²C backpack (QAPASS), address `0x27` | |
| 1 | Red indicator LED | Panel-mounted in the lid |
| 1 | **LM7805CT** regulator + heatsink | On a small piece of perfboard |
| 2 | 10 µF electrolytic capacitor | |
| 2 | 100 nF ceramic capacitor | |
| 1 | 9 V 6F22 battery + clip | **VARTA Super Heavy Duty** — zinc-carbon |
| 1 | Rocker / toggle switch | Power |
| 1 | Trimmer potentiometer | LCD contrast, most likely |
| 2 | Resistors for the battery divider | **[unknown]** values |
| 1 | Relay module, `6A 250VAC / 10A 12VDC` | **[unknown]** purpose — not driven by any surviving firmware |
| 1 | Mini breadboard | |
| 1 | Hinged metal tin | |

---

## Firmware dependencies

| Library | Author | Used for |
|---|---|---|
| [`EBYTE`](https://github.com/KrisKasprzak/EBYTE) | Kris Kasprzak | E32 LoRa modules |
| `Adafruit_Fingerprint` | Adafruit | R557 sensor |
| `Adafruit_GFX` | Adafruit | Graphics primitives |
| `Adafruit_SH110X` | Adafruit | SH1106 OLED |
| `Keypad` | Mark Stanley, Alexander Brevig | Matrix keypad |
| `ezBuzzer` | ArduinoGetStarted | Non-blocking buzzer on the main controller |
| `LiquidCrystal_I2C` | — | LCD 1602 on the transmitter |
| `Wire`, `SPI`, `HardwareSerial` | Arduino core | |

Core: arduino-esp32. **[unknown]** exact version — see the small-string-optimisation note in
[`SOFTWARE.md`](SOFTWARE.md), where it matters.

---

## Tooling

| Item | Notes |
|---|---|
| EasyEDA | PCB design; migrated from KiCad during the project |
| JLCPCB | Board manufacture — **$2 for the boards, $15 shipping** for rev. 2 |
| EBYTE RF Setting tool + FTDI232 adapter | One-time configuration of the E32 modules |
| Arduino IDE | Firmware |

The rev. 1 SMD design was quoted at over **$120** and was never manufactured.

---

## Cost note

The through-hole decision was driven entirely by budget: $17 delivered against $120+ for the
SMD version. For a student project that trade-off was obviously right, and it brought
benefits that were not the reason for the choice — easier soldering, easier rework, and a
board that survived two years of being handled before it was finally dismantled.

---

## Sourcing notes for anyone rebuilding

Read [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) and [`REDESIGN.md`](REDESIGN.md) before buying
anything. In particular:

- **Do not buy a 9 V 6F22 for this.** It cannot supply the currents involved. A single 18650
  with a protection board costs about the same and stores two to three times the energy at a
  fraction of the internal resistance.
- **Do not buy a BDX53C for a 3.3 V GPIO.** A logic-level N-channel MOSFET with Rds(on)
  specified at Vgs = 2.5–3.0 V is cheaper, cooler and actually works.
- **Check your local radio regulations before buying a 1 W module.** In Europe the
  433.05–434.79 MHz band is limited to 10 mW ERP for licence-free use. A 20 dBm E32 variant,
  or the same module configured down, is the appropriate choice.
- The **R557 is a 3.3 V device** — its communication lines are rated for 3.6 V maximum. Do not
  drive it from 5 V logic.
