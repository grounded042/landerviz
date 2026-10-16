# LanderViz PCB specification

Status: reconciled design basis, 2026-10-15.

LanderViz is a standalone audio visualizer for a ZSA Moonlander Mark I,
hardware revision A. It sits between the keyboard and the Mac, samples stereo
line audio from a mixer, performs a per-channel FFT on an RP2040, and sends
per-key RGB frames to custom Moonlander firmware. No visualization software
runs on the Mac.

The breadboard prototype validates the end-to-end architecture. The repository
at <https://github.com/grounded042/landerviz> is the behavioral, protocol,
pin-map, and wiring reference after the current breadboard milestone is
committed and pushed. Record that commit in the PCB release notes before
ordering boards.

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
- ordinary HID keyboard reports forwarded to macOS;
- per-key RGB delivery over the vendor Raw HID protocol;
- eight 32-byte Raw HID packets per complete RGB frame;
- LCD status, mode-button input, silence detection, and all visualization
  modes;
- complete-system power from a data port on the powered Anker hub; and
- sustained delivery of approximately 30 complete RGB frames per second.

Thirty FPS is the current proven target, not 60 FPS. The Moonlander firmware's
26 ms LED flush interval imposes a ceiling of approximately 38 FPS, so 60 FPS
is not a revision-A requirement.

If RGB packets stop for 500 ms, the Moonlander returns to its own layer
lighting. Both firmwares cap brightness at 175/255.

The observed left/right level difference was traced upstream of LanderViz.
Intermittent mixer-powered ground noise is also external to the visualizer.
Windows enumeration remains unresolved; macOS is the frozen revision-A host
platform.

## Frozen topology

```text
Moonlander -- downstream USB --> RP2040 -- upstream USB --> powered hub --> Mac
                                      |
                                      +-- stereo audio input
                                      +-- external display port
                                      +-- mode button
```

The RP2040 is a USB host toward the keyboard and a USB device toward the Mac.
Native RP2040 USB is upstream. Pico-PIO-USB implements the downstream host on
GPIO0/D+ and GPIO1/D- through two 22 ohm series resistors. Typing is forwarded
to the Mac; visualization data travels in the opposite direction to the
Moonlander.

## Compute — frozen

Revision A uses a bare RP2040 in its 56-pin QFN package, assembled by JLCPCB,
not a Pro Micro module. **QFN** means quad-flat no-lead, a fine-pitch surface
mount package. It can be reworked with appropriate equipment, but the project
does not depend on hand assembly or repair of the RP2040; order spare boards.

