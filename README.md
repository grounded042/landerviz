# LanderViz

LanderViz is a stand-alone audio visualizer for a ZSA Moonlander Mark I. A
SparkFun Pro Micro RP2040 sits between the keyboard and the Mac, analyzes a
stereo audio pair, and streams 60 frames per second of per-key color to custom
Moonlander firmware. No visualizer program runs on the Mac.

The complete stereo prototype has been hardware validated: both audio channels
drive the display and keyboard independently, the Moonlander receives RGB
frames, and ordinary typing passes through to macOS. The Moonlander output is
specifically for **revision A**.

## Architecture

```text
                             ordinary keyboard/mouse/media reports
Moonlander -- downstream USB --> Pro Micro RP2040 -- upstream USB --> hub --> Mac
     ^                           |  host toward keyboard; device toward Mac
     |                           +-- stereo ADC + FFT
     +---------- RGB frames -----+-- LCD + mode button
```

- **HID** (Human Interface Device) is the standard USB keyboard/mouse protocol.
  TinyUSB makes LanderViz appear as a composite keyboard, mouse, media-key, and
  system-control device to the Mac and forwards the Moonlander's reports.
- **PIO** (Programmable Input/Output) is configurable digital hardware inside
  the RP2040. Pico-PIO-USB uses it to create the second USB connection on GPIO0
  and GPIO1, where LanderViz acts as the host for the real Moonlander.
- **Raw HID** is a vendor-defined HID data pipe. LanderViz owns that pipe and
  sends each RGB frame as eight 32-byte packets. The Moonlander publishes only
  complete frames, so a dropped packet cannot make a torn frame.
- **ADC** (analog-to-digital converter) samples biased left and right audio on
  GPIO26 and GPIO27. The RP2040 runs a 256-point **FFT** (fast Fourier
  transform) for each channel to split it into frequency bands.

The custom firmware deliberately removes ZSA's Oryx Raw HID module. Normal
typing, layers, macros, mouse keys, media keys, and N-key rollover remain; the
live Keymapp layout display and Oryx Live Training do not. Keymapp is not
required for normal use and remains optional as a firmware flashing tool.

If RGB packets stop for 500 ms, the keyboard returns to the layer lighting from
your Oryx layout. Both firmwares cap LED brightness at 175/255.

## Reproducible Nix builds

The flake pins the Raspberry Pi Pico software development kit, TinyUSB,
Pico-PIO-USB, ZSA QMK Firmware 25, and QMK's required submodules.
The generated Oryx keymap source is vendored as reviewable text under
`moonlander/oryx`; its source and refresh procedure are documented in
`moonlander/README.md`.

```sh
nix flake check
nix build .#rp2040 -o result-rp2040
nix build .#moonlander -o result-moonlander
```

Artifacts:

- `result-rp2040/landerviz.uf2` — Pro Micro RP2040 firmware
- `result-moonlander/zsa_moonlander_reva_landerviz.bin` — Moonlander revision-A
  firmware based on Oryx layout `z9wlG`, revision `ARDpK`

`nix develop` provides the pinned build tools for manual CMake work. The actual
build board name is `sparkfun_promicro`; the flake already supplies it.

Before flashing a Moonlander build, confirm the keyboard's hardware revision.
ZSA's command-line flasher can accept the local file:

```sh
nix run github:zsa/zapp -- flash result-moonlander/zsa_moonlander_reva_landerviz.bin
```

That command intentionally is not run by the build. Hold BOOT while connecting
the Pro Micro, then copy `landerviz.uf2` to the `RPI-RP2` drive to flash it.

## Minimal prototype parts

In addition to the parts already listed, the USB bridge needs:

- one USB-A female breakout, or a known-good female extension cable cut open;
- two 22 ohm resistors, one in each USB data wire;
- one bulk electrolytic capacitor, start with 470 microfarads rated for at
  least 6.3 V;
- a way to make reliable connections to the headerless boards: normally two
  12-pin 0.1-inch male headers for the Pro Micro and one 16-pin header for the
  LCD, soldered in place;
- one 3.5 mm TRS stereo jack breakout and a male-to-male TRS cable, four
  100 kilohm resistors, and two 10 microfarad coupling capacitors;
- one 10 kilohm trimmer for LCD contrast and one approximately 100 ohm LCD
  backlight resistor.

No larger controller or extra interface board is required. Keep the USB data
wires short; a solderless breadboard may be electrically marginal at USB full
speed, so first failures there do not automatically imply a firmware bug.

## Wiring

### USB bridge

The Pro Micro's onboard USB-C connector is the upstream/device connection to a
**data port** on the powered Anker hub. Never use a PowerIQ charging-only port.
The added USB-A female connector is the downstream/host connection for the
Moonlander. Use a known-good data cable: a charging-only USB cable can power the
Moonlander's startup LEDs while leaving LanderViz at `WAIT` indefinitely.

| USB-A female contact | Pro Micro connection |
|---|---|
| VBUS | backside `V` USB-power pad |
| D- | GPIO1 through 22 ohms |
| D+ | GPIO0 through 22 ohms |
| GND | GND |

