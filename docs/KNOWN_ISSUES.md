# Known Issues and Limitations

Every entry is classified as one of:

- **Confirmed issue** — demonstrable from the code, the datasheets or the surviving evidence.
- **Suspected issue** — a well-reasoned hypothesis that has not been measured.
- **Historical workaround** — a deliberate hack used to get the project working at the time.
- **Needs further investigation** — cannot be settled from the surviving material.

---

## Firmware and protocol

### C-1 · No framing on the radio link — Confirmed issue

```cpp
if (Serial2.available()) {                          // one byte is enough
  Transceiver.GetStruct(&MyData, sizeof(MyData));   // 24 are demanded
```

`available() >= 1` does not mean a full packet has arrived. `readBytes` blocks up to the
Stream's 1000 ms timeout and returns a short count, which is discarded. The struct is left
half-updated and the byte stream stays **permanently misaligned** — every following read
starts mid-packet until a long enough silence resynchronises it by accident.

This is the most likely single cause of the loss floor visible in the original range tests
(see C-10).

**Fix:** a byte-oriented reader with a start delimiter, a length field and a CRC.

---

### C-2 · `String` inside a memcpy'd struct — Confirmed issue

```cpp
struct DATA { bool aperto; String passwordTX; bool connesso; };
Transceiver.SendStruct(&MyData, sizeof(MyData));
```

An Arduino `String` is an object that, above a certain length, holds a pointer into the heap.
Transmitting it by raw byte copy transmits that pointer.

It worked only because arduino-esp32's `String` uses small-string optimisation, storing up to
**11 characters inline** (`SSOSIZE = sizeof(struct _ptr) + 4 - 1 = 15`). A 5-digit code fits.
A 12-character one would not, and the receiver would dereference a pointer into the sender's
address space.

**This is the most instructive bug in the project.** It is silent, it is version-dependent,
and it would have failed the moment anyone lengthened the code.

**Fix:** `char code[8]` in a `__attribute__((packed))` struct, or explicit serialisation.

---

### C-3 · No acknowledgement, retry, sequence number or CRC — Confirmed issue

Neither the EBYTE library nor the sketches implement any of these. The only reliability
mechanism is a 5-second beacon plus a 7-second timeout.

Concrete consequence: if the "briefcase opened" packet is lost, the transmitter never
generates a new code, the briefcase keeps the old one, and **the beacon keeps arriving with
the stale code so the link looks perfectly healthy**. The two units silently disagree about
the state of the system, and the "one-time" code is no longer one-time.

The original report states that an acknowledgement system was implemented. No such mechanism
exists in the surviving firmware. Either it was in an earlier revision, or "acknowledgement"
described the beacon.

---

### C-4 · Return values never checked — Confirmed issue

`SendStruct()` and `GetStruct()` both return a boolean indicating whether the expected number
of bytes moved. Neither is ever read.

---

### C-5 · Hard-coded initial password — Confirmed issue

```cpp
String password = "1234";
```

Until the first radio packet arrives, the briefcase opens with `1234`.

---

### C-6 · The unlock code is transmitted in clear — Confirmed issue

The E32 modules run in **transparent mode**, address `0x0000`, channel `23` — every one a
factory default. Anyone within range with an unconfigured E32 receives the code.

**Fix:** challenge–response with a pre-shared key; never transmit the secret itself.

---

### C-7 · No attempt limiting — Confirmed issue

No counter, no delay, no lockout, no logging. 10⁵ codes at roughly one attempt per second is
about 28 hours worst case.

---

### C-8 · Blocking `delay()` in critical paths — Confirmed issue

`delay(500)` on a wrong code and `delay(1000)` in the fingerprint branch. During those windows
the keypad, the radio and the display are all ignored — and, given C-1, bytes arriving during
a `delay()` are exactly the ones that desynchronise the stream.

The codebase already uses `millis()` timers everywhere else, so these are inconsistencies
rather than deliberate design.

---

### C-9 · The `connesso` field is redundant — Confirmed issue

