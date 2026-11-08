# Carrier order status — 2026-11-07

The user reports **the OSH Park bare-board order is placed**, with a quoted
**12–21 day shipping window**. This is the user's shipping estimate, not a
confirmed delivery date. The earlier quote was $52.30 for three boards; no
receipt/order number or final paid amount is recorded here.

Ordered design reference: **Carrier Rev A, 90 × 75 mm, two layers**.
The released manufacturing package is preserved in Git:
[landerviz-carrier-RevA-gerbers.zip](releases/rev-a/landerviz-carrier-RevA-gerbers.zip).
Its [release manifest and notes](releases/rev-a/README.md) record the checks
and manufacturing settings. The ignored working snapshot remains at
`hardware/fabrication/carrier-order-20261107/`.

- Board SHA-256: `73a8fa9c8e2926b336ab9a96fe3c684fc772ab46f2bac94f24df9b723bde404f`
- Manufacturing ZIP SHA-256: `6efba88ea4c479ada24e96dabca6f6cfd9fdf925c13644ab10652f613084da2b`
- Independent follow-up: `ASTRA_CARRIER_FOLLOWUP.md`, GO for the prototype.

Keep the manufacturing snapshots and ordered hardware unchanged. A subsequent
hardware revision belongs in a separate working copy/revision. Parts sourcing
must match this board; no substitute encoder, USB breakout or jack is assumed
to fit merely because its description sounds similar.

Next: [source the remaining components](SHOPPING.md), then prepare the separate
PGA2350 firmware port and assembly/bring-up while the boards are in production.
The user will ensure the PGA2350 has A4 silicon before buying it. No other
component purchases or external cart changes were made by the agent.
