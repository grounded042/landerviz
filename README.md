# LanderViz

LanderViz is a stand-alone audio visualizer for a ZSA Moonlander Mark I. A
SparkFun Pro Micro RP2040 sits between the keyboard and the Mac, analyzes a
stereo audio pair, and targets 30 frames per second of per-key color to custom
Moonlander firmware. No visualizer program runs on the Mac during stand-alone
operation; the optional Mac preview exists only for development.

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
  GPIO26 and GPIO27. The RP2040 uses ARM CMSIS-DSP's 256-point Q15 **FFT**
  (fast Fourier transform) to split audio into frequency bands. FLOW averages
  left and right before one FFT; SPECTRUM retains one FFT per channel. Q15 is a
  signed fixed-point number format with 15 fractional bits.

The custom firmware deliberately removes ZSA's Oryx Raw HID module. Normal
typing, layers, macros, mouse keys, media keys, and N-key rollover remain; the
live Keymapp layout display and Oryx Live Training do not. Keymapp is not
required for normal use and remains optional as a firmware flashing tool.

If RGB packets stop for 500 ms, the keyboard returns to the layer lighting from
your Oryx layout. Both firmwares cap LED brightness at 175/255.

## Reproducible Nix builds

The flake pins the Raspberry Pi Pico software development kit, TinyUSB,
Pico-PIO-USB, ARM CMSIS-DSP, ZSA QMK Firmware 25, and QMK's required
submodules.
The generated Oryx keymap source is vendored as reviewable text under
`moonlander/oryx`; its source and refresh procedure are documented in
`moonlander/README.md`.

```sh
nix flake check
nix build .#rp2040 -o result-rp2040
nix build .#moonlander -o result-moonlander
nix build .#preview -o result-preview
nix build .#power-capture -o result-power-capture
nix build .#rp2040-power-test -o result-rp2040-power-test
nix build .#moonlander-power-test -o result-moonlander-power-test
```

Artifacts:

- `result-rp2040/landerviz.uf2` — Pro Micro RP2040 firmware
- `result-moonlander/zsa_moonlander_reva_landerviz.bin` — Moonlander revision-A
  firmware based on Oryx layout `z9wlG`, revision `ARDpK`
- `result-preview/bin/landerviz-preview` — development-only Mac visualizer
- `result-power-capture/bin/landerviz-power-capture` — FNIRSI FNB58 power
  measurement logger
- `result-rp2040-power-test/landerviz.uf2` — manual all-white power-test
  controller firmware
- `result-moonlander-power-test/zsa_moonlander_reva_landerviz.bin` — matching
  Moonlander revision-A diagnostic firmware with its receive cap raised to 255

`nix develop` provides the pinned build tools for manual CMake work. The actual
build board name is `sparkfun_promicro`; the flake already supplies it.

### USB power capture

The development-only power logger reads an FNIRSI FNB58 through its separate
micro-USB `PC` port while the meter remains inline between the powered hub and
LanderViz. **HID** (Human Interface Device) is used here only as the meter's
driver-free USB data transport. The tool reports the lowest measured USB bus
voltage and the average and highest current and power. It can also retain every
sample as a **CSV** (comma-separated values) file for later inspection.

```sh
nix run .#power-capture -- --duration 30
nix run .#power-capture -- --duration 30 --csv /tmp/landerviz-power.csv
```

