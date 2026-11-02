# LanderViz PCB specification

Status: schematic capture in progress with hardware-accepted FLOW v3 and
frozen revision-A compute, display, and USB/power architecture, 2026-10-29.

LanderViz is a standalone audio visualizer whose revision-A printed circuit
board (PCB) targets a ZSA Moonlander Mark I. It sits between the keyboard and
the Mac, samples stereo line audio from a mixer, performs fast Fourier
transform (FFT) analysis on an RP2354A, and sends per-key red/green/blue (RGB)
frames to custom Moonlander firmware. FLOW averages the channels for one mono
FFT; SPECTRUM retains independent per-channel FFTs. No visualization software
runs on the Mac. The proven breadboard uses an RP2040; revision A's RP2354A
selection is the deliberate custom-board successor.

The breadboard prototype validates the end-to-end architecture. Repository
commit `ebb5dcd` is the hardware-accepted FLOW v3 firmware milestone;
`5d1f1a0` adds the accepted rotary menu and color controls. The repository at
<https://github.com/grounded042/landerviz> is the behavioral, protocol,
pin-map, and wiring reference. Record the final PCB-design baseline commit in
the release notes before ordering boards.

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
- liquid-crystal display (LCD) status, rotary-encoder input, silence detection,
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
                                       +-- rotary encoder + push switch
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

### Raspberry Pi reference-design boundary

The schematic-capture reference is Raspberry Pi's `RP2350A Minimal` KiCad
design, revision R4-S1 dated July 2026, obtained from
<https://pip.raspberrypi.com/documents/RP-010328-CA-RP2350A%20Minimal%20KiCAD.zip>.
The downloaded archive's SHA-256 digest is
`c0b858d78f56fa271a827064198e8ee81e4ec246234ce6659be8a1fcd877d06b`.
It is published under the MIT license.

That design uses **RP2350A**, while LanderViz uses **RP2354A**. They share the
QFN-60 package and much of the processor support circuitry, so the reference
is used net by net for power, core regulation, decoupling, crystal, USB,
RUN/reset, BOOTSEL, Serial Wire Debug (SWD), and exposed-pad treatment. Its
external QSPI flash circuit, flash boot configuration, Micro-USB connector,
board outline, and layout are not inherited. The RP2354A integrated-flash
configuration must be checked directly against the current RP2350 datasheet
and hardware-design guide before schematic approval.

## GPIO map

**I2C** means Inter-Integrated Circuit, a bidirectional two-signal serial bus.
Its signals are **SDA** (serial data) and **SCL** (serial clock).

These assignments are frozen:

| Function | RP2354A pin |
|---|---:|
| PIO-USB host D+ | GPIO0 through 22 ohms |
| PIO-USB host D- | GPIO1 through 22 ohms |
| Display I2C1 SDA | GPIO2 |
| Display I2C1 SCL | GPIO3 |
| Rotary encoder A | GPIO4 |
| Rotary encoder B | GPIO5 |
| Diagnostic test pad | GPIO6 |
| Diagnostic test pad | GPIO7 |
| Left audio | GPIO26 / ADC0 |
| Right audio | GPIO27 / ADC1 |
| Rotary encoder push switch | GPIO8 |

Encoder A, encoder B, and the push switch use internal pull-up resistors and
share a digital-ground common. Revision A uses a Bourns
`PEC12R-4120F-S0012` (`C143801` in the JLCPCB/LCSC catalog): 12 physical
detents, 12 electrical pulses per revolution, a momentary push switch, and a
20 mm top-facing shaft. Bourns calls this a horizontal mount because the
encoder body lies flat on the PCB; the shaft itself points up. This variant has
no threaded panel bushing, so the enclosure must support the PCB near the
encoder rather than relying on a shaft nut to carry push force.

Firmware must produce one action per physical detent. Turning selects the
visualization, a short press cycles its color scheme, and holding for more than
800 ms opens the LCD menu. Revision A preserves the validated electrical
circuit directly: it does not add external pull-ups, resistor-capacitor
debounce networks, or other filtering to the encoder signals.