Follow Raspberry Pi's
[Hardware design with RP2040](https://datasheets.raspberrypi.com/rp2040/hardware-design-with-rp2040.pdf)
reference design and checklist. The minimum design includes:

- a 12 MHz crystal and its correctly calculated load capacitors;
- supported external QSPI flash, because RP2040 has no internal program flash;
- a 5 V-to-3.3 V regulator;
- all required local supply and core-regulator decoupling capacitors;
- the required ADC supply/reference treatment;
- 27 ohm series resistors on native upstream USB D+ and D-;
- BOOTSEL and reset/RUN controls; and
- SWD debug pads plus 5 V, 3.3 V, and ground test points.

**QSPI** is the four-data-line serial interface used for external program
flash. **SWD** is Serial Wire Debug, the RP2040 programming and debug
interface.

W25Q128JV is a candidate flash, not a frozen selection. Choose the smallest
supported capacity that comfortably fits the firmware and is available for
assembly. Verify boot-ROM/Pico SDK support, package footprint, JLCPCB/LCSC
stock, and lifecycle at BOM release. JLCPCB part number `C2761095` is a
procurement hint for RP2040, not a permanent design identifier; verify it at
order time.

The visualization, analysis, USB-bridge, and Moonlander protocol code carries
over. Firmware is not literally unchanged: the raw-chip board target, any pin
assignment changes, the display driver, and board-specific initialization must
be implemented and tested.

## GPIO map

These assignments are frozen:

| Function | RP2040 pin |
|---|---:|
| PIO-USB host D+ | GPIO0 through 22 ohms |
| PIO-USB host D- | GPIO1 through 22 ohms |
| Left audio | GPIO26 / ADC0 |
| Right audio | GPIO27 / ADC1 |
| Mode button | GPIO8 |

The mode button uses the internal pull-up resistor and approximately 30 ms
debounce. A short press selects the next mode; holding for more than 800 ms
selects the previous mode.

GPIO2 through GPIO7 drove the prototype's parallel LCD. They are available for
the external serial display and expansion interfaces, but their final mapping
is open until the connector pinout is frozen.

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
after reserving power for the RP2040, audio frontend, and display.

Choose the downstream CC pull-ups only after power measurements and the
upstream source contract are known. Default-current advertisement is the safe
baseline for the proven USB-A-to-USB-C hub setup. If 1.5 A advertisement is a
requirement, the upstream Type-C source and the board must both establish and
support that budget without relying on USB Power Delivery.

The exact downstream attachment and VBUS switching circuit is open. Revision A
should use current-limited, protected VBUS switching if the size and component
budget permit it. Document any deliberate departure from standards-compliant
attach-controlled VBUS before schematic approval.

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
    -> RP2040 and 3.3 V logic

upstream USB-C VBUS
    -> protected/switched 5 V display output
```

The Pro Micro underside-VBUS connection was a breadboard expedient and does
not appear on this board. Retain approximately 470 microfarads of bulk
capacitance close to the downstream connector unless measurement supports a
different value. Never power the Moonlander from a second supply at the same
time.

Component voltage/current ratings should include sensible margin, but the
protection threshold and USB current advertisement must be based on the real
available current rather than a nominal “1.5 A class” goal.

### Power-budget measurement — order blocker

Use a USB power meter to record:

- the PCB without the Moonlander or display;
- the Moonlander connected with visualization idle;
- typical visualization current;
- the worst observed current at `BRIGHTNESS_CAP`;
- downstream VBUS voltage during Moonlander startup and the worst LED
  transition;
- each supported display at typical and maximum brightness; and
- startup/inrush current with the Moonlander and target display connected.

The prototype's approximately 4.99 V downstream measurement demonstrates
feasibility, not the complete power budget.

The Noritake CU16029-UW1J is especially important here. Its
[official datasheet](https://www.noritake-itron.com/site2017/images/Specs/VFD/57CLEM/CU16029-UW1J/CU16029-UW1J_e01-b1.pdf)
specifies 340 mA typical and 450 mA maximum with the display on, and warns that
startup current may be twice its usual value. A 500 mA upstream budget cannot
simultaneously guarantee that display, the Moonlander, and the controller.
This conflict must be resolved before the VFD is declared a supported
bus-powered configuration.

## Audio frontend — frozen topology, open protection/filter values

The 3.5 mm TRS jack and both analog input stages are on the main PCB. There is
no audio codec in revision A and no pluggable analog submodule.

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
resistors. Firmware currently adds 8x, or approximately +18 dB, visualization
gain because the proven passive mixer attenuates its output. Clip indication
is based on the physical ADC input before that gain.

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
- Place the analog frontend together near the RP2040 ADC pins.
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

## External display — frozen architecture, open connector

The display is external and pluggable; the prototype's parallel QC1602A is not
mounted on the main board. The display connection is serial so displays can be
changed without revising the main PCB.

Candidate displays include:

- Noritake CU-series character VFD modules using their synchronous serial
  interface;
- an HD44780-compatible character display through a PCF8574 I2C backpack;
- Noritake GU graphic VFD modules; and
- 128x64 OLED modules using I2C or SPI.

**I2C** is a two-signal shared serial bus. **SPI** is a faster clocked serial
interface with separate data and device-select signals.

The connector and pinout remain open. The preferred direction is a keyed port
carrying:

- 5 V;
- 3.3 V;
- ground;
- I2C data and clock;
- SPI data-out, data-in, clock, and chip-select/strobe; and
- optional reset or one spare GPIO if the pin/size budget permits.

A 128x64 one-bit framebuffer is 1024 bytes. A full transfer consumes roughly
20 ms at 400 kHz I2C before protocol overhead, but approximately 1 ms over a
typical multi-megahertz SPI link. Providing both buses avoids forcing a slow
graphic-display path later.

The CU16029-UW1J is not ordinary SPI even though it is synchronous serial. Its
six-pin serial connector uses VCC, bidirectional SI/SO, GND, STB, SCK, and an
optional reset pin. The datasheet specifies a 5 V supply and a minimum logic
high of 0.7 × VCC on SI/SO, so 3.3 V drive is not guaranteed; SI/SO output can
also exceed the RP2040's safe input voltage. The final port therefore needs an
explicit 3.3 V/5 V level-translation strategy, or a documented write-only
interface with suitable unidirectional translation. SCK and STB have different
input thresholds, but one coherent level-shifting design is preferable to
depending on that asymmetry.

The VFD datasheet also advises a cable no longer than 30 cm and warns against
connecting or disconnecting it while powered. Capture those constraints in
the enclosure and operating documentation.

The display driver must become a replaceable firmware interface. Revision A
needs at least one implemented serial driver before the parallel LCD pins can
be considered truly released.

## Digital expansion footprint — frozen

Provide an unpopulated expansion footprint on the digital side for a future
codec or microphone module. It should expose:

- I2S data, bit clock, and word-select clock;
- I2C data and clock;
- 3.3 V;
- 5 V;
- ground; and
- at least one spare GPIO.

**I2S** is a clocked digital-audio bus. No analog audio crosses this connector.
The exact connector, GPIO assignment, and whether any signals can be shared
with the display port remain open. If this footprint prevents the main board
from meeting the size target, return for an explicit scope decision rather
than silently increasing board size.

## Mechanical target

The main board should be connector-limited and approximately 35–40 mm class.
That is a placement target, not yet a guaranteed outline. It must accommodate:

- two USB-C receptacles;
- one 3.5 mm TRS jack;
- the RP2040 support circuit;
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

ERC and DRC—the CAD tool's electrical-rule and design-rule checks—are
necessary but cannot prove the board will work. Before fabrication:

1. Commit and push the known-good breadboard milestone and record its commit.
2. Freeze the display and expansion connector pinouts and GPIO assignments.
3. Resolve the upstream budget, downstream CC advertisement, VBUS switch, and
   optional VFD power conflict.
4. Select the regulator, flash, crystal/load capacitors, USB protection, and
   assembly-supported packages.
5. Calculate or simulate the audio protection and anti-alias filter, then
   validate any material change to the proven input response.
6. Freeze the enclosure and connector geometry after a component placement
   study confirms the 35–40 mm target is credible.
7. Check every RP2040 support net against Raspberry Pi's hardware-design
   checklist, including crystal, flash, core supply, ADC supply, USB, BOOTSEL,
   and exposed-pad grounding.
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
