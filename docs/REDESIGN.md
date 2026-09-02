# What I Would Do Differently Today

This is the part of the repository I find most useful. Nothing here is a criticism of the
2024 build for its own sake — every item is something the project taught me by failing.

---

## Summary table

| Original design | Problem | How I would redesign it today |
|---|---|---|
| 9 V zinc-carbon battery | Cannot source 300 mA continuous or 610 mA bursts; capacity collapses under load | 1S Li-ion 18650 with protection (9–13 Wh), plus bulk capacitance at the radio |
| LM7805 linear regulator | 44 % of energy wasted as heat; 1.5 A ceiling; thermal shutdown | Synchronous buck to 5 V, separate LDO for 3.3 V |
| One supply for logic and lock | 1.7 A transients on the logic rail | Separate lock supply, its own switch, single-point common ground |
| BDX53C Darlington on a 3.3 V GPIO | Two stacked Vbe drops; β of 1130 needed against 750 guaranteed | Logic-level N-channel MOSFET with Rds(on) specified at Vgs = 2.5–3.0 V |
| LED + photoresistor coupler | Slow, unspecified, ambient-light sensitive, not real isolation | PC817 / TLP291 if isolation is wanted; direct MOSFET drive if not |
| 18 V drill pack into a 9 V solenoid | ~3.4 A and ~4× the rated coil power | Buck the pack down to the solenoid's rated voltage |
| Freewheel diode only inside the Darlington | Absent once you switch to a MOSFET | Explicit Schottky across the solenoid |
| Raw C struct containing an Arduino `String` | Undefined behaviour masked by small-string optimisation | Packed byte protocol: magic, version, type, sequence, length, payload, CRC-16 |
| No ACK, retry or timeout | A single lost packet silently desynchronises system state | ACK + sequence numbers + bounded retries + explicit link state machine |
| `if (available()) readBytes(24)` | Short reads desynchronise the stream permanently | Byte-oriented parser with a start delimiter; resync on CRC failure |
| Rolling code sent in clear | Readable by any default-configured E32 in range | Challenge–response with a pre-shared key; never send the secret |
| No attempt limiting | 10⁵ codes brute-forceable overnight | Exponential backoff, hard lockout, attempt logging |
| Always-on ESP32s | Hundreds of mA for a device used for seconds a day | Deep sleep with `ext1` wake on a keypad row; load switches for radio and sensor |
| A second ESP32 for one UART | Board area, cost, three signalling lines | One ESP32 with `Serial1` remapped |
| GPIO12 as a keypad column | Strapping pin; boot hazard | Any non-strapping GPIO (32, 33, 27, 26…) |
| GPIO19 shared between lock and UI signal | Actuator coupled to a UI event | Separate pins — or one MCU and no signalling at all |
| 1 W on 433.0 MHz | Illegal licence-free in the EU; busiest channel in the band | ≤ 10 mW ERP on a quieter channel, or 868 MHz with duty-cycle discipline |
| Estimated power figures | Never measured | INA219/INA226, design from a measured average |

---

## 1. Driving the lock

### Why the Darlington was the wrong device — and what "logic-level MOSFET" actually means

Saying "use a logic-level MOSFET" is not enough advice to act on, so here is the reasoning in
full, for switching **1.7 A from a 3.3 V GPIO**.

**Vgs(th) is not the turn-on voltage.** It is defined as the gate voltage at which the device
conducts roughly 250 µA — essentially "barely not off". A part with Vgs(th) = 2 V is *not*
guaranteed to be usefully on at 3.3 V. Choosing a MOSFET by its threshold voltage is the
single most common mistake in this area, and it is exactly the reasoning that made me avoid
MOSFETs in 2024.

**The number that matters is Rds(on) specified at Vgs = 2.5 V or 3.0 V.** If a datasheet only
quotes Rds(on) at Vgs = 10 V or 4.5 V, that part is not intended for 3.3 V gate drive,
whatever its threshold says.

**Design to the dissipation, not to the current rating.**

```
At 1.7 A with Rds(on) = 50 mΩ @ Vgs = 3.0 V:
  P = I²R = 1.7² × 0.05 ≈ 0.14 W        → no heatsink needed

Compare the Darlington, if it had saturated:
  P = I × Vce(sat) = 1.7 × ~1.5 V ≈ 2.5 W → heatsink mandatory
  and it did not saturate, so the real figure was worse.
```

