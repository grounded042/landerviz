# FLOW v3 design contract

FLOW v3 is a preview-only experiment. It does not replace FLOW v2 or enter the
RP2040 firmware until its behavior is accepted in the desktop preview.

Status: Bass Lab B was accepted as the frozen bass foundation on 2026-10-17.
Lab A remains available as its comparison baseline. Mid Lab was accepted as
the frozen hue foundation on 2026-10-17. Combined Lab was accepted as their
integration foundation on 2026-10-17. Later feature work must preserve all
three tested behaviors. Energy Lab was accepted as the visibility-floor
foundation on 2026-10-17, and its capped floor was accepted in Combined on
2026-10-17. Attack Lab and its conservative spatial-sharpening integration
were accepted on 2026-10-17. Texture Lab and its bounded hue integration were
accepted on 2026-10-17.

FLOW v3 remains preview-only until the complete behavior is accepted.

## Goal

FLOW should read as one visual material moving with the music, not as a set of
spectrum meters or independent effects. A listener should be able to connect
visible changes to musical gestures without watching for a particular color or
keyboard row.

Every analyzed audio frame produces one **music slice**. A slice has a shared
position and carries related properties:

- `energy`: how present the whole slice is;
- `body`: sustained low-frequency weight;
- `attack`: a positive change that shapes the front of that same body;
- `hue` and `saturation`: the slice's musical color;
- `texture`: fine detail contributed by higher frequencies.

Those properties must combine before rendering. Bass, mids, and highs must not
be rendered as separate lanes, flashes, or overlays. The renderer moves and
interpolates the resulting slices as one continuous ribbon.

## Musical model

The finished model uses three timescales:

- Fast state follows attacks and articulation.
- Medium state follows groove, note bodies, and riffs.
- Slow state follows phrase energy and palette context.

The first v3 lab intentionally isolates bass so its timing can be judged. It
renders only the continuous bass envelope; attack shaping is deferred until
the envelope itself is accepted:

- The three lowest FFT (fast Fourier transform) regions, approximately
  61–244 Hz, supply bass energy. This is substantially lower and narrower than
  v2's first onset group, which extended into the midrange.
- Left and right channels merge with a maximum, not a waveform sum, so a
  panned hit remains visible and opposite phases cannot cancel it.
- Continuous bass energy creates `body`.
- One fixed saturated diagnostic color removes palette motion as a confounding
  variable.
- There is no onset threshold, attack overlay, hue modulation, or visual flash
  in this lab. A smoothed body envelope is the only rendered signal.

The preview maintains two bass envelopes in parallel. Lab A is the frozen
direct envelope. Lab B subtracts a fixed, bounded portion of a slow bass bed,
remaps the remaining headroom, and retains at least 45% of Lab A's continuous
body. B also gives small frame-to-frame changes a slower response while large
rises and falls retain A's response speed. The response rate changes
continuously with delta size, so this is contrast expansion and micro-motion
smoothing, not beat detection: it has no trigger threshold and cannot create a
body where the input has none. Press `B` to compare A and B at the same
playback position.

This bass-only lab is test isolation, not a bass-owned architecture. Later
slices will obtain primary hue and gesture from midrange musical content, slow
palette context from the whole spectrum, and bounded texture from high
frequencies while retaining the same slice and transport model.

## Mid Lab contract

Mid Lab isolates approximately 396 Hz–2.6 kHz. It ignores bass, cymbal/high-
frequency energy, and audio loudness after a soft presence fade. The midrange
spectral center controls only hue; it cannot change brightness.

- Colors are generated in OKLCH (Oklab lightness, chroma, and hue) at fixed
  perceptual lightness and the highest safe chroma for the selected hue.
- Hue is smoothed and transported continuously. RGB interpolation is not used,
  so transitions do not create muddy pastel colors.
- One fixed, song-independent log-frequency palette centered at 850 Hz
  expands the range common in the six reference tracks. There is no per-song
  normalization.
- A slow baseline follows each midrange mel band. Positive spectral change
  above that baseline supplies a bounded directional hue accent: newly
  prominent higher-frequency content can only pull the base hue upward, and
  lower-frequency content can only pull it downward. The accent decays back to
  the absolute spectral-center hue under sustained content. It is not an onset
  trigger and cannot change brightness.
- Equal-lightness tests cover the complete hue circle both before and after
  the preview's 175/255 brightness cap.
- Press `M` to compare the isolated mid lab with the frozen bass lab at the
  same playback position.

