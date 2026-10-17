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

`FLOW` consumes 12 overlapping triangular frequency bands spaced on the mel
scale from 60 Hz to 7.5 kHz. The mel scale approximates how humans perceive
pitch, while overlapping bands retain energy near a band boundary instead of
assigning it abruptly to one rectangular bucket. These bands are separate
from the legacy logarithmic peak bands used by the LCD and other modes.

Every analysis frame calculates the spectrum's weighted center and biases it
toward the strongest band. That position selects a fully saturated source hue
from a palette that moves from bass red through body magenta/blue and presence
cyan/green/amber. Per-band peak followers keep quieter regions relevant, while
a shared peak follower preserves whole-packet loudness changes. A kick
reinforces the lowest mel band. There is no spectral-flux gate or delayed hue
target: sustained notes and every new spectrum continue defining the source.

Every hue/value source packet is timestamped in a 96-entry history. The
history is a literal moving record of the audio: a transient produces a
distinct packet that travels outward. Audible packets retain a 20% brightness
floor after adaptive gain rather than being compressed into a nearly constant
brightness. Hue is unwrapped before storage and interpolated in hue space, so
neighboring packets remain saturated instead of becoming pastel RGB blends.

Each key receives a fractional ribbon coordinate derived from its outward
distance and row. One column represents 110 milliseconds of history, and each
successive row is offset by 0.11 column (about 12 milliseconds). The two halves
use opposite row phase.

The renderer samples five nearby ribbon positions at offsets
`-0.70, -0.35, 0, +0.35, +0.70`, weighted `1, 4, 6, 4, 1`. This spatial filter
spreads a hard color boundary across multiple keys. Distance applies a small
3% per-column fade. FLOW adds no periodic carrier or other autonomous motion;
changes in the ribbon come from its recorded audio packets.

Tests should lock down behavior rather than tuning accidents: fractional row
coordinates, broad transitions at sharp source boundaries, frame-independent
propagation, mel-band placement and overlap, sustained content, saturation,
loudness dynamics, noise gating, and the absence of unrelated motion.

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