A 20× reduction in dissipation, from one part substitution.

**Gate drive.** A solenoid does not care about switching speed, so gate charge is not a
constraint here. Two components make it robust:

- **100 Ω in series with the gate** — limits the inrush the ESP32 pin sees while charging the
  gate capacitance, keeping it under the 40 mA absolute maximum.
- **100 kΩ from gate to source** — holds the MOSFET off while the GPIO is high-impedance,
  which it is during reset, boot and deep sleep. Without this, the lock's state during a reset
  is undefined. On a lock, that matters.

**You still need a flyback diode.** The BDX53C had one built in; a MOSFET's body diode faces
the wrong way for a low-side switch. Add an external Schottky across the solenoid, rated for
at least twice the supply voltage and twice the current — for 9 V at 1.7 A, something like a
40 V / 3 A part. Without it, the inductive kick when the coil is de-energised will punch
through the MOSFET.

**Part selection.** Devices commonly used for 3.3 V gate drive at this current include
AO3400-class SOT-23 parts and IRLB8721-class TO-220 parts. **Check the Rds(on)-at-2.5 V column
on the datasheet of the part you actually buy**, not on a forum recommendation — the same
marking from different manufacturers can behave differently at low Vgs.

### Could a different part just drop into the same footprint?

Yes — and this is the most useful practical question about the whole board, because it does not
require a redesign.

**The pinouts map one-to-one.** A TO-220 NPN Darlington is Base–Collector–Emitter on pins
1–2–3. A TO-220 N-channel MOSFET is Gate–Drain–Source on pins 1–2–3. The functions correspond
exactly: base → gate, collector → drain, emitter → source. Even the metal tab agrees — it is
the collector on the BJT and the drain on the MOSFET, which are the same node in both circuits.

**So a logic-level N-channel MOSFET solders straight into the `Q1` pads with no board
modification at all.**

Two things to know before doing it:

**1. Leave the 1 kΩ where it is. Do not bridge it.** For a BJT that resistor sets base current;
for a MOSFET it only limits the current spike that charges the gate capacitance. With a typical
Ciss of 1–2 nF, 1 kΩ gives a switching time constant of a couple of microseconds — completely
irrelevant for a solenoid, and it protects the ESP32 pin. The 0 Ω bridge was the right instinct
for the wrong device: it makes a BJT slightly less starved and a MOSFET slightly less safe.

**2. Two things must be added, and neither needs a new board.**

- **A gate-to-source pulldown, ~100 kΩ.** The original board has nothing holding the gate low.
  A MOSFET gate is an open circuit, so while the ESP32 is in reset, booting, or asleep, the pin
  is high-impedance and the gate floats — the lock's state becomes undefined. Solder the
  resistor directly across pins 1 and 3 of the device.
- **A flyback diode across the solenoid.** This one is not optional. The BDX53C had one built
  in, which is why the original circuit got away without it. A MOSFET's body diode faces the
  wrong way for a low-side switch, so the inductive kick when the coil de-energises has nowhere
  to go and will punch straight through the device. The existing screw terminal is the obvious
  place to fit it.

**3. And fix the supply voltage while you are in there.** At 18 V a 30 V-rated MOSFET is too
close to its limit with an inductive load, and 30 V is where most parts with Rds(on) actually
specified at Vgs = 2.5 V live. More importantly, the solenoid should not be seeing 18 V at all.
Buck the pack down to the solenoid's rated voltage and the whole problem gets easier: lower
current, a quarter of the coil dissipation, and a comfortable choice of MOSFET.

If you keep 18 V, a 3.3 V GPIO can still drive it — a low-side switch has its source at ground,
so the supply rail does not affect gate drive at all — but you will want either a 60 V part
whose datasheet genuinely quotes Rds(on) at 3 V, or a small gate driver / two-transistor level
shift so the gate can be driven to 10 V.

### Or: keep the Darlington and fix only the base circuit

There is a smaller fix that keeps every original part, and it is worth stating because it is
what the LED-and-LDR hack was groping towards.

Replace the LDR with a real optocoupler (PC817, TLP291), fed from the lock's own supply. The
optocoupler's output transistor sits where the LDR was, and the base resistor now has the lock
rail across it instead of 3.3 V:

