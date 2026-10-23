# LanderViz PCB specification

Status: reconciled design basis with hardware-accepted FLOW v3 and frozen
revision-A compute/display choices, 2026-10-18.

LanderViz is a standalone audio visualizer whose revision-A printed circuit
board (PCB) targets a ZSA Moonlander Mark I. It sits between the keyboard and
the Mac, samples stereo line audio from a mixer, performs fast Fourier
transform (FFT) analysis on an RP2354A, and sends per-key red/green/blue (RGB)
frames to custom Moonlander firmware. FLOW averages the channels for one mono
FFT; SPECTRUM retains independent per-channel FFTs. No visualization software
runs on the Mac. The proven breadboard uses an RP2040; revision A's RP2354A
selection is the deliberate custom-board successor.

The breadboard prototype validates the end-to-end architecture. Repository
commit `ebb5dcd` is the hardware-accepted FLOW v3 firmware milestone. The
repository at <https://github.com/grounded042/landerviz> is the behavioral,
protocol, pin-map, and wiring reference. Record the final PCB-design baseline
commit in the release notes before ordering boards.

## Status vocabulary

- **Frozen** means revision A must follow this requirement unless it is
  explicitly reopened.
- **Open** means the decision must be completed before schematic capture can
  be considered finished.
- **Order blocker** means a board must not be fabricated until the item is
  resolved and reviewed.

## Proven behavior

The prototype has demonstrated:

- independent stereo audio capture and FFT processing;
- USB host operation toward the Moonlander;
- ordinary Human Interface Device (HID) keyboard reports forwarded to macOS;
- per-key RGB delivery over the vendor Raw HID protocol;
- eight 32-byte Raw HID packets per complete RGB frame;
- liquid-crystal display (LCD) status, mode-button input, silence detection,
  and all visualization modes;
- complete-system power from a data port on the powered Anker hub; and
- sustained delivery of approximately 30 complete RGB frames per second with
  no missed analysis windows and responsive keyboard forwarding.

Thirty frames per second (FPS) is the current proven target, not 60 FPS. The
Moonlander firmware's 26 ms LED flush interval imposes a ceiling of
approximately 38 FPS, so 60 FPS is not a revision-A requirement.

If RGB packets stop for 500 ms, the Moonlander returns to its own layer
lighting. Both firmwares cap brightness at 175/255.

The observed left/right level difference was traced upstream of LanderViz.
Intermittent mixer-powered ground noise is also external to the visualizer.
Windows enumeration remains unresolved; macOS is the frozen revision-A host
platform.

## Frozen topology

```text
Moonlander -- downstream USB --> RP2354A -- upstream USB --> powered hub --> Mac
                                       |
                                       +-- stereo audio input
                                       +-- external display port
                                       +-- mode button
```

The RP2354A is a Universal Serial Bus (USB) host toward the keyboard and a USB
device toward the Mac. Native RP2354A USB is upstream. Pico-PIO-USB uses the
chip's programmable input/output (PIO) hardware to implement the downstream
host on general-purpose input/output (GPIO) pins 0/D+ and 1/D- through two
22 ohm series resistors. Typing is forwarded to the Mac; visualization data
travels in the opposite direction to the Moonlander.

## Compute — frozen

Revision A uses a bare RP2354A in its 60-pin, 7 x 7 mm QFN package, assembled
by JLCPCB, not a Pro Micro module. **QFN** means quad-flat no-lead, a fine-pitch
surface-mount package. RP2354A provides two Arm Cortex-M33 cores with hardware
single-precision floating point and digital signal processing instructions,
520 kB of static random-access memory (SRAM), and 2 MB of stacked program
flash. Revision A uses the Arm cores so the existing Cortex Microcontroller
Software Interface Standard DSP library (CMSIS-DSP) integration carries
forward. The project does not depend on hand assembly or repair of the
processor; order spare boards.

