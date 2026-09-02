# A critical review of the original school report (and made entirely by AI (i'm lazy))

The 2024 report is preserved unedited in [`original-report/`](original-report/). It is in
Italian, 33 pages long, and it was written by a student with a deadline.

The original school report contained several assumptions and claims that were not fully
validated by real-world testing. This repository documents the project more critically and
transparently. This file goes through the report section by section and marks each substantive
claim as **correct**, **imprecise**, **unverifiable**, or **wrong**, with the evidence.

The point is not to embarrass a document I wrote. It is that a repository claiming to be
honest has to be checkable, and the fastest way to make it checkable is to show my working
against the primary source.

---

## Section 1 — "L'idea"

| Claim | Verdict | Notes |
|---|---|---|
| Keypad entry, buzzer feedback per keypress, second buzzer for the success tune | **Correct** | Matches the firmware exactly |
| A signal from an ESP32 activates the lock through a power transistor | **Correct** | GPIO19 → BDX53C |
| After opening, the case reports it over LoRa and receives a new code | **Correct** | Confirmed in both sketches |
| On link loss the keypad becomes unusable and the OLED tells the user to use the fingerprint | **Correct** | Confirmed |
| Module identified as "ebyte 433t30d" | **Correct** | |

Section 1 is accurate. It is a clear, honest description of what the system does.

---

## Section 2 — Block diagram and flowcharts

| Claim | Verdict | Notes |
|---|---|---|
| Keypad on 33, 25, 26, 27, 14, 12, 13 | **Correct** | Matches `pin_righe` / `pin_colonne` |
| OLED on 21, 22 | **Correct** | ESP32 I²C defaults |
| LoRa on 16, 17, 4, 23, 5 | **Correct** | Matches `PIN_RX/TX/M0/M1/AX` |
| Fingerprint sensor on 16, 17 "from the second ESP32" | **Correct** | `Serial2` defaults; excellent cross-check |
| Darlington on pin 19 | **Correct** | Same pin as `SUONO_OK` — see KNOWN_ISSUES C-19 |
| UART used for the fingerprint sensor and the LoRa modules | **Correct** | |
| I²C used for both displays | **Correct** | |
| Transmitter flowchart: "3 minutes elapsed?" → init timer → compute battery % → warn below 20 % | **Wrong** | The firmware uses a **5-second** timer and contains **no battery calculation** at all |
| Briefcase flowchart | **Correct** | Accurately describes the implemented state machine |

The briefcase flowchart is a genuinely good piece of documentation. The transmitter one
describes software that does not exist.

---

## Section 3 — Power supply

| Claim | Verdict | Notes |
|---|---|---|
| "A normal 9 V battery provides sufficient voltage to power all the components **for a prolonged time**" | **Wrong** | It cannot. See KNOWN_ISSUES C-16: several ohms of internal resistance against 610 mA transmit bursts, and capacity that collapses above a few tens of mA. The photographs show a **zinc-carbon** cell, which is the worst case |
| "These batteries are known for their **durability**" | **Wrong** | The opposite is true at these currents |
| LM7805 converts ~9 V to a stable 5 V ±0.1 V | **Correct** | |
| Purpose of the 10 µF and 100 nF capacitors, and placing them close to the regulator | **Correct** | Well explained, and good practice |
| A heatsink with thermal paste is needed because linear regulators produce heat | **Correct in principle** | But the report does not compute the dissipation. At 300 mA it is 1.2 W; at the lock's 1.7 A it would be 6.8 W, which no small heatsink handles |
| The power schematic is labelled **+12 V** at the regulator input | **Inconsistent** | The text and photographs both show a 9 V battery. Treat the label as a drawing artefact |
| A Darlington "is able to handle high currents, allowing the solenoid to be activated effectively and safely" | **Wrong in this context** | See below |

---

## Section 3.2 — The Darlington

| Claim | Verdict | Notes |
|---|---|---|
| A solenoid needs more current than a microcontroller pin can supply | **Correct** | |
| Darlington hFE is the product of the two stages, e.g. 100 × 100 = 10 000 | **Correct as theory** | But the BDX53C datasheet guarantees **hFE ≥ 750**, not 10 000 — and 750 is the number that matters for design |
| BDX53C: 100 V, 8 A, with protection resistors and a freewheel diode | **Correct** | Confirmed against the datasheet |
| "I preferred the empirical method for finding the right resistance. After various tests I discovered that the correct base resistance is 1 kΩ" | **Correct value, undocumented conditions** | 1 kΩ is roughly right when the base is fed from the lock's 18 V rail (≈16 mA, β needed ≈ 210). It cannot work from a 3.3 V GPIO (≈1.5 mA, β needed ≈ 1130 against 750 guaranteed). The report states the value without stating what it is measured across, which is precisely what makes an empirical result untransferable |
| Explanation of the freewheel diode and inductive kickback | **Correct** | Clearly and correctly explained |