```
I_base = (18 V − 1.6 V) / 1 kΩ ≈ 16 mA
required β = 3.4 A / 16 mA ≈ 210      vs   750 guaranteed
```

**Which means the 1 kΩ resistor was never the wrong value.** It was the right value with the
wrong voltage across it. Finding it "empirically" gave a number that would have worked
perfectly well from an 18 V rail and could never have worked from a 3.3 V pin — which is a
tidy illustration of why empirical component selection without a model tells you *that*
something works, but not *when it will stop*.

### If isolation is genuinely wanted

The LED-and-LDR hack was, in effect, an attempt at isolation. If that is the goal, do it
properly:

- **PC817** — cheap, ubiquitous, CTR 50–600 % (wide, so design for the worst case).
- **TLP291** — better CTR specification, easier to design around.
- Either switches in microseconds instead of hundreds of milliseconds, has a rated isolation
  voltage, and is immune to ambient light.

But for a low-side solenoid switch sharing a ground with the MCU, **isolation is not needed at
all** — a MOSFET does the job. The reason isolation seemed necessary in 2024 was the base-drive
problem, and that problem disappears with the right device.

---

## 2. Power

### Why a rectangular 9 V battery is the wrong choice for hundreds of milliamps

Three distinct problems that are easy to conflate:

**1. Capacity.** A 9 V alkaline is rated around 550 mAh, but that rating assumes a drain of a
few milliamps. Usable capacity falls sharply as current rises, and for a 6F22
that fall is steep. Zinc-carbon, which is what the photographs show, is far worse again.

**2. Internal resistance — the one that actually killed this project.** A 6F22 is six tiny
cells stacked in series inside a small can. The electrode area is minute and the internal
resistance is on the order of **several ohms**. A 610 mA transmit burst therefore drops
volts *inside the battery*, before any of it reaches the regulator.

This is a **power** limit, not an energy one. No amount of "a bigger 9 V battery" fixes it,
because there is no such thing.

**3. The linear regulator throws away the difference.** At 300 mA the LM7805 dissipates
(9 − 5) × 0.3 = **1.2 W** out of about 2.7 W drawn from the battery. **Over 44 % of the stored
energy becomes heat before it powers anything at all.**

### What I would build instead

**Logic rail.** A single 18650 Li-ion cell: 3.7 V nominal, 2500–3500 mAh, i.e. **9–13 Wh**
against about 4.5 Wh nominal for a 9 V alkaline, at maybe a third of the cost per cycle and
with an internal resistance in the tens of milliohms rather than ohms.

- With a protection board (over-discharge, over-current, short circuit) — not optional on Li-ion.
- Boost to 5 V for the LoRa module, or run a 5 V/3.3 V rail pair from a small buck-boost.
- A **synchronous buck** (MP1584, TPS5430 class) at 85–92 % efficiency instead of the 7805's
  55 %. That difference alone roughly doubles the runtime.

**Radio rail.** A **220–470 µF low-ESR capacitor directly across the E32's supply pins.** The
610 mA burst lasts a few hundred milliseconds; local bulk capacitance turns a supply
*collapse* into a supply *sag*. This is the single cheapest fix in this entire document, and
it would likely have removed a good share of the "random disconnections".

**Lock rail.** Completely separate. The lock's 1.7 A transient has no business on the same
rail as an MCU and a radio.

- A dedicated pack, or a boost converter sized for the surge, with its own bulk capacitance.
- Common ground at a **single point**, so the lock's return current does not flow through the
  logic ground.
- Its own switch, so the lock can be de-energised for firmware work without powering the
  system down.

**Sleep.** This is a device that is genuinely in use for a few seconds a day and idle the rest
of the time. Hundreds of milliamps at idle is the wrong shape entirely.

- `esp_deep_sleep_start()` brings an ESP32 to a few tens of microamps.
- Wake on `ext1` from a keypad row — pull the rows up, tie the columns low while asleep, and
  any keypress wakes the chip.
- Power the fingerprint sensor and the LoRa module through **load switches** so they draw
  nothing while asleep. The key switch already does exactly this for the sensor, mechanically;
  the redesign would keep the key and add a MOSFET load switch for the radio.
