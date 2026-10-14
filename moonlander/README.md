# Moonlander receiver

The baseline was downloaded from the public
[Oryx layout](https://configure.zsa.io/moonlander/layouts/z9wlG/ARDpK/0) after
compiling revision `ARDpK` in the ZSA web UI with ZSA QMK Firmware 25. Oryx
provided `zsa_moonlander_reva_z9wlG_ARDpK_coding_source.zip` on 2026-10-12.
Only the five text files from its `zsa_moonlander_coding_source` directory are
vendored under `oryx/`; the generated firmware binaries, checksums, and build
log are intentionally not tracked.

The Nix build copies that source snapshot, applies `oryx-ARDpK.patch`, and adds
`landerviz.c`, `landerviz.h`, and the shared protocol header. To refresh the
layout, compile it again in Oryx and download the source ZIP, replace the five
files in `oryx/`, then review or regenerate the patch and update the documented
layout revision and pinned ZSA firmware inputs as needed. Run `nix flake check`
before flashing. Source ZIPs are ignored so a new export cannot accidentally
add its generated binaries or build log to Git.

The patch disables the Oryx Raw HID module and assigns that vendor-defined USB
channel to LanderViz. It does not remove normal keyboard, mouse, media-key,
system-control, or N-key-rollover reports. Keymapp's live layout display and
Oryx Live Training are therefore unavailable, which is intentional for this
build.

LanderViz sends each color frame as eight 32-byte Raw HID packets. Each packet
carries nine consecutive RGB pixels and a frame sequence number. The receiver
publishes only after all eight chunks arrive. If no complete frame arrives for
500 ms, the original Oryx layer lighting resumes.

Both received colors and ordinary QMK-generated colors are capped at 175/255.
The output target is `zsa/moonlander/reva`; verify the physical board revision
before flashing `zsa_moonlander_reva_landerviz.bin`.