GPIO2 through GPIO7 drove the initial parallel-LCD prototype. The validated
revision-A breadboard configuration now uses GPIO2 and GPIO3 as the native
I2C1 data and clock pins for the display port. GPIO4 and GPIO5 are frozen for
the rotary encoder. GPIO6 and GPIO7 terminate only at labeled test pads; they
are not part of a supported revision-A expansion interface.

## USB-C connectors — frozen

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

### Downstream current advertisement and attachment

Revision A advertises 1.5 A to the dedicated Moonlander-facing port. This does
not force the keyboard to draw 1.5 A; it communicates the current available at
that downstream port. The choice covers the measured approximately 0.67 A
Moonlander contribution at the accepted 175/255 all-white limit and avoids
advertising the traditional USB 2.0 500 mA limit while knowingly operating
above it.

The advertisement does not make LanderViz a universally compliant 1.5 A USB
source. Its own upstream source remains the frozen, measured powered-hub data
port, and the known mismatch with the upstream 500 mA USB declaration remains
documented below. Do not describe direct laptop-port or arbitrary-hub operation
as supported.

**VBUS** is the 5 V USB bus-power rail. A TPS25821 USB Type-C source controller
monitors both downstream CC pins and enables VBUS only after detecting a sink.
It provides the 1.5 A CC advertisement, attachment-controlled VBUS discharge,
and a 1.7 A nominal internal current limit. The TPS25821 is the USB 2.0-oriented
variant without VCONN; revision A neither uses SuperSpeed nor requires an
electronically marked cable for its 1.5 A link.

## Power architecture — frozen

Revision A is supported from a data port on the powered Anker 60 W hub through
a USB-A-to-USB-C data cable. It is not specified for a laptop port, an
unpowered hub, a charging-only PowerIQ port, or an arbitrary USB source. This
is a deliberate revision-A constraint based on the proven system, not a claim
of universal USB bus-power compliance.

The intended path is:

```text
upstream USB-C VBUS
    -> input resettable overcurrent protection
    -> protected 5 V rail and 470 uF bulk reservoir
    -> TPS25821 attachment-controlled, current-limited VBUS path
    -> Moonlander USB-C VBUS
    -> less than 10 uF local output bypass near the downstream connector

upstream USB-C VBUS
    -> protected 5 V rail
    -> TLV75533PDBVR 3.3 V linear regulator
    -> RP2354A and 3.3 V logic

upstream USB-C VBUS
    -> protected 5 V rail
    -> separately protected 5 V display output
```

The display output is sized only for revision A's 5 V PCF8574T-backed 16x2
character LCD. Vacuum fluorescent displays are not supported or included in
the revision-A power budget.

The Pro Micro underside-VBUS connection was a breadboard expedient and does
not appear on this board. Retain approximately 470 microfarads of bulk
capacitance on the protected 5 V rail before the TPS25821 switch. The
breadboard placed this capacitor directly across the always-powered downstream
connector, where it buffered LED and startup load steps. On an
attachment-controlled Type-C port, placing it after the switch would instead
create a large inrush current every time VBUS turns on. Follow the TPS25821
guidance by using less than 10 microfarads directly on its switched output.
The upstream reservoir still supports downstream load steps through the
switch's low-resistance power path. Never power the Moonlander from a second
supply at the same time.

The 3.3 V rail uses the fixed-output TLV75533PDBVR 500 mA low-dropout linear
regulator. The Moonlander and LCD remain on 5 V, so this regulator supplies
only the RP2354A and low-current 3.3 V circuitry. A linear regulator is chosen
over a switching converter for revision A because it needs fewer components
and introduces no additional switching node beside the analog audio frontend.
Verify its temperature during assembled-board bring-up.

The input resettable fuse must be able to hold the advertised 1.5 A across the
specified operating-temperature range; do not pair a 1.1 A hold-current part
with the 1.5 A advertisement. The downstream TPS25821 provides faster and more
precise Moonlander-port fault limiting. The input device protects the board's
other 5 V branches and wiring from sustained faults. Schematic capture uses the
stocked LUTE `1206L200/16NR` (`C22374899`), a 2 A-hold polymer
positive-temperature-coefficient (PTC) resettable fuse. Its temperature
derating remains a release check; do not silently substitute a lower
hold-current part.