- The transmitter has to stay awake to receive, but its E32 supports WOR (wake-on-radio), which
  the datasheet puts at around **30 µA average** — the parameters were already configured for
  250 ms WOR timing and never used.

**Measure it.** Every current figure in this project is a recollection. An INA219 or INA226 on
the battery lead, logging over a realistic day, would turn a design guess into a design input.
Battery sizing should start from a measured average, not an estimated peak.

---

## 3. The protocol

### An explicit packet format

```
┌────────┬─────────┬──────┬─────┬─────┬─────────┬────────┐
│ 0xA5   │ version │ type │ seq │ len │ payload │ CRC-16 │
│ 1 byte │ 1 byte  │ 1 B  │ 1 B │ 1 B │ 0-32 B  │ 2 B    │
└────────┴─────────┴──────┴─────┴─────┴─────────┴────────┘
```

- **`0xA5` start delimiter** — gives the parser something to resynchronise on. This alone fixes
  the permanent-desync failure.
- **Version** — lets two units with different firmware detect the mismatch instead of
  misinterpreting each other's bytes.
- **Type** — `BEACON`, `OPENED`, `NEW_CODE`, `ACK`. Explicit, instead of inferring intent from
  which boolean happens to be set.
- **Sequence number** — makes acknowledgement and duplicate detection possible.
- **Length** — the receiver knows how many bytes to expect before reading them.
- **CRC-16** — catches what the module's FEC does not.

Everything fixed-width and explicitly serialised. No `String`, no `sizeof(struct)` on the
wire, no dependence on compiler padding or on a core's string implementation.

### A byte-oriented reader

```cpp
// sketch of the idea, not production code
switch (state) {
  case WAIT_START: if (b == 0xA5) state = READ_HEADER;      break;
  case READ_HEADER: /* ... collect 4 bytes, then */ state = READ_PAYLOAD; break;
  case READ_PAYLOAD: if (--remaining == 0) state = READ_CRC; break;
  case READ_CRC: /* verify; on failure → WAIT_START */      break;
}
```

Never block. Never assume `available()` means a whole packet. On a CRC failure, drop back to
hunting for the start byte — which is what makes the link self-healing instead of permanently
broken.

### Acknowledgement and retry

The critical message is "the briefcase was opened", because losing it means the code is never
rotated and the system silently stops being a one-time-code system.

```
CASE → TX : OPENED   seq=N
TX   → CASE: ACK     seq=N
   (no ACK within 500 ms → retransmit, up to 3 times, backing off)
   (still nothing → surface it on the OLED)
```

With a 70 % per-packet success rate, one attempt plus two retries gives 1 − 0.3³ = **97.3 %**
per message. That is what would have turned an unreliable-feeling device into a reliable one —
without touching the radio at all.

### Link state, explicitly

Replace `nobuono` and the overloaded `connesso` field with a named state:

```
ALIVE ──(no beacon for 7 s)──► DEGRADED ──(no beacon for 60 s)──► DOWN
   ▲                                │
   └────────────(beacon)────────────┘
```

`DEGRADED` enables the fingerprint path but keeps the keypad live in case the link comes back
mid-entry. The current code disables the keypad the instant the timeout fires, which is why a
30 % packet loss rate is so visible to the user.

---

## 4. Security

The current design is a demonstrator, and the honest redesign starts by naming what it should
actually resist.

**Never transmit the secret.** A rolling code sent in clear is readable by anyone with a €10
module. Challenge–response, instead:

```
CASE → TX : CHALLENGE, nonce
TX   → CASE: RESPONSE, HMAC-SHA256(pre-shared key, nonce)[0..7]
```

The key never goes on the air. A recorded exchange is useless because the nonce never repeats.
The ESP32 has hardware SHA support, so this costs nothing in practice.

**Rate-limit attempts.** After 3 failures, 5 seconds. After 6, 60 seconds. After 10, a hard
lockout that requires the key. This turns 28 hours of brute force into geological time, and it
is about fifteen lines of code.

**Do not display the code permanently.** Show it on demand, on a button press, with a timeout.

**Use a real RNG.** `esp_random()` is only a true RNG when the RF subsystem is active or the
ADC entropy source is enabled — neither of which applied here. Call
`bootloader_random_enable()`, or derive codes from the HMAC construction above and stop
needing an RNG at all.

