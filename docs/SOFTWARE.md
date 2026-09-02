# Software

Three Arduino sketches, roughly 450 lines in total, preserved with their original Italian
filenames and comments.

| File | Board | Role |
|---|---|---|
| `valigia_definitivo.ino` | ESP32 A | Keypad, OLED, LoRa, lock, top-level state machine |
| `impronte_definitivo.ino` | ESP32 B | Fingerprint polling, buzzer melodies |
| `password_definitivo.ino` | ESP32 C | Code generation, LCD, LoRa beacon |

Libraries: `EBYTE` (Kris Kasprzak), `Adafruit_Fingerprint`, `Keypad`, `ezBuzzer`,
`Adafruit_GFX`, `Adafruit_SH110X`, `LiquidCrystal_I2C`, `Wire`, `SPI`.

---

## 0. A note on names: this code is in Italian

The sketches were written in Italian and are preserved that way. Nothing here has been renamed,
because a tidied-up reconstruction would not be the thing this repository is documenting.

This table is the decoder ring. Some of these names are ordinary Italian; a few are
abbreviations that meant something at 1 a.m. in 2024 and nothing to anyone since — including,
by now, to me.

### Data and protocol

| Identifier | Italian | English | What it actually does |
|---|---|---|---|
| `DATA` | — | — | The struct sent over the radio |
| `aperto` | aperto | opened | `true` when the case has just been opened |
| `passwordTX` | — | transmitted password | The 5-digit code |
| `connesso` | connesso | connected | Sent as `true`, cleared locally — effectively unused (C-9) |
| `MyData` | — | — | The single global instance |
| `Transceiver` | — | — | The `EBYTE` object |

### State flags

| Identifier | Literally | What it means |
|---|---|---|
| `nobuono` | "no good" | **The link-lost flag.** `true` = radio down, keypad disabled, fingerprint enabled |
| `apro` | "I open" | Lock currently energised, waiting to be released |
| `welc` | welcome | The "Welcome Mr Minguzzi" screen is showing |
| `incpass` | incorrect password | The "INCORRECT PASSWORD" screen is showing |
| `impronta_ok` | fingerprint OK | Last poll of the sensor matched |

### Timers — all `millis()` timestamps, all named badly

| Identifier | Literally | What it times |
|---|---|---|
| `Last` | — | Timestamp of the last received packet — the real link-liveness variable |
| `noconnected` | — | Rate-limits the "not connected" serial print to every 500 ms |
| `scritta` | "writing", "lettering" | Redraws the LOST CONNECTION text every 5 s |
| `timer_impronta` | fingerprint timer | Redraws the fingerprint bitmap every 10 s |
| `delei` | *unclear* — probably a mangled "delay" | Holds the lock energised for 1 s after a correct code |
| `vario` | "various" | Clears the OLED 7 s after a welcome or error screen |
| `timer_conto` | "count timer" | The 5-second beacon interval on the transmitter |
| `t` | — | Generic 100 ms poll timer on the fingerprint board |

`delei` is the one I cannot reconstruct. It is the most important timer in the sketch — it is
what releases the lock — and it is named after nothing.

### Pins and hardware

| Identifier | Literally | Meaning |
|---|---|---|
| `SUONO_OK` | "sound OK" | Correct code — **and, on the main board, the lock line** (C-19) |
| `SUONO_noOK` | "sound not OK" | Wrong code |
| `OK_IMPRONTE` | "fingerprints OK" | Fingerprint matched |
| `RIGHE` / `pin_righe` | rows | Keypad rows |
| `COLONNE` / `pin_colonne` | columns | Keypad columns |
| `Chan` | channel | **Declared in both radio sketches and never used** — the modules were configured externally |
| `buzzer`, `finger`, `display`, `lcd`, `keypad` | — | Already English |

### Strings and comments that are lies

A few artefacts that are worth knowing about before you read the source:

- **`Serial.println("\n\nAdafruit Fingerprint sensor enrollment");`** in
  `impronte_definitivo.ino`. The sketch does **not** enrol anything — it only calls
  `getImage()`, `image2Tz()` and `fingerSearch()`. This string is a leftover from Adafruit's
  `enroll` example, which is what the file was started from. It is also the best evidence for
  how the fingerprints were originally registered: with the stock example, from a separate
  sketch that no longer survives.