It always arrives `true` and is cleared locally on the next pass. Link liveness is decided
entirely by the 7-second timeout on `Last`. Harmless, but it makes the state machine harder to
read than it needs to be.

---

## Radio

### C-10 · The range claim rests on one unrepeated measurement — Confirmed issue

The report states a range of "approximately one kilometre" and supports it with this table:

| Distance | "Quality" |
|---|---|
| 50 m | 95 % |
| 100 m | 79 % |
| 400 m | 70 % |
| 1000 m | 62 % |

**The author does not stand behind these numbers.** They are a single unrepeated walk through
town — one run, one day, one route, roughly line of sight, counting successes out of at least
ten received beacons — presented as if it characterised the system. No variable was controlled
and the experiment was never repeated.

The table is retained here because deleting inconvenient data is the habit this repository
exists to correct. But it should be read as an anecdote, not a measurement.

The author's actual repeated experience: **the link was reliable and then, at roughly 400 m,
dropped and stayed dropped.**

**And range was never the binding constraint:**

```
FSPL(433 MHz, 1 km) ≈ 85.2 dB
P_rx ≈ 30 dBm + ~2 dBi + ~2 dBi − 85.2 dB ≈ −51 dBm
sensitivity at 2.4 kbps ≈ −140 dBm (estimated; the −147 dBm datasheet
                                    figure is measured at 0.3 kbps)
→ ~89 dB of margin
```

Even a two-ray ground-reflection model, where path loss grows as d⁴ beyond a breakpoint of a
few metres, still leaves tens of dB of margin at 400 m. With both antennas mounted outside
their metal enclosures, **a hard limit at 400 m is not something propagation explains.**

---

### C-11 · The report's stated frequency is wrong — Confirmed issue

The report says channel 17 → 433 + 17 = 450 MHz. Both halves are wrong:

- The E32 channel formula is **410 MHz + CHAN × 1 MHz**.
- The channel was not 17. The configuration screenshot shows `Param Now: 0x00, 0x00, 0x1a, 0x17, 0x44`, and `0x17` **is 23 decimal** — the module's factory default.

Real operating frequency: **410 + 23 = 433.0 MHz**, exactly what the same screenshot reports
as `Freq Now: 433.0MHz`. The error came from reading a hexadecimal value as decimal.

---

### C-12 · Operating on the most congested channel in the band — Confirmed issue

433.0 MHz exactly. The EBYTE manual explicitly advises avoiding crowded integer frequencies
such as 433.0 MHz. In a built-up area this channel is shared with gate remotes, weather
stations, car keys and sensors.

---

### C-13 · The −148 dBm sensitivity figure was quoted out of context — Confirmed issue

That value is measured at **0.3 kbps, SF12, CR 4/5**. The project ran at **2.4 kbps**, where
sensitivity is meaningfully worse. The datasheet states plainly that "the receiving sensitivity
will be reduced ... while increasing the air data rate".

---

### C-14 · 1 W at 433 MHz is not legal for licence-free use in Europe — Confirmed issue

The firmware leaves the modules at 30 dBm. The 433.05–434.79 MHz band is limited to **10 mW
ERP** for licence-free short-range devices across most of Europe — 20 dB below what this
project transmitted.

Anyone rebuilding this must reconfigure the modules to a legal power level for their
jurisdiction, or move to a band they are allowed to use.

---

### S-1 · The link dropped permanently rather than degrading — Suspected issue

The single most useful clue about the range behaviour is its *shape*, not its distance.

- **A range limit is soft and reversible.** Near the edge you get intermittent losses; step
  back towards the transmitter and it recovers. You can walk it back and forth.
- **What was actually observed is a cliff that stays.** It worked, it dropped, it did not come
  back.

The second shape is the signature of **C-1**, not of a radio limit. Because `GetStruct` reads a
fixed 24 bytes whenever a single byte is available, the first corrupted or truncated packet
leaves the byte stream permanently misaligned. The first packet lost at the edge of usable
range therefore does not cost one beacon — **it costs the link**, until a long enough silence
happens to resynchronise it.