Component voltage/current ratings should include sensible margin, but the
protection threshold and USB current advertisement must be based on the real
available current rather than a nominal “1.5 A class” goal.

### Completed power-budget measurement

The revision-A breadboard power characterization is complete. The FNIRSI
FNB58 logged the entire supported system between the powered Anker hub and
LanderViz at approximately 100 samples per second. These measurements freeze
the 175/255 brightness cap and provide the sizing basis for the PCB power
path. No additional breadboard power test is required before schematic
capture.

### RP2040 breadboard baseline — 2026-10-22

The FNB58 sampled the complete system at approximately 100 samples per second
between the powered Anker hub and LanderViz. These are upstream VBUS
measurements; they do not measure the voltage after the prototype's underside
VBUS pad, breadboard contacts, jumpers, or downstream USB breakout.

| Case | Duration | Minimum VBUS | Average current | Peak current | Peak power |
|---|---:|---:|---:|---:|---:|
| Controller and serial LCD, Moonlander disconnected | 30 s | 5.210 V | 0.057 A | 0.057 A | 0.299 W |
| Moonlander connected, visualization idle | 30 s | 5.197 V | 0.320 A | 0.332 A | 1.725 W |
| FLOW with music | 60 s | 5.192 V | 0.354 A | 0.430 A | 2.234 W |
| Moonlander reconnection/startup | event | 5.192 V | — | 0.406 A | 2.106 W |
| All 72 keys white at 175/255 | 300 s | 5.174 V | 0.713 A | 0.724 A | 3.748 W |

The five-minute 175/255 white soak remained visually white and electrically
flat. Its first minute averaged 0.7121 A and its last minute averaged 0.7128 A,
with no meaningful thermal drift at the meter. This validates the existing
firmware cap of 175 with the frozen serial-display configuration. An earlier
parallel-display startup capture reached 0.505 A; retain that higher observed
startup peak when sizing the design rather than treating the lower repeat as a
new limit.

Higher settings are not usable on the prototype. All-white 200/255 eventually
shifted red/orange; 225 and 255 did so as well. The keyboard recovered white
only after the command was reduced to 64. Upstream VBUS remained stable, so
the observation establishes a downstream sustained-load limit but does not by
itself identify whether the limiting element was the Pro Micro VBUS path,
breadboard wiring, USB breakout, cable, or Moonlander. Do not use 200 as a
design target merely because it survived briefly during a stepped sweep.

The 0.724 A worst-case peak exceeds the firmware's current 500 mA upstream USB
declaration. Revision A accepts that known mismatch only within the frozen,
measured powered-hub configuration; it must not be described as universally
USB bus-power compliant. Size the switch, protection, traces, connectors, and
cable path with margin above 0.724 A. Repeating startup and the five-minute
175/255 white soak on the assembled PCB is a board bring-up acceptance test,
not another prerequisite for schematic capture.

**VFD** means vacuum fluorescent display. VFD support is deferred to revision
B or later because these displays can require substantially more operating and
startup current than the revision-A character liquid-crystal display (LCD).
Adding one later requires a fresh power budget, interface-voltage review, and
connector review.

## Audio frontend — frozen

The 3.5 mm tip/ring/sleeve (TRS) stereo jack and both analog input stages are
on the main PCB. There is no audio codec in revision A and no pluggable analog
submodule.

Each channel uses the following passive circuit:

```text
                         3.3 V
                           |
                         22k 1%
                           |
TRS signal -- 10 uF -- bias node -- 10k 1% -- ADC node -- GPIO26 or GPIO27
                           |                     |  |
                         22k 1%               2.2nF BAT54S rail clamps
                           |                     |
                          GND                   GND
```

The channels must not share a bias node. If polarized capacitors are used,
their positive terminals face the bias nodes. The two matched 22 kilohm, 1%
resistors present an approximately 11 kilohm audio load and set the nominal
bias near half the 3.3 V rail. This is deliberately lower impedance than the
prototype's 100 kilohm pair because RP2354A specifies approximately 100
kilohms of ADC input impedance; a bias network of the same order would not
control the midpoint robustly.

