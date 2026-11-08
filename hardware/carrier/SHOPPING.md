# Remaining components for one ordered carrier

Checked 2026-11-07 against the reviewed Carrier Rev A. The OSH Park board order
is placed; see ORDER_STATUS.md. This list is for **one assembled carrier**,
not for populating every spare PCB. No shopping cart or component order made.

## Consolidated DigiKey order

[shopping-digikey.csv](shopping-digikey.csv) contains exact supplier numbers and
quantities for list import. Map its part-number, quantity and customer-reference
columns if the importer asks. Review the resulting matches and packaging before
checkout. It includes two known matching service buttons; remove that row if
you already have two compatible ones. No unrequested spare quantities included.

| Part | Qty | DigiKey number | Listed unit / line cost |
|---|---:|---|---:|
| [Soldered 333011 USB-C breakout](https://www.digikey.com/en/products/detail/soldered-electronics/333011/21720502) | 2 | 5032-333011-ND | $2.20 / $4.40 |
| [Same Sky ACZ11BR1E-20FD1-12C encoder](https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices/ACZ11BR1E-20FD1-12C/1923355) | 1 | 102-1760-ND | $4.33 / $4.33 |
| [Same Sky SJ1-3533NG audio jack](https://www.digikey.com/en/products/detail/same-sky-formerly-cui-devices/SJ1-3533NG/738701) | 1 | CP1-3533NG-ND | $1.69 / $1.69 |
| [Vishay BAT85S-TR diodes](https://www.digikey.com/en/products/detail/vishay-semiconductor-diodes-division/BAT85S-TR/BAT85SCT-ND/3104371) | 4 | BAT85SCT-ND | $0.50 / $2.00 |
| [Vishay K222J15C0GF5TH5 filter capacitors](https://www.digikey.com/en/products/detail/vishay-beyschlag-draloric-bc-components/K222J15C0GF5TH5/286504) | 2 | BC1050CT-ND | $0.52 / $1.04 |
| [Kingbright WP710A10LZGCK green LED](https://www.digikey.com/en/products/detail/kingbright/WP710A10LZGCK/5177416) | 1 | 754-1915-ND | $0.59 / $0.59 |
| [B3F-1000 boot/reset buttons](https://www.digikey.com/en/products/detail/omron-electronics-inc-emc-div/B3F-1000/33150) | 2 if needed | SW400-ND | $0.35 / $0.70 |

Listed subtotal is **$14.05 without buttons / $14.75 with both**. The older
parts worksheet budgets $0.59 per filter capacitor, making its conservative
subtotal $14.89 including both buttons. All seven listings show stock, but
live cart availability/price and fulfilment are the purchase check. Keep one
DigiKey shipment; do not add a separately shipped marketplace offer by accident.
These subtotals exclude shipping, tax and any checkout tariffs, and exclude
the processor and any missing inventory items below.

**CT means cut tape:** individual parts cut from a reel. Select CT for the four
diodes and two small capacitors; do not buy a full reel. The filter capacitors
must be the **H5 / 5 mm lead-spacing** variant. The 12-detent encoder is the
reviewed Same Sky part, not the superseded Bourns or a generic EC11.

## Processor purchase

One **Pimoroni PGA2350 / PIM722, with A4 silicon**. The user will confirm A4
before purchase. [PiShop's listing](https://www.pishop.us/product/pga2350/)
shows $11.95 and in stock, but does **not establish A4**. A separate module
shipment remains the previously accepted exception to supplier consolidation.
Do not substitute a Pico, another module or A2 silicon to fill a cart.

Module plus the seven-line DigiKey list is **$26.70 at the listed prices**,
before shipping/tax, headers, missing resistors and any optional accessories.
This is a component subtotal, not a complete delivered build price.

## Use owned inventory

- Existing serial LCD/backpack, BSS138 converter and female-ended LCD cable.
  Converter orientation is now confirmed; no replacement module needed.
- One 470 µF electrolytic and two 10 µF electrolytics; the user confirmed fit.
- Axial resistors, **1% tolerance, ¼ W**, with bodies fitting the ordered board:
  **5.1k ×2, 22k ×6, 22 Ω ×2, 10k ×2, 1k ×1**. Reuse the kit; buy only missing
  values. Tolerance describes the permitted deviation from the marked value.
- Male/female headers from the owned assortment. Contact counts below are
  individual pins/sockets, not packaged strip quantities:

| Connection | Contacts / intended pieces |
|---|---|
| PGA2350 male pins | 64 male contacts around the module perimeter |
| Carrier PGA sockets | 64 female contacts: two 2×10 and two 2×6 pieces; see socket-frame.svg |
| USB breakout male pins | 16 male contacts: two 1×8 pieces, unless supplied installed |
| Carrier USB sockets | 16 female contacts: two 1×8 pieces |
| Carrier converter sockets | 12 female contacts: two 1×6 pieces; existing converter pins reused |
| LCD header | Four male contacts: one 1×4 piece |

Use single-row sockets for the USB and converter positions. Extra unused rows
from a double-row body are not automatically a fit. The PGA frame uses the
owned double-row strips on the accepted dimensional/paper-fit basis; cutting
and actual module mating remain assembly steps. Don't order a new header
assortment or cut female strips without first choosing the actual pieces.

Keep the working breadboard intact while gathering spares where possible.
No new fit questionnaire or repeat breadboard power testing is introduced.

## Cables and optional finishing parts

The new keyboard-facing connector is USB-C: have a known **USB data** cable
with USB-C at both ends for carrier-to-Moonlander. Reuse the known working
hub-to-controller cable for upstream if its ends suit the hub. Charging-only
cables are not useful for keyboard data; don't assume a spare cable works
solely from its connector shape.

The encoder takes a **6 mm flat/D shaft** knob. The Taiss cap's fit is unknown;
do not assume it matches. A matching cap can be sourced or printed later;
the encoder is usable by its shaft during bring-up. Mounting screws/spacers
and an enclosure are finishing work after the actual board arrives; no extra
parts for those are added to this order.