Distance was the trigger. The receive loop is why it stayed down.

**To settle it:** instrument the receiver to log short reads and log the return value of
`GetStruct`. If the drop coincides with a short read, this is confirmed.

---

### S-2 · The radio was probably never transmitting anywhere near 1 W — Suspected issue

The E32-433T30D draws **570–670 mA** while transmitting. On the code transmitter that came
from a 9 V zinc-carbon 6F22 through an LM7805 — a cell with an internal resistance of several
ohms. The EBYTE datasheet states plainly that "the transmitting power will be lowered by
lowering the power supply voltage."

This is worth quantifying, because in free space range scales with the square root of power:

```
−20 dB of transmit power  →  10× less range
```

If supply sag left the effective output at 10–14 dBm instead of 30, the usable distance
collapses by roughly an order of magnitude — enough to turn a link that should have reached
kilometres into one that gave up in the hundreds of metres. It is also consistent with the
report's separate complaint of disconnections "for no apparent reason".

Compounding it: a quarter-wave monopole needs a ground plane to work against. Mounted on a dev
board with a few centimetres of copper, real efficiency is well below the textbook figure.

**To settle it:** a scope on the 5 V rail during a transmit burst; ideally, an RF power
measurement with the battery fitted versus a bench supply.

**Not a factor:** enclosure shielding. Both antennas were mounted outside their metal boxes on
SMA connectors.

---

## Hardware

### C-15 · The LM7805 cannot supply the lock — Confirmed issue

The solenoid draws **1.7 A at 9 V** (bench-supply measurement). An LM7805 in TO-220 is rated
**1.5 A** with adequate heatsinking, and 9 → 5 V at 1.5 A is **6 W** to dissipate. The small
heatsink fitted would have gone into thermal shutdown.

---

### C-16 · The 9 V battery cannot supply the system — Confirmed issue

The cell in the surviving photographs is a **VARTA Super Heavy Duty**, i.e. **zinc-carbon**.
Three separate problems:

1. **Capacity.** A 9 V alkaline gives ~550 mAh at a few milliamps; usable capacity collapses
   as current rises. Zinc-carbon is far worse.
2. **Internal resistance.** Six tiny cells in series, several ohms of internal resistance. The
   610 mA transmit burst is a **power** limit, not an energy one.
3. **Regulator losses.** At 300 mA the LM7805 burns 1.2 W out of ~2.7 W drawn — over 44 % of
   the energy becomes heat before it powers anything.

Order-of-magnitude runtime: ~1.7 h ideal, well under an hour in practice.

---

### C-17 · Insufficient base drive for the Darlington — Confirmed issue

```
I_base(total)  ≈ (3.3 V − 1.6 V) / 1 kΩ            ≈ 1.7 mA
internal divider loss   1.6 V / 8.7 kΩ             ≈ 0.18 mA
I_base(useful)                                     ≈ 1.5 mA

required β = 1.7 A / 1.5 mA ≈ 1130    vs    750 guaranteed (BDX53C, Vce 3 V, Ic 3 A)
```

The transistor could not saturate. It sat in the linear region, dropped several volts and
dissipated watts.

**And no resistor value fixes it.** A Darlington has two stacked base-emitter junctions; the
datasheet allows Vbe(sat) up to **2.5 V** at high current. Out of a 3.3 V rail that leaves
under a volt of headroom. **A Darlington is the wrong device for a 3.3 V GPIO**, independently
of everything else.

Two independent causes — C-15/C-16 and C-17 — each sufficient on its own.

---

### C-18 · GPIO12 used as a keypad column — Confirmed issue

GPIO12 is the MTDI strapping pin. Held high at reset, the ESP32 configures the flash for 1.8 V
and fails to boot. The `Keypad` library holds columns `OUTPUT LOW` while scanning, but at the
instant of reset the pin is still an input. A key held down in column 2 at power-up can
prevent boot.

No failure attributable to this is recorded, but the hazard is real.

---

### C-19 · GPIO19 drives both the lock and an inter-MCU signal — Confirmed issue

