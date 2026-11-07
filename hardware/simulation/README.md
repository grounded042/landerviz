# Candidate electrical checks — 2026-11-06

Internal engineering evidence, not factory instructions. Candidate:
`marking-20261105`. No schematic, footprint, routing or fabrication change.
**These results do not release the board for production.**

## Model availability and execution

KiCad 10.0.6's bundled ngspice 45.2 shared library runs locally through Python's
standard-library `ctypes`; no Python packages or new Nix dependencies added.

- U6 TPS22953: TI's unencrypted PSpice transient package SLVMAS6B downloaded
  from [TI](https://www.ti.com/lit/zip/slvmas6b). ZIP SHA-256:
  `fd5a2820b7ca2bf52c79689e680becabae314c7bce2789dfcc4ef4a205e7004c`.
  The ZIP contains `SLVMAS6A/TPS22953_TRANS.LIB`, internally Final 1.00 (2015).
  It runs in ngspice's PSpice compatibility mode with KiCad code models loaded.
  Only text encoding is normalized; no model equations edited. Compatibility
  is demonstrated for these runs, not independently qualified by TI.
- The model header lists switching characteristics, on-resistance and room-
  temperature quiescent/off currents; **temperature effects are not modelled**.
  Do not infer damage survival, real cable behaviour or protection guarantees.
- No downloadable TPS25821 model was found on its
  [product page](https://www.ti.com/product/TPS25821) during this check. Its
  attachment timing/current limiting is not represented by the load resistor.
- TLV755P's [product page](https://www.ti.com/product/TLV755P) lists transient
  models for some output variants. No exact 3.3 V model was acquired/validated;
  a different-voltage model was not substituted. U5 and the RP2354A regulator
  are not simulated here.
- Audio uses exact passive component values and verified channel connections;
  it does not include a manufacturer ADC or nonlinear diode model.

Results, generated netlists, waveforms and logs are in the ignored directory
`../fabrication/simulation-20261106-verified/`. `results.json` includes hashes
of the script, frozen manifest and downloaded model. TI model data stays in
ignored output; do not redistribute it as project source.

## What was checked

Before simulation the script checks the frozen snapshot's artifact hashes,
both audio channels' passive values/nets, clamp pin mapping and U6's pins,
enable divider and timing/input capacitors against the stored board audit.
The existing board audit separately covers the other circuits.

Completed: two audio AC sweeps, four U6 transient runs and 18 input-network
transient runs, with saved logs checked for simulation errors. All audio
SPICE points agree with an independent complex-impedance solution within
2e-7 absolute voltage gain. Six unit tests check the independent calculation's
limits and waveform crossing measurements. Another 128 audio component/source
corners are calculated analytically, not simulated as 128 separate SPICE runs.

## Audio result

Small-signal analysis (AC) describes the response to signals small enough that
rail clamps remain off. Source resistance is assumed 100 ohms nominal; the
corner screen includes 1 kilohm. ADC means analog-to-digital converter.
The ADC is represented by either negligible AC loading or a 100 kilohm
AC load to a fixed midpoint. This is a sensitivity approximation, **not a
model of DC leakage, input sampling, settling or effective resolution**.

| Frequency | Negligible ADC loading | 100 kilohm effective AC load |
|---|---:|---:|
| 20 Hz | -0.10 dB | -0.94 dB |
| 60 Hz | -0.08 dB | -0.92 dB |
| 1 kHz | -0.16 dB | -0.99 dB |
| 7.8125 kHz | -3.48 dB | -3.88 dB |
| 15.625 kHz | -7.68 dB | -7.84 dB |

The passive network preserves the bass range under these assumptions. It is
not a sharp anti-alias filter: attenuation near/above the sampling limit is
modest, as already disclosed in PCB.md. Negative dB indicates attenuation.

The corner calculation varies 1% resistors, 5% shunt capacitance, assumed
20% coupling capacitance, source resistance and effective AC loading. At
60 Hz its gain range is -1.68 to -0.08 dB. This is an engineering screen, not
qualification over temperature, diode leakage, actual source impedance or
capacitor ageing. No conclusion about audio overdrive or unpowered rail
injection is established by this small-signal model.

## U6 startup result

500 microfarads aggregate output capacitance (600 in the low-source case),
0.1 ohm reservoir equivalent series resistance, 1 microfarad input bypass,
100k/20k enable divider and 10 nF timing capacitor (9 nF in low-source case).
Supply rises in 1 microsecond; no cable inductance in these startup runs.
The listed source resistance includes assumed cable/fuse/source resistance.
Loads are resistors selected from measured breadboard idle/peak current at
5 V; they are **not** actual system startup or constant-power models.

| Case | Supply / source resistance | Output at 60 ms | 10–90% rise time |
|---|---|---:|---:|
| Unloaded | 5 V / 0.15 ohm | 5.000 V | 13.13 ms |
| Idle-equivalent | 5 V / 0.15 ohm | 4.953 V | 13.46 ms |
| Peak-equivalent | 5 V / 0.15 ohm | 4.883 V | 13.78 ms |
| Low-source screen | 4.75 V / 0.5 ohm | 4.420 V | 12.33 ms |

Rise thresholds are 10% and 90% of the source voltage. Nominal cases support
the expected gradual output rise; this does not prove the connected keyboard
will start. The low-source case shows supply resistance matters. Source
current after 100 microseconds peaks at 0.152/0.414/0.797/0.771 A respectively.
Those figures intentionally exclude the input bypass's initial charging pulse
and must not be quoted as whole-device peak/inrush limits. Output capacitance,
load startup sequence, downstream switch and temperature need real validation.

## Input hot-plug sensitivity — still unresolved

An isolated linear resistor/inductor/capacitor (RLC) network models an assumed
5.2 V source rising in 1 microsecond, cable inductance 0.1/1/3 microhenries,
series resistance 0.1/0.3/0.8 ohm and input capacitance 0.6/1 microfarad.
These are sensitivity assumptions, not measured properties of the Anker hub,
USB cable, fuse or PCB. Input resistance/damping and source rise dominate.
No actual U6 clamp, failure behaviour or upstream hub current limit is modelled.

The calculated input peaks span **5.20–9.94 V**; some assumptions exceed U6's
6 V absolute maximum. This reinforces the existing review question, **not a
prediction that this board sees 9.94 V or a confirmed hardware defect**.
The meter's millisecond-scale readings cannot resolve these fast peaks.

A real source/cable oscilloscope capture, or a justified protection/damping
change with its own verification, is needed to close this risk. Do not treat
nominal startup success as clearing it. No protection part or redesign is
selected by these simulations.

## L1 orientation recheck

L1 is the 3.3 microhenry inductor used by the processor's 1.1 V switching
regulator. [Raspberry Pi's design guide, power section](https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-008280-DS-1-hardware-design-with-rp2350.pdf?disposition=inline)
explains that winding orientation affects magnetic coupling into the output
capacitor and can disturb regulator operation.

The official R4-S1 KiCad reference in `/tmp/landerviz-inrush-references/`
places the physical dot beside pad 1 / +1V1, opposite pad 2 / VREG_LX.
Our frozen board likewise has pad 1 left at (120,109.65), +1V1; pad 2 right
at (122,109.65), switching node. The corrected fabrication dot marks the left
end. The factory must confirm the **physical white dot faces that left end**.
[Abracon's marking drawing](https://abracon.com/datasheets/AOTA-B201610S3R3-101-T.pdf)
identifies the winding orientation; a vendor model's zero rotation is not proof.

Drawing ambiguity to resolve before a final release: our fabrication dot is
upper-left, while the reference's part dot is lower-left in this orientation.
Both identify the same terminal, but our illustration is not an exact physical
corner depiction. No CAD or frozen-file change was silently made here. Any
correction must produce a new checked snapshot and replacement handoff.

## Reproduce

Download the model ZIP directly from TI to a local temporary path. With KiCad
installed on macOS, run from the repository root (fresh output path required):

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s hardware/simulation -p 'test_*.py'
PYTHONDONTWRITEBYTECODE=1 python3 hardware/simulation/check_candidate.py \
  --snapshot hardware/fabrication/marking-20261105 \
  --model-zip /tmp/landerviz-tps22953-model.zip \
  --out hardware/fabrication/simulation-rerun
```

The runner does not download dependencies, edit KiCad files, upload anything,
or approve production. Library/code-model paths default to KiCad's macOS
installation; an alternative shared-library path can be supplied explicitly.
