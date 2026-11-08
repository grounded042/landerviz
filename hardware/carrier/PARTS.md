# Carrier parts and fit checklist

2026-11-07. Parts list for **one hand-assembled carrier**. The user has ordered
the OSH Park bare boards; component purchases remain pending. See
[SHOPPING.md](SHOPPING.md) for the current purchase list and
[ORDER_STATUS.md](ORDER_STATUS.md) for the frozen board reference.
[parts.csv](parts.csv) covers all 32 schematic positions plus header/socket
contacts. It is an inventory worksheet, not a factory assembly BOM (bill of
materials). Contact counts are not counts of packaged header strips.

## Purchasing constraint — one consolidated small-parts order

The user requested common sourcing to avoid repeated shipping charges. Use
**DigiKey as the planned supplier for missing small parts**, including the new
audio jack, both USB breakouts, LED, diodes, encoder and any missing ordinary
parts. Reuse the owned kit first. Do not silently add a Mouser/Newark/etc.
order for a convenient alternate. Check actual seller/fulfilment and shipping
at checkout; a product listing alone is not a shipping quote. No cart submitted.

**Processor purchasing requirement:** the user will confirm A4 stepping
before buying the PGA2350. Supplier stock is not claimed confirmed; this is
now an explicit user-managed purchase condition, rather than another fit check. DigiKey lists
[PIM722 / 1778-PIM722-ND](https://www.digikey.com/en/products/detail/pimoroni-ltd/PIM722/24814572)
but its checked listing shows **zero stock**, no backorders and pricing at a
224-piece quantity. That is not a viable single-module purchase. Do not promise
one supplier for the whole build or substitute another processor/module.
PiShop remains a candidate separate module order, subject to A4 confirmation.
The module price below is the earlier PiShop reference, not a confirmed order.
The checkout goal is one small-parts shipment plus, if necessary, one module
shipment, with any exception and delivered cost reviewed before purchase.

## Reuse first

- Existing LCD with backpack, BSS138 level converter and LCD jumper cable.
  These were already demonstrated on the prototype. The old audio breakout is
  superseded by the user's request for a new jack with downward-facing pins.
- Existing header assortment, where its bodies fit and male/female contacts
  mate correctly. Pitch means centre-to-centre pin spacing: 2.54 mm = 0.1 inch.
- Existing 470 µF and two 10 µF electrolytic capacitors, after checking actual
  body diameter and lead pitch. The stated 50 V rating on the audio capacitors
  is sufficient; it does not establish their physical dimensions.
- Existing resistors only if values, 1% tolerance and package match the list.
  The carrier needs 2 × 5.1k, 6 × 22k, 2 × 22 Ω, 2 × 10k and 1 × 1k. It does not use
  the prototype's 100k bias resistors. Do not buy another assortment by default.
- Existing service switches if their body/lead arrangement fits. Two are needed,
  for boot and reset. No separate next-mode button is added alongside the encoder.

The selected encoder is now **Same Sky ACZ11BR1E-20FD1-12C**, 12 detents
with push switch and documented hand-soldering limits. It replaces the Bourns
after Astra review; see [ENCODER.md](ENCODER.md).
Reusing the Taiss encoder would preserve its 20 detents and needs a different
mechanical check; the word EC11 alone does not prove footprint compatibility.

## Specific candidates for missing parts

Prices are individual-unit USD prices checked 2026-11-06 (encoder updated
2026-11-07), before shipping,
tax or any checkout surcharge. They are availability evidence, not purchases.

| Position | Quantity | Candidate | Unit | Line |
|---|---:|---|---:|---:|
| D1–D4 | 4 | [Vishay BAT85S-TR, DigiKey BAT85SCT-ND](https://www.digikey.com/en/products/detail/vishay-semiconductor-diodes-division/BAT85S-TR/BAT85SCT-ND/3104371) | $0.50 | $2.00 |
| SW3 | 1 | [Same Sky ACZ11BR1E-20FD1-12C, 102-1760-ND](https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices/ACZ11BR1E-20FD1-12C/1923355) | $4.33 | $4.33 |
| SW1/SW2 | 2 | [B3F-1000, DigiKey SW400-ND](https://www.digikey.com/en/products/detail/omron-electronics-inc-emc-div/B3F-1000/33150) | $0.35 | $0.70 |
| D5 | 1 | [Kingbright WP710A10LZGCK, DigiKey 754-1915-ND](https://www.digikey.com/en/products/detail/kingbright/WP710A10LZGCK/5177416) | $0.59 | $0.59 |
| J3 | 1 | [Same Sky SJ1-3533NG, DigiKey CP1-3533NG-ND](https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices/SJ1-3533NG/738701) | $1.69 | $1.69 |
| C3/C5 | 2 | [Vishay K222J15C0GF5TH5, DigiKey BC1050CT-ND](https://www.digikey.com/en/products/detail/vishay-beyschlag-draloric-bc-components/K222J15C0GF5TH5/286504) | $0.59 budget | $1.18 |
| **These candidates only** | | | | **$10.49 budget** |

The B3F-1000 manufacturer drawing specifies a 6 × 6 mm body and a
6.5 × 4.5 mm lead-centre pattern, matching the study's generic 6 mm switch
pattern. It is a purchasing candidate; the schematic has not been changed to
claim an exact installed switch. Manufacturer:
[B3F datasheet, page 4](https://omronfs.omron.com/en_US/ecb/products/pdf/en-b3f.pdf).

The filter-capacitor candidate is now **K222J15C0GF5TH5**: 2.2 nF, C0G, 5%,
50 V, with factory-formed leads at **5.0 mm spacing**. C0G is a stable ceramic
capacitor formulation. This is the H5 lead variant, not the 2.5 mm L2 part.
The [Vishay K-series drawing, pages 2–3](https://www.vishay.com/docs/45171/kseries.pdf)
decodes the full ordering code and specifies the size-15 body at most
4.0 × 2.6 mm, with 0.50 ± 0.05 mm leads. It fits the current 5 mm hole pattern
and 7.1 × 3.0 mm courtyard; the generic fabrication outline is approximate
(5.0 × 2.5 mm), not an exact drawing of the epoxy body. No hole change is needed.
Keep room for up to 6.6 mm seated height. The observed exact-part listing
shows $0.52 while the related-part table shows $0.59; budget the higher figure
and confirm checkout price/stock. Buy two pieces of **cut tape**, not a reel.
Existing electrolytics, resistors, compatible
headers and sockets may cover other needs, but inventory quantities are unknown.

With the $16.35 PGA2350/USB-module subtotal, the $10.49 conservative
candidate budget gives **$26.84 in listed electronics for ONE build**, before
unknown passives, headers/sockets, component shipping and tax. The current
filter-capacitor page lists $0.52 rather than the budgeted $0.59, so the actual
listed subtotal is $26.70. These are component subtotals, not delivered quotes.
The earlier $24.28 subtotal predated the encoder replacement and is superseded.
The OSH Park order is already placed; do not add another board order to the
component shopping list. Current supplier numbers/stock are in SHOPPING.md.

## New audio jack — SJ1-3533NG

The carrier now uses the direct through-hole jack instead of the existing
breakout. Its plug opening faces left and its **three solder terminals point
down into the PCB**. No audio-breakout male header or carrier socket is needed;
the three jack terminals are soldered directly.

The [2025-03-10 manufacturer family datasheet](https://www.sameskydevices.com/product/resource/sj1-353xng.pdf)
was downloaded and its page-2 mechanical drawing visually checked against the
installed KiCad circular-hole footprint. Manufacturer terminals are 1 sleeve,
2 tip, 3 ring; KiCad names those pads S, T, R. Tip goes to left audio, ring to
right, sleeve to ground. **This differs from the old breakout's pin numbering.**

The drawing covers multiple variants. Choose **SJ1-3533NG**: its pins 4/5 are
not populated. **SJ1-3535NG** has five terminals and is not interchangeable
with the selected three-hole footprint. A family photo showing five pins is
not the ordering specification. The circular-hole footprint uses 2 mm holes
for the blade terminals; no board-edge notch or plated slots are needed.

## Status LED, authorized 2026-11-06

The added circuit is **GP6 → R13 (1k) → D5 anode → D5 cathode → GND**.
The anode is the positive-side LED leg (pad 2); the cathode is the negative-side
leg (pad 1, the square pad). The selected LED has a 2.54 mm lead spacing and
fits the standard 3 mm through-hole footprint. Source:
[Kingbright datasheet](https://www.kingbrightusa.com/images/catalog/SPEC/WP710A10LZGCK.pdf).

GP6 high turns the LED on and low turns it off. No transistor or additional
supply is needed. Current should be around or below 1 mA; that is an estimate
from the LED characteristics, not a measured brightness/current result.
R13 may come from the existing resistor kit if its value/tolerance/package fit.

This adds **four solder joints** and leaves the carrier outline unchanged.
The LED needs working firmware to provide status. Waiting/ready/error patterns
are planned for the PGA2350 firmware port; no firmware has been changed or
flashed by this hardware addition. It is drawn illuminated in the overview
only to identify its colour and position, not to claim working firmware.

## PGA2350 sockets: owned corner fit confirmed; full-strip plan

**Order acceptance, 2026-11-07:** the user accepts the verified online module
specifications and earlier paper check as the basis for ordering. Full cut-strip
assembly and mating-depth checks remain build steps; they are not additional
pre-order requests. Do not describe those future steps as already performed.

**Fit result, 2026-11-06:** after the actual-size paper check, the user reported
that everything checked out and fitted well. The tested small 2×3 socket
pieces fit together at the PGA corner. This resolves the reported corner
collision question for those owned pieces; it does not approve unrelated
catalog sockets. No new dimensions, cut strips or module mating-depth result
were reported.

**Owned inventory, reported 2026-11-06:** two **2×40 female strips** and ten
**2×3 female pieces**. A 2×40 strip has 80 contacts; two provide 160 contacts.
Female PCB headers have mating sockets on top and solder posts underneath.
Their job here is to make the processor module removable from the carrier.
The 2×3 pieces are not assigned in the current layout.

The long strips have enough contacts for the 64-contact PGA socket. Confirm
their plastic width/height matches the tested small pieces before applying
that corner result to the cut pieces. Female strips are not necessarily
snap-apart: cutting can sacrifice a contact column and damage its neighbours.
Determine the housing arrangement and cutting method before modifying them.
The USB and level-converter modules also need mating sockets, but their
single-row arrangements are not automatically served by these double-row parts.

The electrical 64-hole grid and the owned small-piece corner paper fit are
checked. Full cut-piece fit and module mating depth are still separate checks.
Fully populating a removable module requires 64 male pins on the PGA2350 and
64 female contacts on the carrier: **128 solder joints** for that module alone.
That is separate from the other modules and through-hole components.

The [proposed full frame](socket-frame.svg) uses **two 2×10 pieces** across
the top/bottom and **two 2×6 pieces** down the sides between them: 20 + 20 +
12 + 12 = **64 contacts**. No module pins are omitted and no holes move.
One owned 2×40 strip can supply all four pieces while reserving an entire
contact column for each cut:

| Piece | Keep these columns on the original strip | Contacts |
|---|---|---:|
| A, top | 1–10 | 20 |
| B, bottom | 12–21 | 20 |
| C, left | 23–28 | 12 |
| D, right | 30–35 | 12 |

Columns **11, 22, 29 and 36** are reserved for cutting/discard, leaving four
spare columns (37–40). The second long strip remains intact. This is an
allocation plan, not permission to snap female headers or a verified cutting
method. Check the actual housing and available tool before cutting; protect
the contacts that will be kept. The discarded-column plastic must also be
removed to finish each housing to length: simply sawing through its centre
and leaving the remnants makes the piece too long for the planned corner fit.
The drawing is an assembly map, not to scale.

Do not assume any two 2×10 plus two 2×6 female sockets fit as a square.
For example, the [Sullins female-header drawing](https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/937/Female_Headers.100_DS.pdf)
specifies 5.08 mm body width, 25.90 mm length for 2×10, and 15.74 mm length
for 2×6. On this grid, top/bottom socket centres are 20.32 mm apart, leaving
**15.24 mm** between their bodies. A 15.74 mm side socket would overlap them
by 0.50 mm total. Therefore PPPC102LFBN-RC + PPPC062LFBN-RC are **not an
approved four-piece frame**, despite the correct pin pitch.

This rules out that particular arrangement; it does not invalidate the module
or carrier grid. First check the owned assortment for a compatible housing
arrangement. No socket purchase, extra holes, partial pin population, plastic
trimming or permanent module mounting has been selected as a workaround.
Socket height also affects underside component clearance and module support.

The Samtec SLW family was also examined from its
[manufacturer catalog](https://suddendocs.samtec.com/catalog_english/slw.pdf).
Its nominal dual-row body width is 4.95 mm and six-position length is 15.24 mm,
leaving only 0.13 mm nominal end clearance in the same frame. This is a
candidate requiring tolerance/physical-fit confirmation, **not a proven fit**.
No Samtec parts were added to the purchase list. Any purchased socket choice
must also be available through the consolidated supplier.

## Measurements without a metric tool

A printable fit aid is generated from the actual saved carrier by
`prepare_fit_template.py`. The current PDF is at
`../fabrication/carrier-fit-20261106/carrier-fit-template.pdf` (ignored export).
Print **Actual Size / 100%**, with Fit to Page disabled. Its calibration line
must measure exactly **one inch** before using the drawing. This is a component-side
view. Try the small owned 2×3 sockets at the PGA corners first; leave long
strips uncut. At the printed top-left corner, place one over the top two rows /
left three holes, and turn the other to cover the left two columns / next
three rows immediately below it. No holes are shared. Paper can expose housing collisions, but cannot prove contact
depth, stacking height or production tolerances. The module itself is not needed
for this preliminary check. Use spare capacitors and keep the working breadboard intact.

To regenerate from the repository root, using the KiCad-bundled Python:

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 \
  hardware/carrier/prepare_fit_template.py
```

The generator writes only an ignored snapshot and PDF; it does not modify the
carrier. The PDF is US Letter portrait, exported at scale 1. Its outlines are
not substitutes for the source dimensions and remaining fit checks below.

An inch ruler is fine for board/body envelopes. Report to the nearest 1/16 inch
and retain that uncertainty; converting the number does not make it more precise.
1 inch = 25.4 mm exactly. For hole spacing, a standard solderless breadboard
provides a 0.1-inch grid. Count **gaps**, not holes.

The middle trench is wider than a normal hole gap. On a conventional breadboard,
the centres of rows E and F are 0.3 inch apart; D and G are 0.5 inch apart.
Confirm row labels and actual board arrangement rather than interpreting
“one middle channel and one hole” as a definite distance.

**U2 measurement, 2026-11-06:** the user identified the occupied rows as **E and
G**, with a rough ruler reading of **3/8 inch** between the pin rows. On the
standard breadboard grid, E–G is **0.4 inch = 10.16 mm**; the ruler reading is
0.025 inch (0.635 mm) smaller and is consistent with a coarse measurement.
The study now uses 10.16 mm pin-row spacing, replacing the unverified 12.7 mm
assumption. It does not use 3/8 inch as a drill dimension.

The user subsequently measured the board as **9/16 inch along the pins** and
**just under 1/2 inch across**. Those give an approximate 14.3 mm length and a
conservative 12.7 mm width envelope. The study uses a **15 × 18 mm clearance
reservation** to allow for coarse measurement and socket bodies. The body is
assumed centred on the pin grid; exact registration and label orientation remain
approximate. On November 7 the user confirmed the labelled-side orientation:
HV row left, LV row right; both rows run channel 4, channel 3, GND, supply,
channel 2, channel 1 from top to bottom. J4 remains beside U2.

Physical evidence and assembly checks, updated November 7:

1. **U2:** labelled-side row direction is user-confirmed and the footprint is
   corrected: HV4/LV4 at the top, ground third, supplies fourth, channel 1 at
   the bottom. Existing measured row spacing and body envelope are retained.
2. **J3:** the selected three-terminal jack matches its manufacturer drawing.
   Check the delivered model during assembly; no old-breakout measurements needed.
3. **C1/C2/C4:** the user reports **the capacitors fit**, closing the lead-fit
   question. Retain 3.5 mm lead pitch for C1 and 2.5 mm for C2/C4 and the
   previously reserved body envelopes. No repeat measurement is requested.
4. **Sockets:** manufacturer grid/dimensions plus earlier paper fit are accepted
   by the user for ordering. Cutting, full-frame assembly and module mating
   remain build steps. Do not force or snap female socket housings.

Rough body measurements establish space reservations. They do not release an
unverified drill pattern. The converter orientation and capacitor lead fit
now have user observations; the processor acceptance uses published dimensions.
Independent follow-up review of the corrected board remains before ordering.