The same pin energises the lock driver and tells the second ESP32 to play the success tune.
It works, but it couples an actuator to a UI signal: neither can be changed independently.

---

### C-20 · GPIO34 floats while board B is in reset — Confirmed issue

GPIO 34–39 are input-only with **no internal pull-up or pull-down**. The fingerprint-match
line is undefined while board B is unpowered or resetting. A 100 kΩ external pulldown would
have made this deterministic.

---

### C-21 · The second ESP32 was avoidable — Confirmed issue

The ESP32 has three hardware UARTs. UART0 is the USB bridge; UART2 carried the LoRa module;
UART1 *appears* unusable because its default pins (GPIO 9/10) are wired to the WROOM module's
internal SPI flash. AVR-style `SoftwareSerial` does not work on ESP32 either, removing the
obvious escape hatch.

But **ESP32 UART pins are remappable through the GPIO matrix**:

```cpp
Serial1.begin(57600, SERIAL_8N1, /*RX=*/26, /*TX=*/27);
```

One line would have removed an entire microcontroller, its board area, its cost and the three
GPIO signalling lines between the two.

---

### C-22 · Report/firmware mismatch: Node-RED and battery monitoring — Confirmed issue

Section 8 of the original report describes a Node-RED dashboard over MQTT, with topics for
start/stop, code entry, battery percentage and connection status. The report's own flowchart
also describes a 3-minute timer and a low-battery warning below 20 %.

**None of it is present.** There is no networking code of any kind in any surviving firmware,
and the transmitter's timer is 5 seconds with no battery calculation.

---

### C-23 · A stale comment references hardware that is no longer in the system — Confirmed issue

`valigia_definitivo.ino` refers to an "arduino micro" reading fingerprints. Board B is an
ESP32; the Micro was only used for early standalone testing of the sensor.

---

## Historical workarounds

### H-1 · LED + photoresistor optocoupler

> *Prototype workaround used to isolate the ESP32 control signal from a separately powered
> lock-driving circuit.*

The ESP32 drives an LED; the LED illuminates an LDR; the LDR sits in the Darlington's base
path fed from the lock's **separate 18 V supply** (H-4). This removes the 3.3 V rail from the
base circuit entirely, which is why it worked — it does not "boost" anything, it sidesteps
C-17:

```
I_base ≈ (18 V − 1.6 V) / ~2 kΩ (illuminated LDR) ≈ 8 mA
required β = 3.4 A / 8 mA ≈ 425      vs   750 guaranteed
```

That is the only configuration in this project where the Darlington could actually saturate.
**The optical coupling was the mechanism; the 18 V rail was the fix.** Worth being precise
about, because for two years the LED and the LDR got the credit.

Why it is not a design:

- Response times of tens to hundreds of milliseconds, and **asymmetric** (rise much slower
  than fall).
- Resistance drifts with temperature and recent light exposure (light memory effect).
- The transfer ratio is specified nowhere: it can be tuned, not designed.
- **Ambient light can partially turn the lock on** if the shielding is imperfect. On a lock
  that is a security problem, not merely a reliability one.
- No rated isolation voltage, so it is not actually isolation.
- Many LDRs contain cadmium sulphide and are RoHS-restricted.

A PC817 or TLP291 costs about €0.20, switches in microseconds, and has a specified CTR.

---

### H-2 · The 0 Ω base resistor

Fitted when 1 kΩ did not work. It violates the ESP32's 40 mA absolute maximum per pin, drags
the GPIO output voltage down, and leaves the underlying headroom problem untouched.

---

### H-4 · An 18 V cordless-drill battery as the lock supply

In the final working configuration the lock circuit was fed from an **18 V cordless-drill
battery pack**, wired in as a stopgap and never replaced.

It was the right instinct — a 5-cell Li-ion pack has milliohms of internal resistance, which is
everything a 6F22 is not — applied without doing the arithmetic:

- **Roughly double the solenoid's working voltage.** At 9 V the coil drew 1.7 A; at 18 V, for a
  largely resistive coil, that is on the order of 3.4 A and **four times the power** — around
  60 W dissipated in the coil while energised. It survived only because it was energised for
  about a second at a time, a handful of times. Held on continuously the coil would have been
  the first thing to fail.
