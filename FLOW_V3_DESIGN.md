# FLOW v3 design contract

FLOW v3 is a preview-only experiment. It does not replace FLOW v2 or enter the
RP2040 firmware until its behavior is accepted in the desktop preview.

Status: Bass Lab B was accepted as the frozen bass foundation on 2026-10-17.
Lab A remains available as its comparison baseline. Later feature work must
preserve B's tested bass behavior.

Mid Lab is the next isolated experiment. It is not yet accepted or integrated
with Bass Lab B.

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

When the labs are eventually combined, Bass B exclusively owns body and
perceived lightness. Midrange hue must be applied through this equal-lightness
mapping so changing musical color cannot weaken or exaggerate a bass hit.

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