## Combined Lab contract

Combined Lab samples Bass B and Mid Lab at the same audio timestamp and ribbon
coordinate, then renders one composed slice. It is not a crossfade between two
rendered effects:

- Bass B exclusively owns `body`, spatial smoothing, and perceived lightness.
- Mid Lab owns hue. Its presence fade blends from Bass Lab cyan only when no
  usable midrange color exists; it never changes lightness.
- The compositor measures the perceived lightness produced by the frozen Bass
  Lab value curve, then produces the most vivid in-gamut color at that same
  lightness and the Mid Lab hue.
- The two accepted histories remain independent and unchanged. Composition is
  stateless, making regressions attributable to either input or the compositor.
- Combined is the default v3 preview. Press `M` to cycle Combined, Energy,
  Attack, Texture, Mid, and Bass at the same playback position.

Combined's acceptance tests show that its body equals Bass B, its perceived
lightness is invariant across hue both before and after the 175/255 brightness
cap, and the frozen 39-kick Bass B regression still passes.

## Energy Lab contract

Energy Lab isolates a smooth whole-spectrum presence signal so bass-light
passages can remain visible without weakening Bass B:

- Stereo RMS (root mean square) level merges by maximum, so panning and phase
  cannot cancel the signal.
- The mapping is fixed and song-independent. It uses the existing soft noise
  gate and no automatic gain control.
- A 140 ms attack and 520 ms release follow phrase-level intensity without
  creating onset flashes or beat triggers.
- The diagnostic view uses one fixed hue. Energy controls only its continuous
  body, removing palette motion as a confounding variable.
- Press `M` to cycle Combined, Energy, Attack, Texture, Mid, and Bass at the
  same playback position.

The accepted integration maps Energy onto at most 22% of the body range
and renders with `max(bass_body, 0.22 * energy)`. This maximum is evaluated
only when converting the composed slice to light; the stored `body` remains
the exact Bass B value. Consequently, a bass peak above the floor is rendered
bit-for-bit like Combined without the floor, while bass-light passages retain
a dim version of the same ribbon. Press `E` to compare the accepted floor with
the no-floor baseline at the same playback position.

## Attack Lab contract

Attack Lab isolates fast musical articulation before it is allowed to shape
the accepted ribbon:

- All 12 mel-frequency bands contribute after fixed, song-independent log
  compression. Left and right merge by maximum per band so panning and phase
  cannot erase an attack.
- Each band maintains a fast envelope (25 ms attack, 45 ms release) and a slow
  envelope (190 ms attack, 300 ms release). Only the positive difference
  between fast and slow contributes; the aggregate is the root mean square
  across bands.
- The output is continuous and proportional. There is no threshold,
  refractory timer, binary beat trigger, tempo grid, or per-song
  normalization.
- Initializing on already-playing audio produces no startup pulse. Falling
  edges produce no attack. Sustained input decays toward zero, and a second
  rise 99 ms after the first still registers.
- The diagnostic renderer uses one fixed cyan hue and maps the attack signal
  to brightness. This intentionally exposes the signal; it is not the final
  visual mapping.
- Press `M` to compare Attack with the accepted Combined, Energy, Texture, Mid,
  and Bass views at the same playback position, or start with `--attack-lab`.

The accepted analysis cadence is approximately 33 ms. A roughly 16 ms analysis
cadence remains a controlled future experiment only if firmware validation
reveals latency that was not present in the preview.

## Attack integration contract

Combined samples Attack at the same audio timestamp and ribbon coordinate as
the accepted Bass, Energy, and Mid properties. It tightens the existing spatial
edge rather than rendering another effect:

- With zero Attack, output is bit-for-bit the accepted Energy-floor baseline.
- Attack continuously blends Bass B's accepted blurred body toward its
  unblurred body by at most 75%. The result always remains between those two
  body values.
- Attack cannot create body from silence, raise the Energy floor, choose hue,
  change saturation, or leave the range bounded by the accepted blurred body
  and its underlying unblurred body.
- There is no Attack-colored layer, dedicated lane, or independent brightness
  flash. All properties remain one transported ribbon.
- Attack shaping is enabled by default in Combined. Press `A` to compare it
  with the exact Energy-floor baseline at the same playback position.

The isolated lab showed occasional blink-like sharp transients because its
diagnostic deliberately mapped articulation straight to brightness. Those
flashes are not part of the accepted compositor mapping. The integrated effect
was accepted after subjective A/B checks across reference tracks with sparse
plucks, repeated bass hits, and dense low-frequency passages.