The 10 microfarad coupling capacitor and 11 kilohm bias load form an
approximately 1.45 Hz high-pass response, preserving the bass range. The 10
kilohm series resistor and 2.2 nanofarad C0G capacitor form an approximately
7.2 kHz first-order low-pass response. **Anti-alias** means attenuating
frequencies above the approximately 7.8 kHz Nyquist limit so they do not appear
as false lower frequencies after sampling. This one-resistor/one-capacitor
stage gives modest rather than brick-wall rejection, while avoiding an active
operational-amplifier filter and preserving revision A's minimal hardware.

A BAT54S dual Schottky diode on each ADC node clamps positive and negative
fault excursions toward the 3.3 V and ground rails. The 10 kilohm series
resistor limits clamp and unpowered-input current. Schematic capture uses the
Diodes Incorporated `BAT54S-7-F` (`C83574`) in SOT-23; assembly availability
and leakage specifications remain bill-of-materials release checks.

Firmware currently adds 8x, or approximately +18 decibels (dB), visualization
gain because the proven passive mixer attenuates its output. Clip indication is
based on the physical ADC input before that gain. The PCB filter was not part
of the breadboard response, so assembled-board bring-up must compare its bass,
high-frequency texture, silence calibration, and clip behavior with the
accepted prototype before firmware constants are retuned.

Keep ADC nodes and input traces short and away from USB data, display clocks,
and Moonlander/display power. Add labeled 2 mm surface test pads for left ADC,
right ADC, both bias nodes, 3.3 V, 5 V, and ground. J3 is the CUI
`SJ1-3523N` (`C20182914`) with KiCad's matching horizontal footprint. C26 and
C27 are Honor Elec `RVT1C100M0405` (`C3357`) polarized 10 microfarad, 16 V
surface-mount electrolytics; their positive terminals face the biased ADC
nodes. Placement and polarity review remain release checks, but these are no
longer unassigned mechanical blockers.

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

The main board uses a keyed, right-angle JST
`S4B-PH-K-S(LF)(SN)` (`C157926`) four-position, 2.0 mm-pitch connector with
this frozen pinout. JLCPCB wave-solders the through-hole connector:

| Pin | Signal | External level |
|---:|---|---:|
| 1 | GND | 0 V |
| 2 | display power | 5 V |
| 3 | SDA | 5 V I2C |
| 4 | SCL | 5 V I2C |

The display cable is a hybrid harness, not a JST cable at both ends. Its board
end is a JST `PHR-4` female housing; its display end is one 1x4, 2.54 mm-pitch
female socket that slides over the backpack's four exposed male pins. LCSC's
custom-cable service can supply this harness with a minimum order of one and
ship it with the JLCPCB order. The conductors are straight-through to the
backpack's labeled `GND`, `VCC`, `SDA`, and `SCL` pins. Freeze the cable length
after display and enclosure placement; 100--200 mm is the current working
range, not a released dimension.

Loose 2.54 mm female Dupont jumper wires are acceptable for bench bring-up on
the LCD-backpack end, but they cannot plug directly onto the board's 2.0 mm
JST-PH connector. They also lack the keying and latch needed for the enclosed
device, so they do not replace the final harness.

The LCD-side socket is not keyed. Mark its ground end physically, use a black
ground conductor, and label the backpack connection. Confirm the board
footprint's pin-one orientation against the exact JST drawing; wire color is
an aid, not the source of truth.

RP2354A pins are not 5 V tolerant. The main board therefore includes
bidirectional, open-drain-compatible level translation between the internal
3.3 V I2C1 bus and the external 5 V display bus. **Open-drain** means a device
can pull a signal low but relies on a resistor to return it high, allowing
multiple I2C devices to share the wire safely.