**VBUS** (USB bus voltage) is the nominal 5 V supply. Use the backside `V` pad,
which is connected directly to USB-C VBUS. Do not power the Moonlander from the
`+` pin: that path is downstream of the board's 500 mA resettable fuse and
Schottky diode, so the RP2040, display, and Moonlander would share the fuse and
the Moonlander would receive about 4.8 V. Put the 470 microfarad bulk capacitor
between the USB-A VCC row and GND near the connector: capacitor positive to
VCC, negative stripe to GND. The firmware requests 500 mA from the upstream USB
hub and caps keyboard brightness to reduce LED current spikes. The prototype
held about 4.99 V at the downstream connector during bring-up, but that
measurement is not a guarantee for other hubs or wiring. Do not connect the
Moonlander to another power source at the same time, and do not add capacitors
to D+ or D-.

### QC1602A liquid-crystal display

The display uses 4-bit mode. **GPIO** means general-purpose input/output pin.

| LCD pin | Name | Connection |
|---:|---|---|
| 1 | VSS | GND |
| 2 | VDD | `+` (about 4.8 V) |
| 3 | VO | 10 kilohm trimmer wiper; trimmer ends to `+` and GND |
| 4 | RS | GPIO2 |
| 5 | R/W | GND |
| 6 | E | GPIO3 |
| 7–10 | D0–D3 | not connected |
| 11 | D4 | GPIO4 |
| 12 | D5 | GPIO5 |
| 13 | D6 | GPIO6 |
| 14 | D7 | GPIO7 |
| 15 | A / LED+ | `+` through approximately 100 ohms |
| 16 | K / LED- | GND |

The LCD is powered near 5 V but receives 3.3 V logic. That commonly works with
HD44780-compatible modules; test this display before considering any level
shifter, so the first build stays minimal.

Row 0 shows independent left and right VU bars (volume-unit-style level meters),
peak hold, and `!` on clipping. Row 1 shows the mode and measured frame rate.

### Mode button

Connect GPIO8 to one corner of the tactile switch and GND to the diagonally
opposite corner. Firmware enables the internal pull-up resistor and applies a
30 ms debounce. A short press selects the next mode; holding longer than 800 ms
selects the previous mode.

### Mixer stereo audio to ADC0 and ADC1

Line audio swings above and below 0 V, while the RP2040 ADC accepts only 0 to
3.3 V. Each channel gets its own two-resistor divider that biases its ADC node
to 1.65 V. Its coupling capacitor blocks direct current from the mixer. The
channels must not share a bias node, because that would mix them together.

```text
                         3.3 V                            3.3 V
                           |                                |
                         100k                             100k
                           |                                |
TRS tip/left --- 10 uF ----+--- GPIO26   TRS ring/right -- 10 uF --+--- GPIO27
                           |                                      |
                         100k                             100k
                           |                                |
TRS sleeve ----------------+------------- GND --------------+
```

For a polarized electrolytic coupling capacitor, put its positive lead toward
the 1.65 V ADC node and its negative lead toward the mixer jack. A non-polarized
capacitor works either way. Start with the mixer's output level low and raise it
until the display is useful without showing `!`.

The passive mixer used by the prototype attenuates its output, so firmware
applies a fixed `8.0x` (`+18 dB`) visualization gain before the FFT. Adjust
`AUDIO_INPUT_GAIN` in `main.cpp` if the source level changes. The LCD `!`
indicator still reports clipping at the physical ADC input, before this gain.
Analysis removes each sample frame's measured DC bias before calculating its
level, so normal tolerance around the nominal 1.65 V bias does not appear as
audio on the meters. Keyboard effects use a sensitive `-48 dBFS` logarithmic
curve. LCD metering and silence detection add a `-30` to `-24 dBFS` soft noise
gate, keeping the meter empty at the prototype's measured noise floor without
reducing normal keyboard animation. `dBFS` means decibels relative to the
ADC's full-scale input.

For the numbered five-pin stereo breakout selected for this prototype, the
reported mapping is pin 1 ground/sleeve, pin 2 right/ring, pins 3 and 4 jack
detection switches, and pin 5 left/tip. Confirm that mapping with continuity
mode before applying power, because generic breakout batches can change. Leave
pins 3 and 4 disconnected.

## Firmware behavior

The three v1 modes are:

- `SPECTRUM`: left audio drives the left keyboard half and right audio drives
  the right half; each half has bass at the outside and treble at the center,
  utility keys mirror the adjacent treble column, and thumbs pulse with bass;
- `PULSE`: whole-board kick pulse with a slowly rotating hue;
- `IDLE`: breathing blue wave, also used after five seconds of quiet.

The RP2040 targets 60 keyboard frames and 20 LCD refreshes per second.

## Safe bring-up order

1. Verify that the physical Moonlander is revision A. Do not infer this only
   from the downloaded filename.
2. Flash the custom Moonlander firmware while it is connected directly to the
   Mac. Test every layer plus ordinary, mouse, media, and system keys. The
   visualizer receiver will simply time out to normal layer colors.
3. Flash the RP2040 and power it from the hub with no downstream keyboard or
   breadboard circuit attached.
4. Remove power, mount the headers and breadboard wiring, then check the 5 V,
   3.3 V, ground, and USB wiring for shorts.
5. With the Moonlander disconnected, test the button and LCD separately.
6. Add the stereo audio inputs and begin with the mixer output level down.
7. Check the USB data and power wiring once more, connect the Moonlander last,
   and validate typing before enabling visualization frames.

Keep the unmodified Oryx `.bin` from the original download outside this
repository so reverting the keyboard is straightforward.
