# PGA2350 carrier — working plan

2026-11-06. **Electrical draft and routed carrier draft; no purchase release.**
Open [carrier/landerviz-carrier.kicad_pro](carrier/landerviz-carrier.kicad_pro).
See [carrier/README.md](carrier/README.md) for verified parts and checks.
The user selected a hand-solderable carrier using Pimoroni PGA2350, assuming
RP2350B **A4** silicon. PiShop stock revision is awaiting their reply.
This document governs the new carrier exploration; `../PCB.md` describes the
preserved bare-RP2354A candidate. Do not overwrite that design or reuse its
manufacturing files for the carrier.

## Scope

- Same standalone audio analysis, keyboard forwarding, Moonlander RGB protocol,
  display/menu and supported powered-Anker setup. No host visualizer application.
- PGA2350 provides processor, flash, clock, processor supplies and regulator.
  Additional module memory does not change the visualizer's requirements.
- User-soldered headers, modules and through-hole components. No factory assembly
  assumed. A solderable component has wire leads passing through PCB holes;
  tiny processor/USB contacts arrive soldered on purchased modules.
- Preserve own upstream and downstream USB-C connectors, opposite edges, with
  upstream USB and audio on the same side. The user authorized a larger outline;
  the current hand-soldering placement study is 90 × 75 mm. See
  [carrier/PLACEMENT.md](carrier/PLACEMENT.md); reused-part dimensions remain provisional.
- Simplify power toward the proven prototype. The new TPS22953 input-ramp stage
  is not carried over by default. The bare-chip design's other power features
  are not automatically requirements for this separate carrier.
- User selected an always-powered Moonlander output: keyboard power follows
  LanderViz input power. Automatic CC-controlled power switching is omitted
  from this working baseline; exact connector/termination parts remain open.
- **Accepted limitation, 2026-11-06:** after discussion of accidental laptop
  connection, the user accepts proceeding with this Moonlander-only prototype
  without local electronic current limiting or reverse-current blocking.
  This acceptance does not establish that misconnection cannot cause damage.
  Do not reopen the same decision or add protection circuitry without a new
  requirement or material new evidence. It is not permission to order boards.
- No VFD, digital expansion port, wireless, added processor or enclosure work.

## Module and firmware pin plan

