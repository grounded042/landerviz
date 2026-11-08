# Carrier assembly preparation

Working guide, 2026-11-07. The carrier has a **routing draft and is not released for
fabrication**. This records the intended assembly sequence for the current
schematic. Converter orientation/capacitor fit and the processor dimensional order basis
are now recorded. Socket construction, module support and firmware bring-up
remain assembly work; a focused independent follow-up precedes ordering.

All carrier components are through-hole: their leads pass through the board
and are soldered underneath. The purchased modules contain the small chips.
The front of the carrier holds the components; the underside needs room for
solder joints and clipped leads when an enclosure is eventually designed.

## Intended assembly order

1. **Check the parts against parts.csv and the released board revision.**
   In particular, J3 is **SJ1-3533NG**, the three-terminal audio jack, and the
   processor module must be the intended A4 PGA2350. The similar five-terminal
   jack and a different encoder package are not automatic substitutes.
2. **Resistors and clamp diodes first.** These sit low on the board and are
   easier to reach before installing taller parts. Bend axial leads (wires
   emerging from opposite ends) to the selected hole spacing. Resistors have
   no electrical polarity; the four diodes do.
3. **Small ceramic capacitors, status LED and boot/reset buttons.** The ceramic
   capacitors have no polarity. The status LED does; use the table below.
4. **Headers and sockets.** Confirm their plastic bodies fit before soldering.
   The owned small socket pieces passed the paper corner check. The proposed
   complete PGA arrangement is in [socket-frame.svg](socket-frame.svg) and
   PARTS.md; long-strip housing match and cutting method remain to confirm.
   Tack one pin first, check alignment, then finish. Tack means making an
   initial joint to hold a part while its alignment is checked. Keep all mating
   and alignment work unpowered. Do not force mismatched square/round contacts.
5. **Direct audio jack and encoder.** Seat their bodies squarely against the
   PCB before finishing the joints. The selected encoder is **Same Sky ACZ11BR1E-20FD1-12C**; its two tabs
   are soldered mechanical anchors in addition to five electrical contacts.
   Its specified hand-soldering limits are **350 °C / three seconds maximum**.
   Allow cooling between attempts; avoid prolonged heating of the shell tabs.
   The larger round mounting holes need the part held seated during initial
   soldering. See ENCODER.md; the old Bourns is not an approved substitute.
6. **Electrolytic capacitors.** Install C1/C2/C4 with the polarity shown below.
   These are the taller round cans; their height and diameter must already
   match the final physical drawing.
7. **Plug-in modules last.** Fit the PGA2350, USB breakouts and level converter
   only after checking connector orientation and the completed carrier joints.
   Final socket and support hardware details are still pending.

There are 32 electrical assembly positions. The fully populated PGA2350
connection alone involves 64 male pins on the module and 64 socket contacts on
the carrier: 128 solder joints. The exact total for the build depends on which
module headers arrive soldered and which owned parts are reused.

## Polarity and pin meanings

| Part | Positive/anode side | Negative/cathode side | Recognition |
|---|---|---|---|
| C1, 470 µF | Pad 1: +5V | Pad 2: GND | The can's stripe normally marks **negative**; the PCB '+' marks positive |
| C2, left 10 µF | Pad 1: BIAS_L | Pad 2: LINE_L | Positive faces the biased ADC side of the circuit |
| C4, right 10 µF | Pad 1: BIAS_R | Pad 2: LINE_R | Positive faces the biased ADC side of the circuit |
| D1/D3, upper clamps | Anode pad 2: ADC_L/ADC_R | Cathode pad 1: +3V3 | The diode's band marks the **cathode** |
| D2/D4, lower clamps | Anode pad 2: GND | Cathode pad 1: ADC_L/ADC_R | The diode's band marks the **cathode** |
| D5, green status LED | Anode pad 2: STATUS_LED_A | Cathode pad 1: GND | Flat side/short leg marks cathode; pad 1 is square |

The **anode** and **cathode** are a diode's two terminals. For the status LED,
current flows from the processor through R13 into its anode and out through
its cathode to ground. On the selected four clamp-diode footprints, pad 1 is
the left-hand pad in the present top-view placement; verify the final marking
rather than using that as a universal diode rule.

**BIAS_L/BIAS_R** are the separate left/right audio resting-voltage nodes,
approximately halfway between ground and 3.3 V. **ADC** means analog-to-digital
converter, the processor input that samples the voltage. The coupling capacitors
connect the audio jack to these biased nodes; their positive terminals face
the bias, not the jack.

## Connector orientation to preserve

- **J1 input:** USB-C opening faces left. It connects to the supported powered
  hub's data port. **J2 keyboard:** USB-C opening faces right, Moonlander only.
- **J3 audio:** the plug enters from the left and its three solder terminals
  point down into the carrier. Sleeve/ground is manufacturer terminal 1 (pad S),
  tip/left terminal 2 (pad T), ring/right terminal 3 (pad R). The old breakout's
  numbering does not apply.
- **J4 LCD:** lower left, beside U2. Looking down at the component side, pins
  run top to bottom: **1 GND, 2 5 V, 3 SDA, 4 SCL**. SDA is the display's serial
  data line; SCL is its clock line. Use the corresponding LCD labels, rather
  than assuming its backpack has the same physical pin order.
- **U2:** LV connects to 3.3 V and HV to 5 V; both GND pins connect to ground.
  User-confirmed orientation: labelled side up, **HV4 upper left, LV4 upper
  right**. Both rows descend as **4, 3, GND, supply, 2, 1**. The printed HV4/LV4
  marks identify the top. Grounds are third from top; HV/LV supplies fourth.
- **U1 PGA2350:** the manufacturer's top edge faces down in the present carrier
  placement. Use the marked A1 corner and signal labels when the final assembly
  drawing is prepared. The module's 5 V input is **VB**, not its 3V3 pin.

## Before first power

Inspect the new carrier for bridged pads, misplaced parts and reversed polarized
components, and check that the power rails are not shorted. A charged capacitor
can give a changing resistance reading; a continuity beep alone is not a complete
power-rail diagnosis. These are assembly checks for a newly soldered board,
not a request to repeat the prototype's completed power measurements now.

First-power instructions will be completed with the PGA2350 firmware port and
final hardware. The current RP2040 firmware image is not a PGA2350 image.
D5 provides status only once firmware drives GP6; an unlit D5 by itself does
not establish that the board is unpowered.

This guide does not change the accepted power arrangement, add components,
select an enclosure, authorize an order or imply that the board is ready to buy.

## Audio operating boundary for first bring-up

Connect the mixer audio after LanderViz has power; disconnect audio before
removing power from the carrier. This is the conservative intended sequence,
not certification of hot-plug transients or powered-off input safety. The
rail-clamp diodes can feed an unpowered 3.3 V rail and are not isolation.

At nominal 1.65 V bias the ideal 0–3.3 V conversion span has at most 1.65 V
peak amplitude (about 1.17 Vrms for a sine wave). Vrms is the root-mean-square
voltage, a way to express a varying signal's effective magnitude. This is an
ideal clipping-headroom calculation, **not a validated source-level rating**;
leave margin for bias error, peaks and noise. Do not infer it from headphone
loudness or a multimeter's DC reading. The actual source ceiling and clipping
under keyboard/display activity remain assembled-carrier bring-up checks.
No attenuation/protection circuitry was added by the review response.
