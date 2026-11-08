# Carrier USB layout review — 2026-11-06

Historical November 6 review. The November 7 mounting update reroutes USB
around screw clearances; current lengths/plots are in ROUTING.md and the
`carrier-review-20261107` inspection report. Electrical positions remain unchanged.

Desktop review of the existing two-layer carrier. The circuit, 90 × 75 mm
outline and all 32 component positions are unchanged. No additional components
or layers were selected. This is design evidence for a prototype, not measured
operation of the future assembled carrier.

## Correction made

The native input pair's original **0.25 mm** traces were unnecessarily narrow
for the available space on a 1.6 mm board. They are now **0.80 mm** wide on the
same paths, leaving **0.20 mm edge spacing** on the long parallel straight
sections. Connector and module ends fan apart to their existing pin spacing.
Host pin-grid escapes retain 0.25 mm traces. These choices improve the native
pair geometry without pretending to establish its exact electrical impedance.

Raspberry Pi's [Hardware design with RP2350, §5.1](https://datasheets.raspberrypi.com/rp2350/hardware-design-with-rp2350.pdf)
recommends approximately 90 Ω differential impedance and uninterrupted ground
under USB. Its 0.8 mm / 0.15 mm example is for a **1 mm board**; it does not
validate this 1.6 mm carrier. Ground on the carrier's top layer also affects
the result. A numerical impedance value would require the actual layer
construction and an appropriate calculation or measurement.

## Checks on the saved copper

| Check | Evidence / result |
|---|---|
| Native USB D+/D− connections | J1 → module U+/U−; independent netlist audit passes |
| Host USB D+/D− connections | GP0/GP1 → separate 22 Ω resistors → J2; audit passes |
| Module's native series resistors | Already on the PGA2350; no duplicate carrier pair |
| USB signal layer changes | None; all six carrier net sections use top copper |
| Native carrier lengths | D+ 48.60 mm; D− 46.31 mm; difference 2.29 mm |
| Host carrier lengths, before + after resistors | D+ 24.71 mm; D− 22.79 mm; difference 1.92 mm |
| Ground beneath native trace centres | About 95%; missing portions are principally pin/pad clearances |
| Open carrier area beneath USB | Reserved for ground rather than unrelated signal crossings |
| KiCad 10.0.6, after copper refill | 0 violations, 0 unconnected items, 0 schematic mismatches |
| Mechanical changes | None; printed fit template still matches all holes |

Lengths exclude tracks inside the modules and connectors. Centre-line ground
coverage is an inspection aid, not a pass criterion or proof of adequate return
current paths. The dense PGA pin field retains escape interruptions, particularly
on the host side. See ROUTING.md for the geometry and source/export paths.

## What the review does not establish

Native RP2350 USB operates at full speed (FS), **12 megabits per second**,
or low speed. That is more forgiving than 480-megabit high-speed USB, but does
not make track geometry irrelevant. In particular, do not assume that GPIO-based
host edges obey the native controller's electrical timing.

The [pinned Pico-PIO-USB source](https://github.com/sekigon-gonnoc/Pico-PIO-USB/blob/5a37a66dc5d3fbe0ef3cdbeda923a757440f984f/src/pio_usb.c#L368)
sets fast GPIO slew and 12 mA drive strength in `port_pin_drive_setting()`.
Slew is how quickly a pin's voltage changes; drive strength is its output-driver
setting, not a constant current drawn from the supply. The installed source was
read to confirm those settings. This review does not change them or claim an
edge-time measurement. The PGA2350 firmware port remains separate work.

The resulting board is still a hand-built prototype with header/module
transitions. It is not a certified USB layout, and neither the successful
breadboard nor a clean rules check guarantees its enumeration or typing.
Those require the assembled carrier. No new power tests on the existing
breadboard are requested by this review, and the accepted direct-power
arrangement in CARRIER.md is unchanged.
