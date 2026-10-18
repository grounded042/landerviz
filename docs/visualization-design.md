# Visualization design principles

The Moonlander is a sparse display: its 72 individually lit keys are arranged
in irregular rows, with only six main-grid columns on each half. A smooth
effect therefore cannot treat an LED (light-emitting diode) index as if it were
a pixel in a dense screen.

These rules are the default for every LanderViz mode. A mode may deliberately
break one, but that should be an explicit visual choice.

## Render a field, then sample it

1. **Separate analysis from rendering.** Audio analysis creates slowly varying
   control signals. The renderer evaluates the current animation at its own
   timestamp. Never advance motion by one fixed step per rendered frame.
2. **Define a continuous two-dimensional field.** Compute color and brightness
   for fractional physical coordinates, then sample that field at each key's
   location. Do not assign one state to a whole hardware column.
3. **Stagger neighboring keys.** Add a small row-dependent phase so a boundary
   crosses adjacent keys at different times. Mirror the phase on the two
   keyboard halves when that reinforces the composition.
4. **Band-limit before sampling.** Spatial low-pass filtering—averaging nearby
   field positions to remove changes too narrow for the key spacing—must make
   a moving boundary wider than one key. This produces a real handoff: the old
   key fades while the next key rises.
5. **Use time-based smoothing.** Attack, release, decay, and interpolation use
   elapsed milliseconds, not frame counts. The apparent speed should remain
   stable at 24, 25, or 30 frames per second.
6. **Provide more than one motion cue.** Hue alone disappears when neighboring
   source colors are similar. Pair it with a restrained traveling brightness
   or shape signal, while preserving the audio-driven color as the main event.
7. **Preserve musical dynamics.** Prefer one shared adaptive gain and an
   explicit noise gate. Per-band normalization can make silence move and turn
   every frequency band into the same apparent loudness.
8. **Respect the actual output.** Preview at the device frame rate, map utility
   and thumb keys by their physical positions, apply `BRIGHTNESS_CAP` last, and
   check the result on the keyboard before tuning around the simulator.

## Frame-time budget

The target is 30 complete keyboard frames per second; 27–30 is acceptable on
hardware. The LCD count is end-to-end: it advances only after all eight Raw HID
(human interface device) packets for a color frame reach the Moonlander. A
reading below 27 is a performance regression even when the local render loop
is scheduled at 30.

Effects must not perform a linear history search or a transcendental function
such as sine, cosine, or `pow` separately for every key. Prefer indexed or
binary history lookup, small interpolated lookup tables, and values shared by
an entire frame. Cache invariant digital signal processing (DSP) data, such as
the Fast Fourier Transform window, rather than regenerating it per channel and
frame. Validate the final result on the RP2040 because a desktop benchmark does
not model its software floating-point cost or USB transport.

## FLOW implementation

`FLOW` v3 consumes both the legacy low-frequency FFT regions and 12 overlapping
triangular frequency bands spaced on the mel scale. The mel scale approximates
how humans perceive pitch, while overlapping bands retain energy near a band
boundary instead of assigning it abruptly to one rectangular bucket.

Every analysis frame produces one timestamped music slice. Five continuous,
song-independent signals are extracted and transported together:

- low-frequency energy supplies the ribbon body;
- midrange spectral position supplies hue;
- whole-spectrum energy supplies a capped visibility floor;
- positive multi-band spectral change sharpens the body's existing edge; and
- sustained high-frequency share adds a bounded hue shift.

None of these signals renders an independent lane, row, flash, or overlay.
Bass owns the material's body and perceived lightness; the other signals shape
or color that same material. The reference preview merges stereo channels by
maximum where a shared feature is needed. Revision-A firmware instead averages
the time-domain samples and performs one mono FFT, an accepted CPU trade-off
that leaves independent stereo FFTs available to SPECTRUM. Rare opposite-phase
content can cancel in FLOW's mono input.

Each property keeps a 128-slice timestamped history. Rendering uses the audio
window midpoint rather than analysis-completion time, and a key samples the
history at its fractional outward coordinate. One column represents 110
milliseconds. Neighboring timestamps are interpolated, rows are slightly
staggered, and a three-point spatial blur spreads a boundary across adjacent
positions. Attack can only tighten that existing blur; it cannot create light.

Vivid output is generated at the perceived lightness of the accepted bass
curve. The desktop implementation's OKLCH gamut search is precomputed into an
interpolated lookup table for firmware, avoiding per-key trigonometry while
retaining equal-lightness hues. Distance applies a small 2.5% per-column fade.
FLOW adds no periodic carrier, randomness, or autonomous brightness motion.

Tests lock down the five feature contracts, reference stereo merge, deployed
mono preparation (including its accepted cancellation behavior), continuous
temporal and spatial interpolation, equal-lightness color, close repeated
rises, sustained content, noise gating, and the absence of independent layers.
See the [FLOW v3 design contract](../FLOW_V3_DESIGN.md) for exact accepted
bounds and hardware-validation result.

## References

The model follows techniques used by established keyboard and LED visualizers:

- [QMK RGB Matrix](https://github.com/qmk/qmk_firmware/blob/master/docs/features/rgb_matrix.md)
  evaluates effects from physical key coordinates and time.
- [OpenRGB Effects Plugin](https://github.com/DLdota/OpenRGBEffectsPlugin)
  renders dense virtual gradients or images before mapping them to device LEDs.
- [KeyboardVisualizer](https://github.com/CalcProgrammer1/KeyboardVisualizer)
  uses spatial averaging, temporal filtering, and decay for sparse devices.
- [Audio Reactive LED Strip](https://github.com/scottlawsonbc/audio-reactive-led-strip)
  injects each frame's low-, mid-, and high-frequency energy as a new RGB
  packet in an outward-scrolling history.
- [QMK Riverflow](https://github.com/qmk/qmk_firmware/blob/master/quantum/rgb_matrix/animations/riverflow_anim.h)
  adds a traveling brightness wave to make motion readable.

These are design references. LanderViz implements its own algorithms rather
than copying their source.
