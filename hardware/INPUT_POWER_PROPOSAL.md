# Controlled input charging — approved design, 2026-11-04

Status: **user-approved and implemented in the schematic and PCB; not bench validated**.
This approved addition amends PCB.md's frozen power path. It preserves the 48 × 42 mm
outline, four layers, 470 µF reservoir, existing fuse, downstream controller,
brightness limit and powered-Anker-only source constraint.

## Approved circuit

Add a TI TPS22953DQCR load switch before the protected 5 V rail. A load switch
is a chip that connects power through a transistor; here it gradually raises
the output voltage rather than connecting an empty capacitor abruptly.
JLCPCB lists the exact 2 × 3 mm part as
[C471046](https://jlcpcb.com/partdetail/TexasInstruments-TPS22953DQCR/C471046).

Its low on-resistance reduces the additional voltage loss. It also blocks
reverse current **when disabled**, including after loss of its bias supply.
This is not an always-active ideal diode: some reverse current can flow while
it remains enabled during an input collapse. Choose TPS22953, not TPS22954;
the latter has a discharge function instead of the former's reverse blocking.
[TI product information](https://www.ti.com/product/TPS22953)

```text
J1 USB 5 V -> existing F1 -> VBUS_FUSED -> new U6 -> existing +5V rail
                              |                    |
                           new 1 µF          existing 470 µF C20
                                                   |
                                    existing keyboard/display/3.3 V branches
```

All existing powered branches go after U6. Only the new small input capacitor,
switch bias supply and enable divider connect directly to VBUS_FUSED.
There is no second supply, firmware-controlled enable or changed GPIO pin map.

## Concrete circuit

| New reference | Part / value | Purpose / sourcing target |
|---|---|---|
| U6 | TPS22953DQCR, DQC0010A package | Ramp-controlled series switch; JLC C471046 |
| C30 | 1 µF, Samsung CL10A105KA8NNNC, 0603 | Local input bypass; reuse existing BOM part C5673 |
| C31 | 10 nF, 50 V, Samsung CL05B103KB5NNNC, 0402 | Timing capacitor; [JLC C15195](https://jlcpcb.com/partdetail/C15195) |
| R24 | 100 kΩ, 1%, UNI-ROYAL 0402WGF1003TCE | Upper enable-divider resistor; [JLC C25741](https://jlcpcb.com/partdetail/C25741) |
| R25 | 20 kΩ, 1%, UNI-ROYAL 0402WGF2002TCE | Lower divider resistor; [JLC C25765](https://jlcpcb.com/partdetail/C25765) |

Pin-level plan: U6 IN pins 1/2 and BIAS pin 3 connect to VBUS_FUSED;
OUT pins 9/10 feed +5V. GND pin 5 and exposed pad 11 connect to ground.
R24 runs from VBUS_FUSED to EN pin 4; R25 runs from EN to ground.
C31 runs from CT pin 6 to ground. SNS pin 8 ties directly to OUT.
Unused PG pin 7 ties to ground. C30 bypasses IN/BIAS locally.
CT is the timing connection; SNS senses voltage; PG means power good.
These connections follow [TI's datasheet](https://www.ti.com/lit/ds/symlink/tps22953.pdf),
pin table and §§9.3.1–9.3.3 / 9.3.7 / 9.3.10–9.3.11.

The divider gives a nominal enable threshold of 4.2 V. Screening the published
threshold range, 1% resistors and input leakage gives approximately 3.83–4.59 V
for turn-on; nominal turn-off is 3.6 V. This is not a precision 4.75 V
supervisor. The downstream TPS25821 retains its own supply qualification and
attachment control. Do not interpret unused PG as a verified 5 V-ready signal.

## Calculated behavior and cost

Using the timing formula with 10 nF gives approximately 3.52 ms/V:
about 14.1 ms from 10% to 90%, or 17.6 ms extrapolated across 0–5 V.
Allowing 500 µF for the reservoir and other rail capacitors gives approximately
142 mA of capacitor-charging current. A 600 µF / −10% timing-capacitor screen
gives about 189 mA. These are engineering estimates, not guaranteed current
limits: switch timing variation and the electronics' own current are additional.
The exact external LCD capacitance also affects the load.

At the measured 0.724 A peak, the chip's published 24 mΩ maximum at 5 V bias
screens to approximately 17 mV additional drop and 13 mW dissipation. Extra
routing resistance is not included. Startup pulse heating still needs checking
on the assembled board. Its 5 A component rating does not increase the board's
allowed load, fuse rating or downstream advertisement.

Budget **$1 per board for added components as a planning allowance**, not a
supplier quote. The public JLC listing confirms the part/package but does not
provide a reproducible current price/quantity in this browsing session.
Assembly may add a feeder-loading fee and inspection of the extra leadless
chip. [Published fees](https://jlcpcb.com/help/article/pcb-assembly-price)
currently list $3.07 per Extended part type for Economic assembly; the actual
order quote controls. No separate component shipment is proposed.

## Fit and implementation boundary

A native-KiCad placement study was followed by routing and full checks.
A courtyard is the assembly clearance outline around a component.
Final coordinates, relative to the upper-left outline:

- U6 (15.05,2.0), rotated 180°; C30 (17.6,1.9), rotated 90°;
  C31 (12.5,1.0), rotated 180°.
- R24 (15.1,4.85); R25 (15.1,6.85).
- H1 (21,4), C22 (17,5.5), unchanged from the first input-stage export.
- C21 (11.8,2.8), rotated 180°; F1 (7.8,1.8), rotated 180°.

The footprint matches the DQC land dimensions. The exposed solder pad has
no drilled holes; its ground via is outside the solder pad. Following Astra's
follow-up, rotating U6 places its output toward C21. The drawn connection is
1.5993 mm on top copper, without vias; C21 to U5 IN is 2.2522 mm, likewise
top-only. C30's input bypass is 1.6846 mm of drawn top trace and CT to C31 is
1.07 mm. These are trace lengths, excluding pad spreading, not measured loop
inductance. F1 moves 0.7 mm left and rotates to keep the upstream route clear.
The C21/C22 returns and all new parts pass the explicit via-land/paste audit.
The populated purchased count is now 70, total footprints 87. No additional
bare test pads or features were added. Full schematic/layout checks and
79 topology, 16 value, 9 prior-review and 11 input-power audit checks pass.
All 3,798 existing USB reference samples pass.

The local symbol marks unused PG as passive for electrical-rules checking,
because TI requires grounding this unused open-drain output; typing it as an
ordinary open-collector output falsely conflicts with the ground power flag.
Its physical open-drain behavior is unchanged and the ground tie is audited.
Input/output supply pins remain separate, and no global check rule was relaxed.

## Alternatives and limits

- TPS22918DBVR is simpler and also JLC-listed (C131941), but has more series
  resistance and an uncontrolled reverse path through its internal diode.
  It was considered, not selected. [TI datasheet §9.2.2](https://www.ti.com/lit/ds/symlink/tps22918.pdf)
- Reducing C20 avoids a switch but removes the intended reservoir; neither
  keyboard stability nor the resulting inrush has been established.
- Keeping the present circuit leaves its charging surge uncontrolled.

The user approved this five-component circuit and the three small placement
adjustments, then authorized the follow-up's smaller layout corrections.
It is implemented and checked with the bypass improvements above.
It addresses bulk-capacitor charging, not universal USB compliance or the
existing 500 mA declaration versus measured consumption. It adds no hard
current limit; the existing fault protections remain necessary. No order was
placed or manufacturer contacted. Actual startup, regulator behavior and USB
operation on the raw-chip board remain assembled-board bring-up work.
