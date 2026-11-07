# Preorder confidence — internal engineering record

2026-11-06. Current board: `marking-20261105`. No production approval.
Keep this record out of factory uploads. No success percentage is established.

## What is established

- Breadboard proves audio-driven RGB, keyboard forwarding, LCD and encoder
  with the RP2040 module. It does not prove the bare RP2354A PCB.
- Frozen board has stored zero-finding electrical/layout checks and passing
  topology/value/geometry audits. These are not electrical simulation.
- Independent review corrections are tracked in `REVIEW_RESOLUTION.md`.
  In particular the follow-up's U6 output-bypass routing weakness was corrected
  after that review; do not report that old geometry as a current defect.

## Remaining evidence and proposed work

| Area | Evidence needed / limitation |
|---|---|
| Factory assembly | Explicit acceptance of U1/U2 unfilled ground-pad holes, U6 paste coverage and inspection; actual part revisions and final placement approval. See short assembly requirements. |
| Input hot-plug | Evaluate U6 input/BIAS overshoot against its 6 V absolute maximum. A SPICE sensitivity sweep can explore assumed cable inductance, resistance and source impedance; it cannot establish the real hub/cable waveform. Physical measurement remains needed. |
| Loaded startup / reconnect | Review U6 ramp, enable thresholds, capacitor tolerance and downstream attach timing. Use manufacturer models if available and compatible; label behavioural approximations. Do not claim a capacitor-charging calculation bounds total startup current. |
| Audio input | Simulate frequency response, bias and ADC-node excursions over component tolerances and realistic source/input impedances. Check powered/unpowered cases where the model represents protection paths. This does not predict actual ground-noise performance. |
| RP2354A firmware | Establish a clean build for the actual chip/board configuration and prepare minimal boot/USB/display/encoder/audio diagnostics. Compilation is not proof of boot, clock operation or USB signal integrity. |
| Physical board | Rail voltages, startup transients, boot and both USB roles remain assembled-board checks. Ask separately about cost/feasibility of a factory power/boot test; none is currently ordered. |

Bounded simulations completed: see `simulation/README.md` and its reproducible
runner. Audio small-signal response preserves bass; TI U6 nominal startup
models ramp as expected. Input-only hot-plug sensitivity can exceed the 6 V
limit under assumed cable conditions, so actual hot-plug safety remains open.
No full-system startup or fault-survival result is established. L1 terminal
orientation matches the official reference; the fabrication dot corner
illustration differs and needs resolution in a new snapshot before release. No new test equipment, protection components,
layout changes or paid factory testing authorized by this document.

## Handoff audit outcome

Earlier local packages contained repeated geometry/process descriptions,
black/OSP settings now stale, illustrative renders, schematic and pad exports.
The replacement keeps one short fabrication note and one short assembly note;
only the placement drawing and complete positions accompany assembly notes.
Copper/drill data, BOM and centroid are unchanged. The actual files uploaded
by the user cannot be established from local exports alone. Do not describe
the replacement as submitted or accepted until the user/vendor confirms it.