## Texture Lab contract

Texture Lab tests whether sustained high-frequency content can give the ribbon
a visible surface without becoming sparkle, noise, or another brightness
effect:

- The four highest mel-frequency bands, centered at approximately 3.2–6.1 kHz,
  contribute as a share of power across all 12 bands. Left and right merge by
  maximum per band before the ratio is calculated.
- The ratio is instantaneous spectral shape, not per-song normalization or
  automatic gain control. A fixed square-root curve with a 1.25 scale expands
  the naturally small high-frequency share into a visible texture range.
- A 70 ms rise and 220 ms fall preserve cymbal and guitar presence while
  suppressing frame-level movement. There is no positive-difference detector,
  threshold, refractory timer, random source, or beat trigger.
- The isolated view maps each slice from magenta through a broad 240-degree
  hue range while preserving one OKLCH lightness. Texture cannot brighten,
  dim, or create material.
- Texture is stored in each timestamped slice and moves with the ribbon. Every
  key in a grid column receives that slice's color; rows do not receive
  separate texture treatment and there are no independently twinkling keys.
- Low/mid-only input produces zero texture, stereo panning cannot erase it,
  and scaling the same spectral shape louder leaves the raw share unchanged.
- Start with `--texture-lab` or press `M` to compare it with the other isolated
  labs at the same playback position.

The diagnostic hue range demonstrates the accepted signal and equal-lightness
constraint; it is intentionally much broader than the Combined mapping.

## Texture integration contract

Combined samples Texture at the same timestamp and ribbon coordinate as the
accepted Bass, Mid, Energy, and Attack properties:

- Zero Texture is bit-for-bit the accepted Attack-shaped baseline.
- Texture can move the composed slice's hue forward by at most 32 degrees. It
  cannot alter body, Energy, Attack, saturation, or perceived lightness.
- There is no high-frequency lane, per-row treatment, sparkle, random source,
  or independent layer. The hue change travels as part of the same slice.
- Texture integration is enabled by default in Combined. Press `X` to compare
  it with the exact accepted Attack-shaped baseline at the same playback
  position.

The bounded integration was accepted after same-position A/B checks on Blue,
ID, Radioactive, When I'm Small, and Frontier. Its effect remains deliberately
subtle: high-frequency detail colors the existing material without becoming a
separate effect.

## Spatial and temporal rules

- Transport is based on audio timestamps, not render-frame counts.
- Movement is sub-key and continuously interpolated; a slice must fade between
  adjacent columns instead of disappearing from one and appearing on the next.
- Sustained input produces sustained material, not repeated onset events.
- A rising edge appears without waiting for a complete kick to finish.
- Two genuine close bass rises produce two visible crests when the intervening
  energy falls enough to distinguish them.
- Silence may decay the material, but it must not increase automatic gain or
  invent motion.

## First-lab acceptance criteria

1. A 74 Hz kick train produces one coherent traveling body per kick.
2. A held 74 Hz tone produces continuous body without retriggering or flashing.
3. Two separated rises inside 150 ms remain distinguishable.
4. A unilateral bass signal survives the stereo merge unchanged.
5. A mid-only signal does not masquerade as bass.
6. Rendering halfway between two slice timestamps blends their properties;
   it does not select either slice discretely.
7. The preview can switch among v1, the frozen v2 baseline, and v3 without
   restarting, so subjective comparisons use the same audio and clock.

## Boundaries

- No firmware integration or flash in this phase.
- No new hardware requirements.
- No dedicated bass row, half, or lane. A fixed color is permitted only in the
  isolated diagnostic and is not the final integrated palette.
- No per-song normalization that makes every recording look the same.
- No beat grid or tempo prediction in the first lab. Those are candidates only
  if continuous causal features cannot meet the timing criteria.

## Prior art and provenance

The architecture borrows the useful invariant shared by Scott Lawson's MIT-
licensed `audio-reactive-led-strip` scroll effect and other open visualizers:
audio energy enters a continuously shifted, decayed, and spatially blurred
field. It does not copy their palettes or assume that their frequency bands are
the final musical model. LedFx (GPL-3.0) and WLED are behavioral references
only; no code is copied from them. Kreo Hive keyboard visualizer work is tracked
from its upstream repository, `PushkarDesai-06/Kreo-Hive65-Rgb-Linux`, rather
than the `rjonwalcloud` fork.
