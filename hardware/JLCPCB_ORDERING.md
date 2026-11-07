# JLCPCB prototype handoff

Status: **files prepared locally; manufacturer process and assembly rotations
not approved; no upload, message, payment or order sent**.

The current checked snapshot is `fabrication/marking-20261105/`.
Its large review ZIP contains PDFs, sources and evidence; it is **not** the
Gerber upload. The separate upload files are in
`fabrication/jlc-marking-20261105/`:

| File | Purpose |
|---|---|
| `landerviz-gerbers.zip` | Board copper, solder mask, marking, top paste and outline, plus separate plated/unplated drill files. Twelve files, no PDFs or sources. |
| `landerviz-bom.csv` | Bill of materials (BOM): exact part and catalog number for each of 70 populated positions. |
| `landerviz-cpl.csv` | Component placement list (CPL): matching 70 references, position in millimetres, top side and native rotation. |
| `manifest.json` | File hashes and provenance; retain locally, not an assembly input. |

R22/R23 are not populated. Test pads and mounting holes are not assembly
components. Both surface-mount and purchased through-hole parts are included.
Rotations have only been normalized into 0–360°; no speculative vendor offsets
were applied. KiCad's native X / negative Y coordinates match the Gerbers and
drills; do not independently shift the CPL origin. The converter checks the
native audit against placements and tagged Gerber pad flashes.

## Order sequence

1. Send the draft [assembly review request](ASSEMBLY_REVIEW_REQUEST.md) with
   the current review files. Get written acceptance of the U1/U2 unfilled
   exposed-pad holes, all three chips' stencil patterns, modified USB lands,
   small drills/slots, physical L1 orientation, and the exact processor stepping.
   This is manufacturing acceptance, not a circuit-design guarantee.
2. Upload **`landerviz-gerbers.zip`** for a small prototype quote. Confirm
   48 × 42 mm, four layers, nominal 1.6 mm, blue mask / white marking,
   JLC04161H-3313 stack. Finish, stencil/via process and any surcharge need
   an explicit quote; do not assume a paid filled-via process.
3. Select top-side assembly and upload BOM/CPL. Inspect the exact catalog
   matches, stock, assembly method for the audio jack/encoder/JST connector,
   substitutions and total price. No automatic nominal-value substitutions
   for the processor or qualified regulator capacitors.
4. Inspect the placement preview: chip pin 1, L1's physical dot toward +1V1,
   electrolytic polarity, USB/audio connectors, encoder and outward-facing
   LCD connector. Check the preview against `board-layers.pdf`, not just CSV
   angle values. Resolve any vendor rotation correction before approval.
5. Approve the concrete process, files and quote before payment/production.
   First boards still require raw-chip firmware bring-up and the assembled
   power-stage measurements in [review resolution](REVIEW_RESOLUTION.md).

## Reproduce without plugins

Use installed KiCad and Python's standard library; no package downloads.
The scripts refuse to overwrite existing output directories. Choose a fresh
snapshot name after any design or source change; old review packages stay frozen.

```sh
/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3 \
  hardware/prepare_review.py --snapshot-name marking-20261105 \
  --references /tmp/landerviz-inrush-references
python3 hardware/prepare_jlc.py \
  hardware/fabrication/marking-20261105 \
  hardware/fabrication/jlc-marking-20261105
```

The optional reference directory is local to this review session; omit that
argument if the downloaded datasheets are unavailable. Generated outputs are
ignored by Git. Keep the source/manifest with the eventual accepted order.

Format references, checked 2026-11-04:
[JLCPCB BOM](https://jlcpcb.com/help/article/bill-of-materials-for-pcb-assembly),
[JLCPCB placement file](https://jlcpcb.com/help/article/pick-place-file-for-pcb-assembly),
[assembly order process](https://jlcpcb.com/help/article/how-do-i-place-a-pcba-order).