- **The Darlington was over-dissipating too**, at something over 5 W in an unheatsinked TO-220.
- **But it is also the reason the workaround worked** — see H-1.

---

### H-3 · The physical key as a second factor

The key switch simply supplies power to the fingerprint sensor; the firmware knows nothing
about it. This started as a wiring convenience and ended up being, arguably, the most
genuinely secure element in the design.

Not really a *problem* — but it is a workaround in the sense that the security property is
mechanical and accidental rather than designed.

---

## Needs further investigation

### N-1 · The fingerprint sensor appearing "bound" to one ESP32

The recollection is that after being used with one ESP32, the R557 seemed not to work with
another board.

**This repository does not claim the sensor binds to a controller, because it almost certainly
does not.** The R557 stores templates in its own flash, so enrolment is portable by
construction.

What the module *does* keep in flash, any of which would produce exactly this symptom:

- the **UART baud rate** (`SetSysPara`),
- the **4-byte handshake password** (`VfyPwd`),
- the **4-byte device address**.

Change any of them during experiments and a second host using library defaults fails to
handshake and appears dead.

Compounding factor: the report records that the sensor was first tested with an **Arduino
Micro**, which drives 5 V logic. The R557's communication lines are specified for **3.6 V
maximum**. Sustained over-driving is a plausible way to degrade an input.

**To settle it:** connect the sensor to a known-good ESP32 and sweep the baud rate while
attempting `verifyPassword()`; then try the default password and address explicitly.

---

### N-2 · Actual current consumption

The figures of ~300 mA (briefcase) and ~150 mA (transmitter) are recollections, not
measurements. They are plausible — two ESP32-WROOMs with the radio off sit at 40–70 mA each —
but nothing in the surviving material measures them.

**This can no longer be settled.** The hardware was dismantled in September 2026, so the
figures stay recollections permanently. It is the main reason this repository leans on
datasheet arithmetic rather than measurement — and a good argument for measuring things while
you still have the board in front of you.

---

### N-3 · The lock's nominal ratings

Only one data point survives: **1.7 A at 9 V** on a bench supply. The model number, nominal
voltage, duty-cycle rating and coil resistance are all unknown. A solenoid lock rated for
intermittent duty that is held on continuously will overheat.

---

### N-4 · The solenoid's actual ratings

Two data points survive: **1.7 A at 9 V** on a bench supply, and the fact that it was run from
18 V in the final build (H-4). The model number, nominal voltage, duty-cycle rating and coil
resistance are all unknown, and the hardware no longer exists to measure.

The 3.4 A figure used throughout this documentation assumes a largely resistive coil and
should be read as an order of magnitude, not a measurement.

---

### N-5 · The enrolment sketch

Not among the surviving files. `impronte_definitivo.ino` can match a finger but cannot register
one — it only calls `getImage()`, `image2Tz()` and `fingerSearch()`.

The best available evidence is a string it still prints at startup:
`"Adafruit Fingerprint sensor enrollment"`. That is a leftover from Adafruit's `enroll`
example, which is what the file was started from, and strongly suggests the templates were
registered by running that stock example once from a separate sketch that no longer survives.


---

## Resolved since the original documentation pass

Kept here so the reasoning trail is visible rather than silently edited away.

| Question | Answer |
|---|---|
| Was the briefcase antenna inside the aluminium case? | **No** — mounted outside, on an SMA connector, like the transmitter's. Enclosure shielding is not an explanation for the link behaviour |
| What powered the lock circuit? | An **18 V cordless-drill battery pack** — see H-4 |
| Did the firmware read the key switch? | **No.** Verified in both sketches: no key pin is declared or read. The switch is in series with the sensor's supply |
| Was the Node-RED dashboard real? | **No.** See C-22 |
| Do the PCB source files survive? | The EasyEDA project does not; the **Gerbers are recoverable from the JLCPCB order history** |