Verified against [Pimoroni's schematic](https://cdn.shopify.com/s/files/1/0174/1800/files/Pimoroni_PGA2350_Schematic.pdf?v=1724926880)
and [pinout](https://cdn.shopify.com/s/files/1/0174/1800/files/pga2350_pinout_diagram.pdf?v=1723124465).
Names below are the module's printed signals, not a new carrier header numbering.
GPIO means general-purpose input/output; ADC means analog-to-digital converter.

| Function | PGA2350 signal | Change from working firmware |
|---|---|---|
| Native upstream USB D+ / D− | U+ / U− | Connect carrier input port to module's native USB pins |
| Keyboard PIO-USB D+ / D− | GP0 / GP1 | Preserve, via external 22 ohm series resistors |
| LCD serial data / clock | GP2 / GP3 | Preserve I2C1 (two-wire display bus) |
| Encoder A / B | GP4 / GP5 | Preserve |
| Encoder push | GP8 | Preserve internal pull-up and software timing |
| Status LED | GP6 via 1k resistor | New active-high output for PGA2350 firmware port; green 3 mm LED |
| Audio left / right | GP40 / GP41 = ADC0 / ADC1 | Replace GP26 / GP27; ADC channel indices remain 0 / 1 |
| Boot button | BS to GND | Add accessible push button on carrier |
| Reset button | RUN to GND | Add accessible push button on carrier |
| Module input power | VB, 3–5.5 V | Feed from carrier's 5 V input branch |
| Logic/analog bias power | 3V3 | Module regulator output; 300 mA published maximum |
| Ground | GND pins | Common ground with planned short audio returns |

The module schematic already includes 27 ohm series resistors on native USB;
do not automatically add another pair to the carrier. Keyboard PIO-USB still
needs its own 22 ohm pair. Confirm against the delivered module revision.

Keyboard and LCD 5 V branches do not run through the module's 3.3 V regulator.
Their current also need not pass through the module's VB header: branch at the
carrier power entry, then feed VB separately. Keep audio returns away from
keyboard/display return paths, using continuous ground copper and deliberate
placement rather than an arbitrary split ground plane.

Firmware port is still pending. Current `flake.nix` selects `sparkfun_promicro`,
main.cpp hardcodes GP26/27, CMake defines `ARM_MATH_CM0PLUS`, and the USB bridge
sets 120 MHz for PIO timing. Add a separate RP2350B/PGA2350 build configuration,
review SDK/PIO-USB/CMSIS support and flash size, and preserve RP2040 builds.
PGA2350 silicon is different from the proven RP2040: compile success alone
does not establish USB or ADC operation. Do not overwrite current flash builds.

## What gets soldered

| Assembly | Proposed manual work | Status |
|---|---|---|
| PGA2350 | Standard 2.54 mm headers, preferably socketed | Existing header kit may cover it; full module has 64 pins, not two Pico-style 20-pin rows |
| USB-C ports | Headers on two factory-soldered Soldered 333011 breakouts | Draft selected; input/output roles set by carrier resistors; mounting pending |
| Audio | Six axial resistors, two coupling capacitors, two filter capacitors, four discrete rail-clamp diodes | Values can follow the reviewed passive frontend; Draft selects four Vishay BAT85S-TR diodes; mechanical footprints pending |
| Audio jack | Direct right-angle Same Sky SJ1-3533NG; three downward solder terminals | User preferred a new jack over the rear-facing-header breakout; selected 2026-11-06 |
| Display | Existing 4-channel level-converter module plus a 4-pin header | Reuse known working module and LCD cable; verify voltage-side pin order |
| Encoder | Existing EC11 or the previously selected 12-detent encoder | Exact physical part/footprint to settle; do not silently substitute detent count |
| Service buttons | Boot and reset through-hole switches | Select footprint from available parts |
| Status LED | One green 3 mm LED plus one axial 1k resistor | Authorized 2026-11-06; GP6 drives LED, firmware behaviour pending |
| Power | Existing 470 µF bulk capacitor, short 5 V/ground paths | Exact input/output arrangement still to settle |

Fully populating and socketing 64 module pins means up to 128 header solder
joints between module and carrier, before peripherals. Partial population is
possible only after confirming electrical and mechanical support needs.
Factory assembly has not been eliminated until every remaining part is
available in a genuinely hand-solderable form.

## USB-C decisions to finish

CC means configuration channel: each socket has two CC pins that establish
connection/power roles. The Mac-facing port consumes power; the Moonlander-
facing port supplies it. These need different terminations.

| Candidate | Published price, before shipping | Assessment |
|---|---:|---|
| [Adafruit 4090](https://www.adafruit.com/product/4090) | $2.95 | Suitable input candidate: data, CC pins and built-in 5.1k pull-downs. Despite the product's “downstream” name, it is configured to consume power. Those resistors do not guarantee any particular available upstream current. |
| [Pololu usb07a / 2585](https://www.pololu.com/product/2585) | $4.95 | Exposes data, CC and duplicate power/ground pins. Default pull-downs must be disconnected/removed for source use; manual modification and mounting need checking before selection. Not a complete source-power controller. |
| [Adafruit 5180](https://www.adafruit.com/product/5180) | $1.75 | Not selected: only four classic USB signals, with no exposed CC configuration for our two-role carrier. |

Connector screening, 2026-11-06: [Adafruit 5972](https://www.adafruit.com/product/5972)
is not a suitable shortcut for keyboard power. Its switches are specified for
only 100 mA, below our measured system load; its extra USB-A plug also does
not match the two-socket arrangement. Continue sourcing a bare/configurable
USB-C breakout rather than adding a power controller to work around selection.

Two receptacle orientations must work. Selected working approach: fixed CC
pull-ups plus direct 5 V, an always-powered prototype output like the
breadboard's USB-A power path;
it does **not** reproduce a USB-C controller's attach-controlled power switching,
discharge, current limit or full source behaviour. Do not imply otherwise.
Draft uses two Soldered 333011 modules with independent 5.1k input pull-downs
and 22k-to-5V output pull-ups (1.5 A advertisement, inherited from the original
downstream design). Verify module mounting before layout. Resistor advertisement must not imply unverified upstream
current capability; the tested keyboard load is not proof of a 1.5 A supply.
No USB Power Delivery (PD, negotiation of higher voltage) is required.

### Accidental connection to a MacBook Air or another powered port

With correct source CC pull-ups, the carrier identifies itself as a power
source. A dual-role laptop port can consequently select the power-sink role
and try to receive power, rather than necessarily drive 5 V against our 5 V.
This is a protocol-based expectation, **not a tested prediction** of a specific
MacBook model, battery state, cable or always-live connection sequence. It
could decline the connection or attempt low-power charging. Keyboard data
forwarding is not useful through this host-facing output connection.

The direct power arrangement has no local electronic current limit or reverse-
current blocking. Another sink could load the hub/carrier beyond the measured
keyboard load; the hub might shut down or the supply might sag. If the remote
port does drive its own power, a direct shared rail permits current between
supplies (backfeeding). The design does not establish protection against that
case. Do not describe source conflict as inevitable, or MacBook protections
as a guarantee of safety. An attachment controller also does not authenticate
that a sink is our Moonlander; other sinks can still attach.

Supported use remains the powered Anker input and Moonlander-only output,
clearly labelled. The always-live output is not a general laptop charger or
a fully conforming USB-C source. The normal USB-C source sequence detects
attachment before enabling power and removes power after detachment.

References: [Microchip CC role/current explanation](https://developerhelp.microchip.com/xwiki/bin/view/applications/usb/typec-features/cc-vconn-signals/),
[TI attachment-controlled source example](https://www.ti.com/product/TPS25820),
[Apple USB-C charging guidance](https://support.apple.com/en-ie/102397).
These sources do not validate our nonstandard always-live circuit with a Mac.

## Power decisions to finish

The supported Anker data-port source and 175/255 keyboard brightness cap remain.
Measured whole-system peak was 0.724 A. Use that to size the carrier's 5 V
connections; the PGA2350's 300 mA rating is for its 3.3 V regulator only.

Selected working baseline: branch the incoming 5 V directly to module VB, LCD
and keyboard supply, retain the breadboard's 470 µF bulk capacitor and
intentional return paths. This matches the simplified direction the user
requested; it is not yet a released
USB-C power implementation. A hand-solderable input fuse is an option to price,
not an approved added part. No new input ramp/protection IC is selected.

Removing U6 removes its particular 6 V-limit question from this carrier, but
does not prove a different module/input network immune to plug-in overshoot.
Check module limits and chosen input capacitance without claiming the earlier
simulations validate the carrier or adding circuitry automatically.

## Cost checkpoint before layout

Current draft prices checked 2026-11-06: PGA2350 at PiShop $11.95 plus two
Soldered 333011 modules at DigiKey $2.20 each: **$16.35 before shipping/tax**.
This supersedes the earlier $19.85 Adafruit/Pololu candidate comparison.
See [carrier/README.md](carrier/README.md) for sources and exact module pinout.
No tiny onboard resistors need removal for these selected USB breakouts.

Purchasing constraint added 2026-11-06: consolidate missing small parts at
DigiKey rather than incur multiple small supplier shipments. The direct
SJ1-3533NG audio jack is available there along with the selected indicator,
encoder, diodes and USB breakouts. PGA2350 has not been ordered; PiShop's A4
reply is pending and DigiKey's PIM722 listing currently shows no stock. A
separate module shipment may be necessary; do not promise otherwise or order
substitutes without alignment. See carrier/PARTS.md for sources and limitations.

The 90 × 75 mm study gives a $52.31 area-based estimate for three US-made bare
boards at OSH Park's published $5/in² rate, checked 2026-11-06. This is not a
submitted quote. Adding modules for one build gives $68.66 before missing parts,
component shipping/tax. See the placement notes for source and calculation.
Still unpriced: missing through-hole parts/sockets, shipping and tax.
Reuse existing LCD, converter, headers, capacitors and compatible kit parts
where practical. The audio breakout is replaced by the new direct jack.
Do not describe $16.35 as a finished build.

Next: confirm provisional physical footprints, sockets/switches and
encoder choice, and complete the delivered-cost comparison before fabrication. The electrical
draft passes ERC and 16 independent connectivity requirements. The saved study
adds pad/coordinate/courtyard checks. The [routing notes](carrier/ROUTING.md)
record copper checks; functional validation still requires the assembled carrier.
No purchase, upload, commit or flash.