- **`// se leggo che dall'arduino micro ...`** in `valigia_definitivo.ino`. There is no Arduino
  Micro in the finished system; that board only appeared during early standalone testing of
  the sensor. See HARDWARE.md §3.
- **`Serial.println("apero");`** — a typo for `apro` ("I open"), printed when the fingerprint
  path fires.
- The comments contain a steady drizzle of typos — `orte seriali` for *porte seriali*,
  `sesnsore`, `comunixo`, `cartattere`, `dunzioni`, `imizializzo`, `tastierà`. They are
  preserved as written.
- **`myBitmapdownload`** — the fingerprint bitmap is named after the filename of the PNG it was
  generated from (`download.png`), by whichever image-to-C-array converter produced it.

### There is no key-switch variable

Worth stating explicitly, because it is easy to look for and not find: **neither sketch reads a
pin for the key switch.** The key is wired in series with the fingerprint sensor's supply. With
the key out, the R557 is unpowered and `getImage()` simply never succeeds. The second factor is
real, and it is entirely outside the firmware.

---

## 1. The shared data structure

Declared identically in both radio sketches:

```cpp
struct DATA {
  bool aperto;        // briefcase opened: 0 = closed, 1 = opened
  String passwordTX;  // the transmitted code
  bool connesso;      // "still connected" flag
};
```

### Actual size on ESP32

```
offset  0 : bool aperto        1 byte
offset  1 : padding            3 bytes
offset  4 : String passwordTX 16 bytes
offset 20 : bool connesso      1 byte
offset 21 : padding            3 bytes
                              ─────────
sizeof(DATA)                  24 bytes
```

24 bytes against the E32's 58-byte sub-packet limit. Packet size was never a constraint.

### Why sending a `String` this way is undefined behaviour

`EBYTE::SendStruct()` is a raw byte copy:

```cpp
bool EBYTE::SendStruct(const void *TheStructure, uint16_t size_) {
    _buf = _s->write((uint8_t *) TheStructure, size_);
    CompleteTask(1000);
    return (_buf == size_);
}
```

An Arduino `String` is an object, not an array. For longer strings it holds a `char*` into the
heap. Copying those bytes onto the wire copies the *pointer*; the receiver then has a pointer
into the sender's address space.

It worked because arduino-esp32's `String` implements small-string optimisation:

```cpp
struct _ptr { char *buff; uint32_t cap; uint32_t len; };   // 12 bytes
enum { SSOSIZE = sizeof(struct _ptr) + 4 - 1 };            // 15
struct _sso { char buff[SSOSIZE]; unsigned char len : 7; unsigned char isSSO : 1; };
union { struct _ptr ptr; struct _sso sso; };
```

Strings of up to **11 characters** are stored inline. A 5-digit code lives inside the object,
so the characters really did travel inside the struct.

**Reproduce the bug in ten minutes:** change `passwordGen(5)` to `passwordGen(12)` in
`password_definitivo.ino`. The buffer moves to the heap, the struct starts carrying a pointer,
and the receiver's behaviour becomes undefined. This is the single most instructive experiment
in the repository.

---

## 2. `valigia_definitivo.ino` — briefcase main controller

### Structure

```
setup()
  ├─ pinMode for the three inter-MCU lines
  ├─ Serial (9600) and Serial2 (9600)
  ├─ display.begin(0x3C) — SH1106 128x64
  └─ Transceiver.init() + PrintParameters()

loop()
  ├─ buzzer.loop()                        // ezBuzzer, non-blocking
  ├─ if (Serial2.available())             // radio in
  │     GetStruct → password = MyData.passwordTX; Last = millis()
  │  else
  │     link-timeout state machine → nobuono
  ├─ if (!nobuono) → keypad handling
  ├─ deferred lock release + "case opened" transmission
  ├─ deferred OLED clear
  └─ if (nobuono) → read OK_IMPRONTE, energise lock
```

### Link state machine

```cpp
if (Serial2.available()) {
  Transceiver.GetStruct(&MyData, sizeof(MyData));
  password = MyData.passwordTX;
  Last = millis();
} else {
  if ((millis() - Last >= 7000) and MyData.connesso == false) {
    // link considered lost
    nobuono = true;
    // OLED alternates "LOST CONNECTION / Use fingerprint..." every 5 s
    // with the fingerprint bitmap every 10 s
  } else {
    MyData.connesso = false;
    nobuono = false;
  }
}
```