Leave the FNB58 `PD` communication switch off. The logger implements only the
meter's initialization and measurement-poll commands; it has no fast-charge or
voltage-trigger feature. Its USB measurement protocol is based on the
independently documented [OpenFNB58 protocol](https://github.com/yeckel/OpenFNB58).

#### Controlled maximum-light test

The power-test artifacts are measurement tools, not production firmware. Flash
the diagnostic Moonlander image first, then the diagnostic RP2040 image. The
keyboard starts black after the USB bridge connects. A short press on the
external mode button advances all keys through white levels 0, 64, 128, 175,
200, 225, and 255; a press held longer than 800 milliseconds moves down one
level. The LCD always shows the commanded level. A controller reset returns to
black.

Run the FNB58 logger during the test and advance only after checking the prior
plateau. Stop if USB disconnects, the keyboard resets or flickers, or its supply
voltage becomes marginal. Restore the ordinary `moonlander` and `rp2040`
artifacts after measurement; they retain the production cap of 175.

The 2026-10-22 breadboard test validated 175 as the production limit. A
five-minute, all-white soak at 175 averaged 0.708 A and 3.663 W; current varied
by less than 2 mA and the lowest upstream VBUS reading was 5.174 V. White at
200 eventually shifted red/orange and recovered only after reducing the test
level to 64; 225 and 255 also failed. This is evidence of a sustained-load
limit somewhere in the prototype's downstream power path even though voltage
at the upstream FNB58 remained stable. Do not raise `BRIGHTNESS_CAP` above 175
on the breadboard. [PCB.md](PCB.md) records the complete measurement baseline
and the remaining USB-C power decision.

### Mac visualization preview

The development-only preview uses the same C++ audio analyzer, FLOW v3
components, and Moonlander LED mapping as the RP2040 firmware. It defaults to
the accepted FLOW v3 compositor; press `T` to cycle through v1, the frozen v2
baseline, and v3 at the same playback position. The comparison engines and
isolated labs remain preview-only.
See [FLOW v3 design contract](FLOW_V3_DESIGN.md) for the model and acceptance
criteria. FFmpeg decodes MP3, M4A, WAV, FLAC, and other common audio files;
SDL2 provides playback and draws all 72 simulated keys. Neither dependency is
included in the firmware.

```sh
nix run .#preview -- ~/Music/test-track.mp3
nix run .#preview -- --demo
nix run .#preview -- --demo-kicks
nix run .#preview -- --fps 25 ~/Music/test-track.mp3
nix run .#preview -- --travel-ms 110 ~/Music/test-track.mp3
nix run .#preview -- --trace /tmp/flow.csv ~/Music/test-track.mp3
nix run .#preview -- --analyze-only --trace /tmp/flow.csv ~/Music/test-track.mp3
```

The macOS package also includes a real application bundle. Opening it with no
arguments starts the demo; use `--args` to play a local audio file:

```sh
open -n result-preview/LanderViz.app
open -n result-preview/LanderViz.app --args --fps 30 "/absolute/path/song.mp3"
```

Use `T` to cycle FLOW v1/v2/v3 at the same playback position, left/right or
number keys 1–5 to change mode, space to pause, `R` to restart, `G` to compare
realistic light spill with the flat diagnostic view, `M` to cycle the v3
Combined/Energy/Attack/Texture/Mid/Bass views, `B` to compare Bass A/B inside
the Bass Lab, `E` to compare Combined with/without its capped Energy floor, `A` to
compare Combined with/without its accepted Attack shaping, `X` to compare
Combined with/without its bounded Texture hue influence, `[`/`]` to change the
simulated frame rate, comma/period to make FLOW
slower/faster in 10 ms increments, and escape or `Q` to quit. The title bar
shows the active FLOW engine, mode, frame rate, FLOW travel time, and lighting
style.
`--travel-ms N` sets the initial 50–1000 ms per-column travel time. `--frames
N` exits after N rendered frames and is intended for automated smoke tests.
`--flow-v1`, `--flow-v2`, or `--flow-v3` selects an engine at startup. The
default is v3. Within v3, `M` cycles the Combined, isolated Energy, isolated
Attack, isolated Texture, isolated Mid, and frozen Bass views;
`--combined-lab`, `--energy-lab`, `--attack-lab`, `--texture-lab`, `--mid-lab`,
and `--bass-lab` choose the startup view, with Combined as the default. In the
Bass Lab, `B` switches between the frozen direct bass envelope (Lab A) and
bounded slow-bed contrast expansion (Lab B); `--bass-a` and `--bass-b` choose
the bass version, with the accepted Lab B bass foundation as the default.
Combined always uses Bass B. Its Energy floor is enabled by default;
`--energy-floor` and `--no-energy-floor` select it at startup.
Combined's Attack shaping is also enabled by default; `--attack-shape` and
`--no-attack-shape` select it at startup. Attack shaping requires the Energy
floor integration, so disabling the floor displays the earlier no-floor
Combined baseline. Texture integration is enabled by default;
`--texture-surface` and `--no-texture-surface` select it at startup. It is
layered on the accepted Energy and Attack integration, so disabling either
shows that earlier baseline. `--trace
FILE.csv` records the pre-gain gate,
shared gain, per-channel energy, centroid, dominance, and low/mid/high onset
flux, adaptive thresholds, strengths, and trigger decisions once per analyzed
audio frame, plus the v3 bass and midrange level, absolute and contrast
centroids, contrast strength, base/target/rendered hue, and presence
diagnostics, the isolated Energy Lab raw, target, and smoothed levels, and
Attack Lab's raw and normalized positive-rise signals. Texture Lab adds its raw
high-frequency share, target, smoothed texture, and presence. Add
`--analyze-only` to process the complete track without opening a window or
playing audio; it requires `--trace` and an audio file and prints the three
accepted v2 onset counts when complete. FLOW v2 keeps its base color and
energy stereo, but merges per-band energy across channels for shared rhythmic
events so a panned hit launches one wavefront across both keyboard halves.
`--demo-kicks` plays an isolated 120 BPM kick train and reports detected versus
generated v1 kicks in the title bar. V1 kick packets are enabled only in that
diagnostic mode while the detector is being calibrated. V3 Attack Lab is a
separate continuous positive-rise diagnostic with no threshold, refractory
timer, or binary beat trigger. Accepted Combined uses that signal only to
tighten an existing ribbon edge; it cannot independently create light.

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

In `FLOW` and `SPECTRUM`, row 0 shows a mirrored stereo frequency spectrum:
bass is at the outside edges and treble meets at the center. In the other
modes, row 0 shows independent left and right VU bars (volume-unit-style level
meters), peak hold, and `!` on clipping. Row 1 shows the mode and completed USB
RGB frames per second. It counts only full eight-packet frames delivered to the
Moonlander, rather than visualizer loop calls.

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
reducing normal keyboard animation. Silence detection also requires three of
four consecutive analysis frames to clear that gate, so isolated noise spikes
do not prevent the aurora from starting. `dBFS` means decibels relative to the
ADC's full-scale input.

For the numbered five-pin stereo breakout selected for this prototype, the
reported mapping is pin 1 ground/sleeve, pin 2 right/ring, pins 3 and 4 jack
detection switches, and pin 5 left/tip. Confirm that mapping with continuity
mode before applying power, because generic breakout batches can change. Leave
pins 3 and 4 disconnected.

### Ground routing and noise checks

The prototype uses two breadboard ground rails to keep noisy USB, keyboard,
and LCD return current out of the sensitive audio wiring:

- The audio rail connects the TRS sleeve, the bottom 100 kilohm resistor of
  each bias divider, and one Pro Micro GND pin.
- The digital rail connects USB-A GND, bulk-capacitor negative, LCD VSS, LCD
  LED-, the contrast trimmer's ground end, the button, and the other Pro Micro
  GND pin.

Both Pro Micro GND pins are internally the same electrical node. This is
`star-ground` routing: the two current paths meet at the controller instead of
sharing a long breadboard rail. It does not electrically isolate the audio and
USB systems.

Solderless breadboard power rails are often split in the middle. With all power
removed, use the meter's continuity or lowest resistance range to verify every
ground point. First touch the probes together to learn their own resistance;
each ground-to-GND reading should be close to that value, and no ground rail
should read open. Never measure resistance on a powered circuit.

During bring-up, an audible high-pitched whine appeared only with the TRS cable
connected and cleared after unplugging and reconnecting the mixer's USB-C
power. If it returns, treat mixer power and the audio/USB ground path as the
first suspects. A short shielded cable can reduce radiated pickup, while a
stereo transformer ground-loop isolator can break a persistent shared-ground
loop; ordinary "directional" analog cables cannot provide that isolation.

## Firmware behavior

See [Visualization design principles](docs/visualization-design.md) for the
shared motion, smoothing, sampling, and validation rules used by every mode.

The five selectable modes are:

- `FLOW` (default): the accepted v3 compositor turns each analyzed audio frame
  into one timestamped music slice. FLOW averages the stereo waveform to mono
  before analysis so its single cohesive ribbon has more CPU time; independent
  left/right RMS loudness is still retained for metering and silence detection.
  Bass supplies a continuous body, midrange content selects its vivid hue,
  whole-spectrum energy supplies a capped visibility floor, positive spectral
  change sharpens its existing leading edge, and sustained high-frequency
  content adds a bounded hue shift. These properties travel together as one
  ribbon rather than independent rows or flashes. Adjacent slices and
  fractional physical key coordinates are interpolated as the ribbon moves
  outward at roughly 110 ms per column;
- `SPECTRUM`: left audio drives the left keyboard half and right audio drives
  the right half; each half has bass at the outside and treble at the center,
  utility keys mirror the adjacent treble column, and thumbs pulse with bass;
- `PULSE`: whole-board kick pulse with a slowly rotating hue;
- `VU`: independent left/right meters that fill each keyboard half from green
  at the bottom to red at the top, with partial-row brightness and smoothed
  attack/release;
- `IDLE`: slowly drifting blue, teal, green, and violet aurora, also used after
  five seconds of quiet.

The RP2040 targets 30 complete keyboard frames and 10 LCD refreshes per second.
It retains 256 audio samples per FFT for frequency resolution, but analyzes
every other captured window—about 30.5 analysis frames per second—to leave the
USB and LED renderer more CPU time. Decimation happens when capture windows
complete, so intentionally skipped windows never occupy the consumer buffer or
inflate the missed-window counter. FLOW services upstream USB after its mono
FFT; stereo modes also service it between channels. The FLOW renderer services
USB between small coordinate batches. These yields do not change the analyzed
values or rendered key colors. Time-domain mono can cancel rare material whose
left and right channels have opposite phase; that is an accepted trade-off for
the measured CPU headroom.
The Moonlander's LED-driver flush interval is 26 ms (a ceiling of about 38
frames per second), so 30 Hz leaves timing margin while exceeding the roughly
27 Hz motion target.