The report never states the lock's current, and never mentions that in the working build the
lock circuit ran from a separate 18 V drill battery rather than the 9 V rail everything else is
described against. Those two omissions are what made the whole chapter unverifiable — including
to its author, two years later.

---

## Section 4 — The microcontroller

| Claim | Verdict | Notes |
|---|---|---|
| ESP32 dual-core up to 240 MHz, 12-bit SAR ADC, two 8-bit DACs, I²C and SPI | **Correct** | |
| ESP32 vs Arduino Uno comparison table | **Correct** | |
| "The ESP32's advanced energy management optimises consumption in battery-powered devices" | **Unverifiable, and not true of this build** | The feature exists; the project never used deep sleep, WOR or any load switching |
| "The only downside of the ESP32 is 3.3 V operation, which I managed to compensate for" | **Imprecise** | 3.3 V operation was not compensated for — it is exactly what broke the lock driver. The E32 and R557 are both **3.3 V** parts, so 3.3 V was an advantage there; the mistake was pairing a 3.3 V GPIO with a Darlington |

---

## Section 5 — The fingerprint sensor

| Claim | Verdict | Notes |
|---|---|---|
| R557 by Hangzhou Grow Technology; UART, 3.3 V TTL, 30 mA capture / 5 µA standby, 120 templates, <0.001 % FAR, <1 % FRR, 508 dpi, <0.2 s capture | **Correct** | Matches the manufacturer's manual |
| LED ring: red / green / blue status | **Correct** | Visible in the demonstration video |
| Losing the wireless link triggers the fingerprint path | **Correct** | |
| A physical key turn is required to activate the sensor | **Correct** | And the report is right that this is a real second measure. Worth adding: the firmware knows nothing about the key — it is purely a power interlock |
| First tested on an Arduino Micro, then integrated with an ESP32 | **Correct** | And relevant: the Micro is 5 V, the R557's lines are rated for 3.6 V maximum. See KNOWN_ISSUES N-1 |
| Long capacitive-vs-optical comparison | **Correct** | Good background, though it goes further than the project needs |

---

## Section 6 — The LoRa module

| Claim | Verdict | Notes |
|---|---|---|
| E32-433T30D based on the Semtech SX1278 | **Correct** | |
| LoRa uses chirp spread spectrum (CSS) | **Correct** | |
| "LoRa allows transmission over 10 km in rural areas, 3–5 km in dense urban areas" | **Correct as a general statement about the technology** | But it sits next to the project's own results without any reconciliation, which is what made the "one kilometre" claim feel supported |
| "Receiver sensitivity up to **−148 dBm**" | **Imprecise — quoted out of context** | That figure is measured at **0.3 kbps, SF12, CR 4/5**. The project ran at **2.4 kbps**, where the datasheet explicitly states sensitivity is reduced |
| The −148 dBm → 1.58 fW conversion | **Correct** | The arithmetic is right |
| "Channel 17, so 433 MHz + 17 MHz = **450 MHz**" | **Wrong, twice** | The E32 formula is **410 + CHAN**, not 433 + CHAN. And the channel was **not 17**: the report's own screenshot shows `0x17`, which **is 23 decimal** — the factory default. Real frequency: **410 + 23 = 433.0 MHz**, exactly as the same screenshot states (`Freq Now: 433.0MHz`). A hexadecimal value was read as decimal |
| Quarter-wave antenna ≈ 173 mm at 433 MHz | **Correct** | |
| Monopole radiation pattern description | **Correct** | |
| Configured via the EBYTE tool over an FTDI232 | **Correct** | And the screenshot is the single most valuable artefact in the whole report — the entire radio configuration is recoverable from it |
| "Unicast, point-to-point half duplex" | **Imprecise** | `OPTION = 0x44` bit 7 = 0 means **transparent** mode, not fixed/addressed mode. On address 0 with a default channel, any E32 in range receives the traffic. "Half duplex" is correct |

### What the screenshot actually proves

`Param Now: 0x00, 0x00, 0x1a, 0x17, 0x44` decodes as:

| Register | Value | Meaning |
|---|---|---|
| ADDH / ADDL | `0x00 0x00` | Address 0, factory default |
| SPED | `0x1A` | 8N1 · UART 9600 · air data rate **2.4 kbps** |
| CHAN | `0x17` | **Channel 23**, i.e. **433.0 MHz** |
| OPTION | `0x44` | **Transparent** · push-pull IO · WOR 250 ms · **FEC on** · **30 dBm** |

Every field matches the dropdowns visible in the same screenshot. This is why the radio
configuration can be stated as fact rather than recollection.

---

## Section 7 — The code