The `connesso` field arrives as `true` from the transmitter and is cleared locally on the next
pass, so **the actual liveness detection is entirely the 7-second timeout on `Last`.** The
transmitted flag is effectively redundant. It is not wrong, just more convoluted than it needs
to be, and it took a careful read to establish that.

### Keypad handling

```cpp
char key = keypad.getKey();
if (nobuono == false) {
  if (key) {
    buzzer.beep(100);
    if (key == '#') {
      if (input_password == password) {
        digitalWrite(SUONO_OK, 1);          // lock + success tune
        // OLED "Welcome Mr Minguzzi"
        delei = millis(); apro = true; welc = true; vario = millis();
      } else {
        // OLED "INCORRECT PASSWORD"
        digitalWrite(SUONO_noOK, 1);
        delay(500);                          // blocking
        digitalWrite(SUONO_noOK, 0);
        vario = millis(); incpass = true;
      }
      input_password = "";
    } else {
      input_password += key;                 // '*' included
      // echo on the OLED at text size 4
    }
  }
}
```

Notes:

- The comparison is a plain `String ==`, i.e. byte-by-byte and not constant time. Irrelevant
  at this threat level, but worth naming.
- **No attempt counter, no delay, no lockout.**
- The 500 ms `delay()` blocks the keypad, the radio and everything else.

### Lock release

```cpp
if (millis() - delei >= 1000 and apro) {
  apro = false;
  digitalWrite(SUONO_OK, 0);
  MyData.aperto = true;
  Transceiver.SendStruct(&MyData, sizeof(MyData));
}
```

GPIO19 stays high for one second, then the "opened" packet goes out. Note that this packet is
sent **once, with no acknowledgement**: if it is lost, the transmitter never generates a new
code, the briefcase keeps the old one, and the two sides silently disagree about the state of
the system.

### Fingerprint branch

```cpp
if (nobuono) {
  if (digitalRead(OK_IMPRONTE)) {
    digitalWrite(19, 1);
    delay(1000);            // blocking
    digitalWrite(19, 0);
    nobuono = false;        // one shot
  }
}
```

The literal `19` here is the same pin as `SUONO_OK`. Setting `nobuono = false` at the end
re-enables the keypad even though the radio link is still down; the next loop pass through the
timeout branch will set it back if no packet arrives.

### Display

The OLED is an SH1106 at `0x3C` driven through `Adafruit_SH110X`. A 128×64 fingerprint bitmap
is stored in `PROGMEM` (1024 bytes) and drawn during the lost-link state.

There is a small oddity in the lost-connection screen: the code prints "CONNESSIONE PERSA" at
text size 1 at cursor (0,0), then immediately prints "LOST CONNECTION" at text size 2 starting
from the same cursor position with leading spaces used for alignment. It works, but it renders
by trial and error rather than by layout.

---

## 3. `impronte_definitivo.ino` — fingerprint and audio

```cpp
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

void setup() {
  Serial.begin(9600);
  finger.begin(57600);      // opens Serial2 on default pins 16/17
  pinMode(25, INPUT);       // SUONO_noOK
  pinMode(33, INPUT);       // SUONO_OK
  pinMode(27, OUTPUT);      // OK_IMPRONTE
}
```

`Serial2` is never opened with explicit pins, so it uses the ESP32 defaults GPIO16/17 — which
matches the report's block diagram annotation for the sensor.

The loop does three independent things:

1. If `SUONO_OK` (GPIO33) is high, play the rising tune (1480 / 1480 / 1480 / 1976 Hz).
2. If `SUONO_noOK` (GPIO25) is high, play the falling tune (1976 / 1480 Hz).
3. Every 100 ms, poll the sensor:

```cpp
bool fingerprintMatches() {
  if (finger.getImage()    != FINGERPRINT_OK) return false;
  if (finger.image2Tz()    != FINGERPRINT_OK) return false;
  if (finger.fingerSearch()!= FINGERPRINT_OK) { /* sad tone */ return false; }
  return true;
}
```

On a match it raises GPIO27, plays the success tune, and lowers GPIO27 partway through.

Known rough edges:

- `impronta_ok` holds its value between the 100 ms polls, so `if (impronta_ok)` can re-enter
  the tune block on consecutive loop passes until the next poll clears it.
- The tunes use `tone()` + `delay()`, so this board is blocked for up to ~1.5 s while playing.
  Since its only other job is polling the sensor, this is harmless here — but it is why the
  main board needed `ezBuzzer` instead.
