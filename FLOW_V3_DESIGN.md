# FLOW v3 design contract

FLOW v3 is a preview-only experiment. It does not replace FLOW v2 or enter the
RP2040 firmware until its behavior is accepted in the desktop preview.

Status: Bass Lab B was accepted as the frozen bass foundation on 2026-10-17.
Lab A remains available as its comparison baseline. Mid Lab was accepted as
the frozen hue foundation on 2026-10-17. Combined Lab was accepted as their
integration foundation on 2026-10-17. Later feature work must preserve all
three tested behaviors. Energy Lab was accepted as the visibility-floor
foundation on 2026-10-17.

The capped Energy-floor integration is the current preview-only experiment.

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
- Combined is the default v3 preview. Press `M` to cycle Combined, Mid, and
  Bass at the same playback position.

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
- Press `M` to cycle Combined, Energy, Mid, and Bass at the same playback
  position.

Energy must be accepted in isolation before integration. The later compositor
may use it only as a capped visibility floor beneath Bass B's existing
perceived-lightness curve. It must not modify Bass B's body, brighten a bass
peak that already exceeds the floor, or become a separate visual layer.

The first integration candidate maps Energy onto at most 22% of the body range
and renders with `max(bass_body, 0.22 * energy)`. This maximum is evaluated
only when converting the composed slice to light; the stored `body` remains
the exact Bass B value. Consequently, a bass peak above the floor is rendered
bit-for-bit like accepted Combined, while bass-light passages retain a dim
version of the same ribbon. Press `E` to compare the candidate with accepted
Combined at the same playback position.

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
