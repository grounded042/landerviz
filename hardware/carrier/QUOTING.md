# Carrier bare-board pricing — 2026-11-07

**Order placed:** the user selected and ordered OSH Park after the $88 JLCPCB
comparison. Reported shipping window is 12–21 days. See ORDER_STATUS.md.
The quote instructions below remain as provenance, not a request to reorder.

**Astra follow-up: GO for the current bare-board prototype.** The verdict in
`ASTRA_CARRIER_FOLLOWUP.md` names the current board hash below. The order-named
manufacturing ZIP is
`hardware/carrier/releases/rev-a/landerviz-carrier-RevA-gerbers.zip`.
It is byte-for-byte identical to the reviewed Gerber ZIP. Use it for OSH Park
or a JLCPCB bare-board price comparison: 90 × 75 mm, two layers, 1.6 mm,
FR-4, 1 oz copper, no assembly/stencil. Vendor previews/settings and the final
delivered price need checking for any future comparison. The OSH Park order
has been placed; this guide records the process used.

**Ready to obtain pricing now.** Component purchasing, the remaining socket
details and the PGA2350 firmware port do not block a bare-board quote.
They are separate from authorizing manufacture or purchasing anything.

**Selected supplier: OSH Park**, per the user's November 7 decision. Order
bare carrier boards and hand-solder the components. CircuitHub is parked;
no factory assembly is requested. The original Astra review has been completed. Its corrections and remaining
user confirmations are tracked in REVIEW_RESOLUTION.md. The corrected encoder
and converter mapping passed the focused independent follow-up. The original frozen review snapshot is superseded by the encoder/converter corrections;
do not order its old ZIP. The main source files and upload path below are current.

## File to upload

`hardware/carrier/releases/rev-a/landerviz-carrier-RevA-gerbers.zip`

This frozen export contains seven Gerber layers (top/bottom copper, protective
solder mask and printed markings, plus the board outline), two drill files. Quote/review notes stay outside the ZIP so importers do not
mistake a text note for a drill file. Gerber is the manufacturing drawing format for each layer.
The drills distinguish plated holes, which conduct through the board, from
unplated mounting holes.

The export is from the current **carrier**, not the preserved four-layer
bare-chip board. No assembly BOM, component-placement file, solder-paste layer
or stencil is needed. Select **bare PCB / no assembly** in the quote form.

| Setting | Current carrier |
|---|---|
| Outline | 90 × 75 mm rectangle |
| Copper layers | 2 |
| Thickness | 1.6 mm nominal |
| Material | Standard FR-4 glass/epoxy |
| Copper | 1 oz per outer layer |
| Smallest track / design clearance | 0.25 / 0.20 mm |
| Smallest drill | 0.40 mm |
| Special features | All holes round; four dedicated 3.2 mm M3 mounting holes |
| Finish / color for comparison | Vendor standard |
| Assembly / stencil / filled vias / impedance-control service | None requested |

The outline file is named `.gko` inside the ZIP for quote-form recognition;
its geometry is identical to KiCad's exported `.gm1`. The job metadata is
excluded because it describes the stroked bounding box as 90.05 × 75.05 mm;
the actual outline centre lines are **90 × 75 mm**. Confirm the uploaded preview
detects two copper layers and approximately those dimensions.

Snapshot source board SHA-256:
`73a8fa9c8e2926b336ab9a96fe3c684fc772ab46f2bac94f24df9b723bde404f`.
The export directory retains the source snapshot, checks, drill report and
manifest. Verified: seven Gerber layers, two drill files, copper layer numbering,
90 × 75 mm outline, matching copper/drill coordinates at three named pads and
an intact ZIP. The source's saved rules check has zero findings in all three
lists; exporting did not change the source board.

## US options including standard shipping

The rectangular area is 10.46252 square inches. The original estimates below use published rates. On November 7 the user
uploaded the previous quote snapshot to OSH Park and received **$52.30 for
three bare boards**. The new mounting/marking revision keeps the same outline;
the user subsequently ordered the reviewed revision. Parts, assembly, expedited shipping and
any applicable tax are excluded.

| Service | Copies | Published-rate estimate | Standard US shipping | Standard appearance |
|---|---:|---:|---|---|
| OSH Park two-layer | 3 | **$52.31 total** | Free USPS option | Purple / white |
| DigiKey DKRed two-layer | 4 | **$62.78 total** | Free | Red / white |

**OSH Park is selected.** Its published track/space, drill, annular-ring and edge-clearance
limits accommodate the current draft rules. The replacement encoder uses round
mounting holes; no slots remain. Manufacturer review still determines
acceptance; this comparison is not a fabrication sign-off.

DKRed remains a historical pricing comparison, not the selected supplier.
The older slot-support concern no longer applies to the corrected round-hole
encoder footprint; no alternate supplier/order is being pursued.

Sources checked 2026-11-06:

- [OSH Park two-layer pricing, US manufacture and capabilities](https://docs.oshpark.com/services/two-layer/),
  [free domestic USPS option](https://docs.oshpark.com/submitting-orders/shipping-information/),
  [slot file support](https://docs.oshpark.com/submitting-orders/slots/).
- [DKRed current service and shipping](https://www.digikey.com/en/resources/dkred),
  [official explanation of $1.50/in² per board and four-board minimum](https://forum.digikey.com/t/dkred-an-extension-of-digikeys-pcb-builder/12855),
  [older official board document listing no slots](https://cdck-file-uploads-global.s3.dualstack.us-west-2.amazonaws.com/digikey/original/3X/9/0/90f3b4ab3ed7fb57f6a79320dbee6601b6981307.pdf).

The final cart total is the comparison point for any overseas quote too:
boards + shipping + duties/fees. A low fabrication headline alone is insufficient.
The roughly $260 assembled-board quotes from the earlier design are not prices
for this carrier. The reviewed revision is now ordered from OSH Park; see
[ORDER_STATUS.md](ORDER_STATUS.md).

## CircuitHub: historical quote preparation, not selected

Their [KiCad import guide](https://docs.circuithub.com/importing-a-project/kicad-support)
supports stable KiCad 10 and requires the project (`.kicad_pro`), board
(`.kicad_pcb`) and schematic (`.kicad_sch`). Use the three files in
`hardware/fabrication/carrier-review-20261107/circuithub/`, or their separate
`landerviz-carrier-QUOTE-ONLY-circuithub.zip`. They describe only this carrier,
not the old bare-chip design or the unrouted placement snapshot.

The saved board has current copper fills and passed schematic parity checks.
An assembly BOM (bill of materials) is unnecessary for our bare-board comparison:
after import, manually mark every part **Do Not Place**. Do not silently turn
this into an assembled-module quote. Compare the delivered total, not the
fabrication headline. Their [board-only instructions](https://docs.circuithub.com/ordering/board-only-orders)
say bare boards ship still joined in a manufacturing panel; determine how they
would separate or supply this panel before choosing them. No upload/order is
performed by our export scripts. Requirements checked 2026-11-07.