- `getImage()` returning anything other than `FINGERPRINT_OK` covers both "no finger present"
  and "capture failed"; the code does not distinguish them.

---

## 4. `password_definitivo.ino` — code transmitter

```cpp
void loop() {
  if (Serial2.available()) {
    Transceiver.GetStruct(&MyData, sizeof(MyData));
  }
  if (MyData.aperto) {
    passwordGen(5);
  }
  if (millis() - timer_conto >= 5000) {
    timer_conto = millis();
    MyData.connesso = true;
    Transceiver.SendStruct(&MyData, sizeof(MyData));
  }
}

void passwordGen(int length) {
  String password = "";
  for (int i = 0; i < length; i++) password += String(random(10));
  MyData.passwordTX = password;
  MyData.aperto = false;
  Transceiver.SendStruct(&MyData, sizeof(MyData));
  lcd.setCursor(0, 0); lcd.print("Password:");
  lcd.setCursor(0, 1); lcd.print(password);
}
```

Simple and readable. Three things worth noting:

- `random(10)` on arduino-esp32 is backed by `esp_random()`, not the AVR seeded PRNG. However,
  ESP-IDF documents that `esp_random()` is only a true RNG when the RF subsystem (Wi-Fi or
  Bluetooth) is active or the ADC entropy source has been enabled. Neither applies here, so
  the output "should be considered as pseudo-random only".
- `MyData.aperto = false` is set inside `passwordGen`, which is what stops it firing on every
  loop pass. It works, but the flag is doing double duty as a message field and a local latch.
- The 5-second beacon means the briefcase's `password` variable is refreshed constantly, not
  only after an opening. That is why a lost "opened" packet is invisible: the beacon keeps
  arriving with the *old* code and the link looks perfectly healthy.

### What is missing compared with the report

The original report's flowchart for this unit describes a 3-minute timer, a battery percentage
calculation, and a low-battery warning below 20 %, plus a Node-RED/MQTT dashboard.

**None of that is in this sketch.** There is no networking code of any kind in any surviving
firmware. See [`ORIGINAL_REPORT_REVIEW.md`](ORIGINAL_REPORT_REVIEW.md).

---

## 5. The EBYTE library, as actually used

Only four calls from Kris Kasprzak's library appear in the project: `init()`,
`PrintParameters()`, `SendStruct()`, `GetStruct()`.

```cpp
bool EBYTE::GetStruct(const void *TheStructure, uint16_t size_) {
    _buf = _s->readBytes((uint8_t *) TheStructure, size_);
    CompleteTask(1000);
    return (_buf == size_);
}

void EBYTE::CompleteTask(unsigned long timeout) {
    // ... waits for the AUX pin to go high, or times out
}
```

What this means for the project:

| Feature | Present? |
|---|---|
| Acknowledgement | **No** — not in the library, not in the sketches |
| Retries | **No** |
| Sequence numbers | **No** |
| Message type / length field | **No** |
| Application-level CRC | **No** — the E32's FEC is internal and invisible |
| Timeout | Two, neither of them an application timeout: `CompleteTask(1000)` waits on AUX, and `Stream::readBytes` has a 1000 ms default |
| Error handling | **No** — the boolean return values are never read |

Nothing in the project configures the modules from firmware. `int Chan;` is declared in both
sketches and never used. All parameters were set once with EBYTE's Windows tool over an
FTDI232 adapter, and stored in the modules' own flash — which is why they survive power
cycles, and why the configuration screenshot in the report is the authoritative record of them.

---

## 6. Reading the code today: a short list of things to fix first

If anyone (including future me) picks this up:

1. **Frame the link.** Start delimiter, length, message type, sequence number, CRC-16. Read
   byte-at-a-time into a state machine. This alone should remove most of the observed packet loss.
2. **Stop sending `String` in a struct.** Use a fixed `char code[8]` and a packed struct, or
   serialise explicitly.
3. **Check return values.** `SendStruct` and `GetStruct` both return a boolean nobody reads.
4. **Remove the blocking `delay()` calls** from `valigia_definitivo.ino` — the codebase already
   uses `millis()` timers everywhere else, so this is a consistency fix, not a redesign.
5. **Move the keypad column off GPIO12.**
6. **Separate the lock pin from the inter-MCU signal pin.**
7. **Delete the second microcontroller** by remapping `Serial1`:
   `Serial1.begin(57600, SERIAL_8N1, 26, 27);`
8. **Change the hard-coded `"1234"`.**