FLOW's vivid OKLCH colors (a perceptual lightness/chroma/hue color model) use a
generated, interpolated **LUT** (lookup table) stored in flash. This preserves
the preview palette without running trigonometry and gamut searches for every
key on the RP2040. Regenerate it with:

```sh
python3 tools/generate_flow_v3_color_lut.py src/flow_v3_color_lut.inc
```

While FLOW is selected, LCD row 1 rotates every two seconds among the normal
mode/frame-rate page and three steady-state maximum profiling pages. The
counters reset once, one second after USB enumeration completes, so startup
does not permanently contaminate them. Times are shown in tenths of a
millisecond and cap at `999`: `A` is total audio analysis time, `F` is its
FFT/band-analysis portion, `E` is v3 feature extraction, `R` is render/submit
time, `M` is genuinely missed due analysis windows, `D` is the upstream USB
device-service gap, `H` is the downstream USB host-service gap, and `K` is
keyboard-report forwarding latency. These counters exist to validate the
firmware on the real RP2040; desktop preview timing is not a substitute.

FLOW keeps the fixed 8x (+18 dB) passive-mixer compensation. After the FFT, it
subtracts calibrated noise power per frequency band and applies a shared
adaptive gain before any FLOW feature extraction. Gain scaling commutes with
the linear FFT; placing it here allows spectral noise removal first. The
adaptive stage targets a useful program level, reduces gain over sustained
loud passages, recovers slowly for quiet tracks, and cannot increase gain
while the noise-relative gate is closed. A single beat therefore remains part
of the music rather than being immediately normalized away.