**Be honest about the threat model.** A solenoid inside a thin aluminium case is not resisting
a determined attacker with a screwdriver. The electronics should be proportionate to that —
which is an argument for *simpler* and *more reliable*, not for more cryptography.

---

## 5. Architecture

### One microcontroller

```cpp
Serial1.begin(57600, SERIAL_8N1, /*RX=*/26, /*TX=*/27);   // fingerprint
Serial2.begin(9600,  SERIAL_8N1, /*RX=*/16, /*TX=*/17);   // LoRa
```

The ESP32's UART pins route through the GPIO matrix, so UART1's unusable default pins are not
a constraint. A single ESP32 comfortably hosts:

- UART1 → fingerprint sensor
- UART2 → LoRa module
- I²C (21/22) → OLED
- 7 GPIO → keypad
- 1 GPIO → lock MOSFET
- 1 GPIO → buzzer (LEDC hardware PWM, not `tone()`)

That removes a microcontroller, three signalling wires, a chunk of board area, and the whole
class of bugs that comes from two loops disagreeing about state.

### Alternatives considered, and why they are not better here

The prompt for this analysis asked about multiplexing and port expanders. For completeness,
and because "you could also have…" advice is only useful if it is checked:

- **A UART multiplexer** (e.g. a 74HC4052 switching TX/RX between two peripherals) works, but
  it is strictly worse than remapping: extra parts, and only one peripheral reachable at a
  time.
- **An I²C or SPI port expander** (PCF8574, MCP23017) is the right answer for the *keypad* —
  it would free seven GPIOs — but it does nothing for the fingerprint sensor, which is a UART
  device with no I²C variant in the R55x family. A port expander cannot solve a UART shortage.
- **A software UART on ESP32** via the RMT peripheral is possible but is a lot of complexity to
  avoid one line of pin remapping.

So: remap the UART, and optionally put the keypad on a PCF8574 if the pin count gets tight.

### PCB changes

- **Keypad off GPIO12**, and off GPIO 2 / 15 while you are at it.
- **A 100 kΩ pulldown on any input-only pin** (GPIO 34–39) used for a real signal.
- **Separate lock and logic ground pours**, joined at one point near the battery negative.
- **Bulk capacitance next to the E32's supply pins**, not at the regulator.
- Move the USB connector away from the buzzer — the original report's own complaint, and a
  fair one.
- Keep the easter egg.

---

## 6. Radio

- **Reduce the power.** 10 mW ERP is the licence-free limit across most of Europe for
  433.05–434.79 MHz. Even ignoring legality, 1 W into an antenna a metre from your body,
  radiating from a metal case, is not a good design.
- **Move off 433.0 MHz.** It is the busiest slice of the band and the manual says to avoid it.
  Channel 30 (440 MHz) is quieter, and 868 MHz is quieter still — with the trade-off of shorter
  wavelength and stricter duty-cycle rules.
- **Put the antenna outside the metal.** If it must be inside, use a proper external SMA
  bulkhead — which the transmitter already did and the briefcase may not have.
- **Give the monopole a ground plane.** A quarter-wave whip needs something to work against.
- **Consider dropping the air data rate to 1.2 or 0.3 kbps.** With 24-byte packets every 5
  seconds there is plenty of airtime budget, and the sensitivity improvement is several dB —
  free range, at no cost in this application.
- **Use fixed transmission mode with a real address**, not the default broadcast-ish
  configuration on address 0. It will not make the link secure, but it stops the units from
  hearing every other default-configured E32 in the neighbourhood.

---

## 7. What I would keep

Not everything needs redesigning:

- **The graceful degradation concept.** A system that notices it has lost its primary
  authentication channel and falls back to a secondary one is a genuinely good idea. The
  implementation needs work; the idea does not.
- **The physical key as an interlock.** Ending up with the most robust security element being
  a mechanical one was an accident, but it was a good accident.
- **Rolling codes.** The right instinct, even if the implementation transmits them in clear.
- **Through-hole for a student build.** $2 versus $120, easy to solder, easy to rework, and
  the board survived being handled for two years.
- **Ground pour on both layers.** Cheap, and correct.
- **`ezBuzzer` for non-blocking audio on the main controller.** Recognising that the UI loop
  could not afford to block was the right call — it just was not applied consistently.
