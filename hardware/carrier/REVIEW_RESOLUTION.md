# Response to Astra carrier review — 2026-11-07

**Follow-up verdict: GO for ordering the current bare-board prototype.** See
`ASTRA_CARRIER_FOLLOWUP.md`, reviewed board hash
`73a8fa9c8e2926b336ab9a96fe3c684fc772ab46f2bac94f24df9b723bde404f`.
The order-named ZIP in `../fabrication/carrier-order-20261107/` is identical to
the reviewed Gerbers; no board or copper changes followed that review. The
paragraphs below retain the correction/acceptance history. No order placed.

The reviewed snapshot and `ASTRA_CARRIER_REVIEW.md` remain unchanged. The new
working carrier incorporates the corrections below. **Still a review draft:
do not order a superseded ZIP. A focused independent follow-up is next.**
OSH Park is the selected bare-board supplier; no factory assembly or order is
authorized. Board outline, processor choice, accepted power design and firmware
scope are unchanged.

| Review item | Disposition |
|---|---|
| 1. SW3 mounting-slot axes/shape | Superseded the unpurchased Bourns with Same Sky ACZ11BR1E-20FD1-12C. New independent footprint uses the manufacturer's top-view coordinates, 1.2 mm electrical holes and 9.4 mm shell-centre spacing. Deliberate 3.2 mm round shell holes enclose its recommended 1.8 × 2.1 mm rectangles; this is documented rather than claimed identical to the manufacturer's shape. See ENCODER.md. |
| 2. U2 / sockets / capacitor leads | **User observations/acceptance recorded.** Both converter rows are confirmed as 4,3,GND,supply,2,1 and the footprint/routing are corrected. User reports capacitors fit. User accepts published processor dimensions plus earlier paper fit for ordering; full socket/module mating is an assembly step, not claimed performed. |
| 3. Encoder hand-solder mismatch | Replacement manufacturer explicitly permits hand soldering within 350 °C / three-second maximum limits. Parts worksheet and assembly guide updated. Actual soldered operation is a future assembly check. |
| 4. Audio source/powered-off limits | Kept the accepted circuit. Assembly guide now states the intended power/audio sequence and distinguishes ideal clipping headroom from a validated source rating. Hot-input and unpowered safety are not claimed. Source ceiling/clipping checks remain carrier bring-up work. |
| Stale “unrouted” wording | Corrected in the schematic generator and saved schematic, retaining review-draft/unreleased status. Updated schematic PDF is in the new export. |

## Evidence and accepted assembly steps

- **U2, confirmed by the user November 7:** labelled side up, HV on the left.
  Left row top-to-bottom is **HV4, HV3, GND, HV, HV2, HV1**. The user explicitly
  confirmed the right row is **LV4, LV3, GND, LV, LV2, LV1**. This reverses the
  earlier assumed pad order on both sides. Ground is third and supply fourth;
  the corrected board includes HV4/LV4 top orientation marks. All twelve pad
  coordinates are independently audited, including supplies and grounds.
- **Capacitors:** user reports “caps fit,” closing C1/C2/C4 lead-fit questions.
  The 3.5 mm / 2.5 mm pitches and existing body reservations are retained.
- **Processor:** user explicitly accepts the verified online specification and
  earlier paper fit as the order basis. The manufacturer drawing shows a
  25.4 mm square body and 2.54 mm pitch; all 64 pad assignments were checked
  in the original review. Complete cut-strip frame construction, contact
  mating depth and underside clearance have not been physically demonstrated;
  these remain assembly steps, not new pre-order measurement requests.
- **A4 silicon:** user will ensure A4 before buying the PGA2350. Stock is not
  claimed confirmed, but there is no additional supplier-answer gate imposed
  on this bare-board order decision.

The replacement encoder's actual dry fit/soldered operation and the PGA2350
firmware port remain build/bring-up work. The round mounting-hole choice is
supported by the geometry/tolerance comparison in ENCODER.md; no sample fit
is claimed. No new breadboard tests, expanded power circuit or optional parts
were introduced. User-owned socket dimensions are not falsely described as
manufacturer-verified; the accepted processor basis is the module drawing and
paper check.

## Verification and current artifacts

KiCad 10.0.6, zones refilled: **0 violations, 0 unconnected items, 0 schematic
parity findings**. No exclusions added. The independent placement audit now
also rejects wrong shell centres, the previous transposed slot and undersized
encoder electrical holes and reversed converter rows. Logical net connectivity is unchanged; U2's physical pad/net order is corrected
to match the observed module. Five encoder electrical pad centres and shaft
position are retained. Shell centres, contact drills
and encoder body/markings intentionally differ from the old part.

New export directory: `../fabrication/carrier-followup-20261107/`.
It supersedes the earlier quote ZIP; it does not overwrite the reviewed
`carrier-review-20261107` snapshot. No purchase, upload, firmware flash or commit
has been made. A focused follow-up prompt is in ASTRA_FOLLOWUP_PROMPT.md. The requested
review concerns the new encoder, corrected converter mapping and original
finding closure; it does not request another broad design or new tests.