After calibration, the music gate also accepts sustained energy in at least
two overlapping mel-frequency bands above their individual noise floors. This
keeps genuinely quiet passages alive even when their total RMS level is near
the broadband floor. Opening still requires three consecutive analysis frames,
and a half-second close hold bridges gaps between notes without leaving paused
audio visibly active.

To calibrate the current input hardware, pause audio, hold the external mode
button while LanderViz boots, and keep holding it through the 1.2-second splash.
Release when the LCD says `RELEASE BUTTON`; `SILENCE` then counts through a
three-second measurement while USB service continues. `CALIBRATION OK` reports
the accepted broadband floor. `CALIBRATION BAD` means the input varied too much
and should be retried with audio paused. Calibration is deliberately RAM-only
in this validation build and therefore does not survive a reboot.

For audio-input calibration, set `AUDIO_CALIBRATION_DISPLAY` to `1` in
`main.cpp`. The three profiling pages are temporarily replaced with four
diagnostic pages:

- `RMS L-36 R-38`: the largest pre-gain RMS level in the latest one-second
  measurement window, in dBFS;
- `PEAK L-18 R-20`: the largest pre-gain sample peak in that window; and
- `GAIN 6.3 N-51O`: adaptive gain, calibrated physical-input noise floor, and
  whether its gate is open (`O`) or closed (`C`); and
- `B123>456 M000`: the one-second maximum FLOW raw bass and rendered body on a
  0–999 scale, followed by missed analysis windows.

The input readings divide out `AUDIO_INPUT_GAIN`, so they describe the voltage
that actually reached the ADC rather than either visualization gain. Wait at
least two seconds after changing volume, then record the typical readings over
the same repeatable passage. A full five-page rotation takes ten seconds. Set
`AUDIO_CALIBRATION_DISPLAY` back to `0` to restore the normal performance
pages.

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