Revision A reproduces the proven breakout topology with one BSS138 N-channel
**MOSFET** (metal-oxide-semiconductor field-effect transistor) per bus signal.
Each MOSFET's source is on the 3.3 V side, drain is on the 5 V display side,
and gate is tied to 3.3 V. R20 and R21 are populated 4.7
kilohm, 1% pull-ups from the internal SDA and SCL signals to 3.3 V. R22 and R23
are matching **DNP** (do not populate) footprints from the external signals to
the protected display 5 V rail. The accepted backpack already supplies the 5 V
pull-ups; the optional footprints allow a future compatible backpack without
pull-ups to be supported without a PCB revision. Do not populate them blindly:
parallel pull-ups reduce the resistance and increase the current a device must
sink when it drives the bus low.

The schematic uses the standard BSS138 gate/source/drain pin order with an
SOT-23 (small-outline transistor) footprint. Q1 and Q2 are frozen as onsemi
`BSS138LT1G` (`C82045`); stock remains a release-time check. Assembled-board bring-up must
verify that both lines idle at the correct side-specific voltage and that
communication remains reliable at 100 kHz before the translator is accepted.

Firmware probes the common PCF8574T address `0x27` and PCF8574A address `0x3F`,
then uses the verified backpack-to-LCD pin mapping. The delivered Teyleten
Robot module was validated at `0x27` with the common P0=RS, P2=enable,
P3=backlight, and P4 through P7=data mapping. Contrast, backlight-on operation,
100 kHz communication, differential screen updates, and approximately 30 FPS
keyboard output were demonstrated on the RP2040 breadboard. Deliberately test
backlight-off control during assembled-board bring-up. The display driver
becomes a replaceable firmware interface even though revision A ships only
this character-display backend.

## Digital expansion — deferred beyond revision A

Revision A has no digital expansion connector. A useful future audio-module
interface would need enough pins for **I2S** (Inter-IC Sound: data, bit clock,
and word-select clock), I2C, power, ground, and a spare signal. The two
otherwise-free GPIOs do not provide that interface cleanly, and reserving a
connector without a selected module would consume scarce board edge and imply
compatibility that has not been designed.

GPIO6 and GPIO7 remain accessible as small labeled test pads for diagnostics
or revision-A rework. They are not a supported peripheral interface. A future
codec, microphone, or broader display interface belongs in a separately
specified revision B or C.

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
- the display connector; and
- debug/test access.

Freeze connector direction, enclosure geometry, mounting holes, control
access, and cable clearances before routing. Do not enlarge the board or move
to four layers without reviewing the tradeoff. A two-layer board is acceptable
only if it can retain a continuous ground reference, clean USB routing, and
the required analog/power separation at the target size.

## Assembly and loose-part sourcing

Prefer JLCPCB-stocked manufacturer parts for both surface-mount and
through-hole components. JLCPCB assembles supported through-hole parts by wave
soldering; revision A therefore targets an assembled audio jack, rotary
encoder, and display header rather than asking the user to solder them after
delivery.

JLCPCB does not ship unused assembly inventory as loose parts. Any cable,
knob, or fallback component that is not assembled onto the board is purchased
from LCSC and combined with the JLCPCB shipment. Assembly stock and pricing
remain release-time checks; a catalog identifier documents the intended part
but does not guarantee future availability.

## Revision-A review gate

Electrical-rules check (ERC) and design-rules check (DRC) are the
computer-aided design tool's automated schematic and layout checks. They are
necessary but cannot prove the board will work. Before fabrication:

1. Commit and push the known-good breadboard milestone and record its commit.
2. Verify J4's physical pin-one orientation against its final footprint,
   validate the JST-PH-to-2.54-mm-female display cable, and verify GPIO6/GPIO7
   test-pad placement remains probe-accessible.
3. Verify the frozen TPS25821 1.5 A advertisement and VBUS implementation
   against the powered-hub-only source constraint and measured 0.724 A peak.
4. Verify assembly availability and temperature derating for the captured LUTE
   `1206L200/16NR` (`C22374899`) input fuse and TPD2EUSB30DRTR USB protection;
   verify the frozen TLV75533PDBVR, TPS25821, crystal, capacitors, and connector
   selections remain available.
5. Verify the captured audio protection and first-order anti-alias filter,
   then validate their response on the assembled revision before retuning
   firmware.
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