Follow Raspberry Pi's
[Hardware design with RP2350](https://datasheets.raspberrypi.com/rp2350/hardware-design-with-rp2350.pdf)
reference design and checklist. The minimum design includes:

- a 12 MHz crystal and its correctly calculated load capacitors;
- the RP2354A boot configuration and integrated-flash requirements exactly as
  specified by the reference design;
- a 5 V-to-3.3 V regulator;
- all required local supply, core-regulator, inductor, and decoupling
  components;
- the required analog-to-digital converter (ADC) supply/reference treatment;
- 27 ohm series resistors on native upstream USB D+ and D-;
- boot-select (BOOTSEL) and reset/RUN controls; and
- Serial Wire Debug (SWD) pads plus 5 V, 3.3 V, and ground test points.

SWD is the processor's programming and debug interface.
Revision A adds no external flash or pseudo-static random-access memory
(PSRAM): the integrated 2 MB flash is the program store, and additional memory
is outside the frozen scope. At bill-of-materials (BOM) release, verify the
RP2354A stepping, package, JLCPCB/LCSC stock, lifecycle, and Pico software
development kit (SDK) support. Require A4 silicon or a later stepping
explicitly accepted after reviewing its errata.

The visualization, analysis, USB-bridge, and Moonlander protocol code carries
over. Firmware is not literally unchanged: the raw-chip board target, any pin
assignment changes, the display driver, and board-specific initialization must
be implemented and tested.

The RP2040 breadboard remains the behavioral and timing baseline: its accepted
build measured 12.5 ms total analysis and 17.0 ms render/submit time inside a
33.3 ms frame period while servicing both USB roles and the LCD. RP2354A is
selected to retain the Pico SDK and PIO architecture while adding compute and
memory headroom and eliminating external flash. Do not assume a specific speed
increase: the assembled revision-A board must still pass the 30 FPS, USB, ADC,
and keyboard-latency acceptance checks.

## GPIO map

**I2C** means Inter-Integrated Circuit, a bidirectional two-signal serial bus.
Its signals are **SDA** (serial data) and **SCL** (serial clock).

These assignments are frozen:

| Function | RP2354A pin |
|---|---:|
| PIO-USB host D+ | GPIO0 through 22 ohms |
| PIO-USB host D- | GPIO1 through 22 ohms |
| Display/expansion I2C1 SDA | GPIO2 |
| Display/expansion I2C1 SCL | GPIO3 |
| Left audio | GPIO26 / ADC0 |
| Right audio | GPIO27 / ADC1 |
| Mode button | GPIO8 |

The mode button uses the internal pull-up resistor and approximately 30 ms
debounce. A short press selects the next mode; holding for more than 800 ms
selects the previous mode.

GPIO2 through GPIO7 drove the prototype's parallel LCD. Revision A reuses
GPIO2 and GPIO3 as the native I2C1 data and clock pins shared by the display
port and the 3.3 V digital expansion footprint. GPIO4 through GPIO7 remain
available for the expansion interface; their final mapping is open.

## USB-C connectors — frozen form, open power details

Revision A has two USB-C receptacles and no USB-A connector:

- The Mac/hub-facing upstream port is a USB device and power sink.
- The Moonlander-facing downstream port is a USB host and power source.

There is no USB Power Delivery negotiation. **CC**, or configuration channel,
is still required even without Power Delivery:

- The upstream sink uses one 5.1 kilohm pull-down on each CC pin.
- The downstream source uses one pull-up on each CC pin.
- At each receptacle, connect both USB 2.0 D+ contacts together and both D-
  contacts together so either cable orientation works.
- Use only two data-series resistors per USB link, after the orientation pairs
  join, and place them close to the driving pins.
- Never place capacitors on D+ or D-.

Route D+ and D- as a short differential pair over an uninterrupted ground
reference. A **differential pair** is a matched pair of traces that carries a
signal as the voltage difference between them.

### Downstream current advertisement — order blocker

A 22 kilohm CC pull-up advertises 1.5 A. That value is not yet approved. The
current firmware declares a 500 mA upstream USB requirement, and a 1.5 A-rated
fuse or switch does not make 1.5 A available from the hub. The downstream port
must not advertise more current than the complete upstream path can supply
after reserving power for the RP2354A, audio frontend, and display.

Choose the downstream CC pull-ups only after power measurements and the
upstream source contract are known. Default-current advertisement is the safe
baseline for the proven USB-A-to-USB-C hub setup. If 1.5 A advertisement is a
requirement, the upstream Type-C source and the board must both establish and
support that budget without relying on USB Power Delivery.

**VBUS** is the 5 V USB bus-power rail. The exact downstream attachment and
VBUS switching circuit is open. Revision A should use current-limited,
protected VBUS switching if the size and component budget permit it. Document
any deliberate departure from standards-compliant attach-controlled VBUS
before schematic approval.

## Power architecture — partially frozen

The intended path is:

```text
upstream USB-C VBUS
    -> input protection
    -> downstream current-limited VBUS path
    -> Moonlander USB-C VBUS
    -> local bulk capacitor near the downstream connector

upstream USB-C VBUS
    -> 3.3 V regulator
    -> RP2354A and 3.3 V logic

upstream USB-C VBUS
    -> protected/switched 5 V display output
```

The display output is sized only for revision A's 5 V PCF8574T-backed 16x2
character LCD. Vacuum fluorescent displays are not supported or included in
the revision-A power budget.

The Pro Micro underside-VBUS connection was a breadboard expedient and does
not appear on this board. Retain approximately 470 microfarads of bulk
capacitance close to the downstream connector unless measurement supports a
different value. Never power the Moonlander from a second supply at the same
time.

Component voltage/current ratings should include sensible margin, but the
protection threshold and USB current advertisement must be based on the real
available current rather than a nominal “1.5 A class” goal.

### Power-budget measurement — order blocker

Use the
[FNIRSI FNB58](https://www.fnirsi.com/products/fnb58), or an equivalent
logging USB power meter with USB 2.0 data passthrough, to record voltage and
current curves at up to 100 samples per second. Insert it between the powered
Anker hub's data port and LanderViz so it measures the complete system. A
simple two-readings-per-second charging meter is insufficient for the startup
and LED-transition checks. Record the breadboard baseline first and repeat the
same cases on the assembled PCB:

- the controller without the Moonlander or display;
- the Moonlander connected with visualization idle;
- typical visualization current;
- the worst observed current at `BRIGHTNESS_CAP`;
- downstream VBUS voltage during Moonlander startup and the worst LED
  transition;
- each supported display at typical and maximum brightness; and
- startup/inrush current with the Moonlander and target display connected.

The meter cannot prove extremely short electrical spikes. If its recorded
curve shows material voltage sag, unstable enumeration, or insufficient margin,
repeat the suspect transition with a faster instrument before selecting the
power-path ratings.

### RP2040 breadboard baseline — 2026-10-22

The FNB58 sampled the complete system at approximately 100 samples per second
between the powered Anker hub and LanderViz. These are upstream VBUS
measurements; they do not measure the voltage after the prototype's underside
VBUS pad, breadboard contacts, jumpers, or downstream USB breakout.

| Case | Duration | Minimum VBUS | Average current | Peak current | Peak power |
|---|---:|---:|---:|---:|---:|
| Controller and parallel LCD, Moonlander disconnected | startup interval | — | approximately 0.040 A | — | — |
| Moonlander connected, visualization idle | 30 s | 5.196 V | 0.262 A | 0.303 A | 1.573 W |
| FLOW with music | 60 s | 5.196 V | 0.291 A | 0.371 A | 1.927 W |
| Moonlander reconnection/startup | event | 5.177 V | — | 0.505 A | 2.615 W |
| All 72 keys white at 175/255 | 300 s | 5.174 V | 0.708 A | 0.709 A | 3.668 W |

The five-minute 175/255 white soak remained visually white and electrically
flat. Its first minute averaged 0.7076 A and its last minute averaged 0.7082 A,
with no meaningful thermal drift at the meter. This validates the existing
firmware cap of 175 for the breadboard prototype's worst commanded frame.

Higher settings are not usable on the prototype. All-white 200/255 eventually
shifted red/orange; 225 and 255 did so as well. The keyboard recovered white
only after the command was reduced to 64. Upstream VBUS remained stable, so
the observation establishes a downstream sustained-load limit but does not by
itself identify whether the limiting element was the Pro Micro VBUS path,
breadboard wiring, USB breakout, cable, or Moonlander. Do not use 200 as a
design target merely because it survived briefly during a stepped sweep.

The 0.708 A worst-case measurement also exceeds the firmware's current 500 mA
upstream USB declaration. Therefore this measurement closes the prototype
brightness-cap question but does not close the revision-A power order blocker.
Before schematic approval:

- repeat the baseline with the frozen PCF8574T serial LCD and level translator;
- decide how the upstream USB-C connection validly supplies the measured
  worst case;
- select the downstream switch, protection, traces, connectors, and cable path
  with margin above the final measured load; and
- repeat startup and five-minute 175/255 white tests on the assembled PCB while
  also measuring downstream VBUS at the Moonlander connector.

**VFD** means vacuum fluorescent display. VFD support is deferred to revision
B or later because these displays can require substantially more operating and
startup current than the revision-A character liquid-crystal display (LCD).
Adding one later requires a fresh power budget, interface-voltage review, and
connector review.

## Audio frontend — frozen topology, open protection/filter values

The 3.5 mm tip/ring/sleeve (TRS) stereo jack and both analog input stages are
on the main PCB. There is no audio codec in revision A and no pluggable analog
submodule.

Each channel retains its own circuit:

```text
3.3 V
  |
100k
  |
  +---- ADC node ---- GPIO26 or GPIO27
  |
100k
  |
 GND

TRS signal ---- 10 uF coupling capacitor ---- ADC node
```

The channels must not share a bias node. If polarized capacitors are used,
their positive terminals face the 1.65 V ADC nodes. Use matched 1% bias
resistors. Firmware currently adds 8x, or approximately +18 decibels (dB),
visualization gain because the proven passive mixer attenuates its output. Clip
indication is based on the physical ADC input before that gain.

Revision A should add appropriately sized input protection and an anti-alias
low-pass filter for the 15.625 kHz-per-channel sample rate. **Anti-alias** means
attenuating frequencies above the approximately 7.8 kHz Nyquist limit so they
do not appear as false lower frequencies after sampling. These additions were
not part of the proven breadboard path: calculate or simulate their values,
confirm that they do not load the mixer or erase useful spectrum, and validate
them before ordering.

Keep ADC nodes and input traces short and away from USB data, display clocks,
and Moonlander/display power. Add labeled test points for left ADC, right ADC,
the 1.65 V bias nodes, 3.3 V, 5 V, and ground.

## Grounding — frozen intent, corrected PCB implementation

The breadboard exposed common-impedance coupling: keyboard/LED return current
flowed through a ground-rail segment shared with audio, creating audible
LED-dependent noise. Separating those return paths fixed it.

On the PCB, preserve that intent with placement and return-current control,
not a split ground plane:

- Use a continuous low-impedance ground plane.
- Place the analog frontend together near the RP2354A ADC pins.
- Route downstream keyboard and display power so their high current returns
  flow directly toward the upstream power entry rather than through the analog
  region.
- Do not route USB differential pairs or digital clocks across a ground split.
- Join connector shields and protection components according to the final
  connector/reference design.

An “analog island joined at one point” is not frozen because an actual split
can force digital return currents around the gap and make noise or USB signal
integrity worse. The schematic/layout review must inspect expected return
paths explicitly.

An external stereo transformer ground-loop isolator remains the remedy for a
persistent mixer/USB ground loop. The PCB must not implement an unsafe ground
lift.

## External display — frozen

The display is external and pluggable; the prototype's parallel QC1602A is not
mounted on the main board. Revision A supports one display class: a 5 V,
HD44780-compatible 16x2 character LCD with a PCF8574T I2C backpack. The
accepted purchasing reference is the Teyleten Robot three-pack identified by
Amazon product identifier `B07T8ZG5D1`; delivered units must still pass the
checks below because commodity-module revisions can change without notice.

All VFD modules, including the Noritake CU16029-UW1J and GU-series graphic
modules, are explicitly outside revision A. The board does not reserve their
current budget or include their specialized 5 V bidirectional interface.
Graphical OLED modules are also deferred to revision B or later.

Revision A runs the display's I2C bus at 100 kHz for PCF8574T compatibility.

The main board uses a keyed, four-position JST-PH 2.0 mm-pitch connector with
this frozen pinout:

| Pin | Signal | External level |
|---:|---|---:|
| 1 | GND | 0 V |
| 2 | display power | 5 V |
| 3 | SDA | 5 V I2C |
| 4 | SCL | 5 V I2C |

The cable is straight-through to the backpack's labeled `GND`, `VCC`, `SDA`,
and `SCL` pins. Confirm the footprint's pin-one orientation against the exact
JST drawing and mark both connector ends clearly; wire color is not the source
of truth.

RP2354A pins are not 5 V tolerant. The main board therefore includes
bidirectional, open-drain-compatible level translation between the internal
3.3 V I2C1 bus and the external 5 V display bus. **Open-drain** means a device
can pull a signal low but relies on a resistor to return it high, allowing
multiple I2C devices to share the wire safely. Select the exact translator and
pull-up resistor values during schematic capture after inspecting the delivered
backpacks; do not assume their fitted pull-up values.

Firmware probes the common PCF8574T address `0x27` and PCF8574A address `0x3F`,
then uses the verified backpack-to-LCD pin mapping. The delivered modules must
be checked for address, mapping, contrast adjustment, backlight control,
100 kHz communication, and refresh behavior on the RP2040 breadboard before
the PCB is ordered. The display driver becomes a replaceable firmware
interface even though revision A ships only this character-display backend.

## Digital expansion footprint — frozen presence, open connector

Provide an unpopulated expansion footprint on the digital side for a future
codec or microphone module. **I2S** means Inter-IC Sound, a clocked digital
audio bus. The footprint should expose:

- I2S data, bit clock, and word-select clock;
- I2C data and clock;
- 3.3 V;
- 5 V;
- ground; and
- at least one spare GPIO.

No analog audio crosses this connector. The expansion footprint shares the
internal 3.3 V I2C1 bus on GPIO2/GPIO3 with the display level translator. Its
exact connector and remaining GPIO assignments remain open. If this footprint
prevents the main board from meeting the size target, return for an explicit
scope decision rather than silently increasing board size.

## Mechanical target

The main board should be connector-limited and approximately 35–40 mm class.
That is a placement target, not yet a guaranteed outline. It must accommodate:

- two USB-C receptacles;
- one 3.5 mm TRS jack;
- the RP2354A support circuit;
- the analog frontend;
- protected Moonlander and display power paths;
- the bulk capacitor;
- mode, BOOTSEL, and reset controls;
- display and expansion connectors; and
- debug/test access.

Freeze connector direction, enclosure geometry, mounting holes, control
access, and cable clearances before routing. Do not enlarge the board or move
to four layers without reviewing the tradeoff. A two-layer board is acceptable
only if it can retain a continuous ground reference, clean USB routing, and
the required analog/power separation at the target size.

## Revision-A review gate

Electrical-rules check (ERC) and design-rules check (DRC) are the
computer-aided design tool's automated schematic and layout checks. They are
necessary but cannot prove the board will work. Before fabrication:

1. Commit and push the known-good breadboard milestone and record its commit.
2. Verify the frozen display connector pinout and freeze the remaining
   expansion connector pinout and GPIO assignments.
3. Resolve the upstream budget, downstream CC advertisement, and VBUS switch
   for the controller, Moonlander, and frozen character display.
4. Select the regulator, crystal/load capacitors, USB protection, RP2354A
   stepping, and assembly-supported packages.
5. Calculate or simulate the audio protection and anti-alias filter, then
   validate any material change to the proven input response.
6. Freeze the enclosure and connector geometry after a component placement
   study confirms the 35–40 mm target is credible.
7. Check every RP2354A support net and boot setting against Raspberry Pi's
   hardware-design checklist, including crystal, integrated-flash boot, core
   supply, ADC supply, USB, BOOTSEL, and exposed-pad grounding.
8. Review both USB-C ports in both cable orientations. Explicitly verify that
   upstream CC uses pull-downs and downstream CC uses pull-ups.
9. Review high-current return paths and ensure no USB or clock trace crosses a
   ground discontinuity.
10. Run ERC and DRC, inspect fabrication outputs, and obtain an independent
    schematic/layout review.
11. Order a small assembled revision with spares and bring it up without the
    Moonlander or optional display attached first.

Revision A exists to reproduce the proven LanderViz behavior on a reliable,
compact PCB. New visualizer features or unrelated hardware are outside this
conversion unless explicitly approved.