| Claim | Verdict | Notes |
|---|---|---|
| "More than 400 lines of code" | **Correct** | ~450 across the three sketches |
| Explanation of `PROGMEM` and the bitmap | **Correct** | |
| The half-duplex reception snippet | **Correct as printed** | The report reproduces the code faithfully — including the missing framing that turned out to matter |
| "It is enough to write the data into the structure and send it" | **Correct as a description, dangerous as advice** | See KNOWN_ISSUES C-2: it works only because of small-string optimisation |
| The authentication mode adapts to link conditions, with a 7-second threshold | **Correct** | |
| Earlier in the report: "I had also implemented an acknowledge system" | **Wrong** | No acknowledgement exists in any surviving firmware. Either it was in an earlier revision, or "acknowledge" described the 5-second beacon |

---

## Section 8 — Node-RED integration

| Claim | Verdict |
|---|---|
| Node-RED dashboard over MQTT, with topics `start`, `codice`, `Batteria`, `Codice`, `connesso`; gauge, text and LED widgets | **Never implemented** |

The MQTT / Node-RED integration described in section 8 of the original report was never
implemented. **No networking code of any kind exists in any of the surviving sketches** — no
`WiFi.h`, no MQTT client, no `callback()`, no battery percentage calculation. The transmitter's
only timer is the 5-second beacon.

The same applies to the battery-monitoring branch of the section 2 flowchart, which belongs to
the same non-existent feature.

---

## Section 9 — Build and testing

| Claim | Verdict | Notes |
|---|---|---|
| Breadboard prototyping, then integration, then PCB | **Correct** | Photographs confirm |
| KiCad → EasyEDA migration | **Correct** | |
| SMD rev. 1 quoted at over $120, abandoned for a through-hole design at $2 + $15 shipping | **Correct** | And a good decision |
| Ground pour on both faces to reduce EMI | **Correct** | |
| Range test table (95 / 79 / 70 / 62 % at 50 / 100 / 400 / 1000 m) | **Unverifiable** | A single unrepeated walk, one route, one day, presented as a characterisation of the system. The author does not stand behind the numbers |
| Method: transmitter at home, walking away, at least 10 received beacons | **Correct, and commendably explicit** | Stating the method is what lets the table be assessed at all — and what makes it clear how thin it is |
| "not in line of sight, but in a fairly built-up area" | **Imprecise** | The route was roughly line of sight; the description understates the conditions, which makes the numbers look better than they were |
| The surrounding narrative implying a range of "about a kilometre" | **Wrong** | One unrepeated run does not support a range figure, and 62 % of packets is not "range" in any case |

**Neither the table nor the conclusion drawn from it deserved the confidence they were given.**
The author's repeated lived experience — reliable, then a drop at roughly 400 m that did not
recover — is better evidence than the table, and points at a different cause entirely. See
[`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) C-10 and S-1.

---

## Section 10 — Defects and possible improvements

This is the best section in the report, and most of it stands up:

| Claim | Verdict | Notes |
|---|---|---|
| Moving to SMD would improve density | **Correct** | |
| The USB connector is too close to the first buzzer | **Correct** | A real and specific layout complaint |
| The heatsink could be improved, "perhaps a fan" | **Correct diagnosis, wrong fix** | A fan moves heat; a switching regulator stops producing it. The right answer is a buck converter |
| Higher-capacity lithium batteries would improve runtime and reliability | **Correct** | This is exactly right, and the report identified it two years before this document did |
| "An intermittent problem concerns the loss of connection, which seems to occur for no obvious reason … could be attributed to firmware issues, external interference, or insufficient robustness of the wireless circuit design" | **Correct, and honest** | The report names the problem and refuses to explain it away. The most likely causes are now identified — brownouts during transmit bursts and permanent stream desynchronisation — but the report was right to leave it open rather than invent a cause |
| Soldering skills could be improved | **Correct** | |

Section 10 is the reason this repository was worth writing. The instinct to list what did not
work was already there in 2024; what was missing was the knowledge to explain it.

---

## Summary

| Verdict | Count |
|---|---|
| Correct | ~35 claims |
| Imprecise | 6 |
| Unverifiable | 2 |
| Wrong | 7 |

The wrong ones, collected:

1. The 9 V battery was adequate "for a prolonged time" — it was not, by a wide margin.
2. 9 V batteries are "known for their durability" — not at these currents.
3. The operating frequency was 450 MHz — it was 433.0 MHz, from a hex-as-decimal misreading.
4. −148 dBm sensitivity applied to this configuration — it is the 0.3 kbps figure.
5. A range of about one kilometre — asserted from a single unrepeated walk, and 62 % of packets received is not a range figure in any case.
6. An acknowledgement system was implemented — no such thing exists in the firmware.
7. The Node-RED / MQTT integration — never implemented.

Seven wrong claims out of roughly fifty is a reasonable ratio for a first serious engineering
document written under exam pressure. Two still bother me:

- **The frequency**, because it was checkable in ten seconds against a screenshot printed on
  the facing page.
- **The range table**, because the problem was not the measurement — it was presenting one
  unrepeated walk as though it characterised the system. That is a habit, not a slip, and it is
  the one this repository is most trying to correct.
