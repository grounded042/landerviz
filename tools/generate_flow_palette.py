#!/usr/bin/env python3
"""Emit FLOW v2 palette stops authored in OKLCH and converted to HSV.

The script uses only the Python standard library. Each hue starts at OKLCH
lightness 0.65 and the largest in-gamut chroma, with a 4% safety margin for
rounding. Runtime code consumes the emitted HSV values without doing color-
space conversion on the RP2040.
"""

import colorsys
import math


LIGHTNESS = 0.65
HUES = (20, 50, 85, 135, 180, 220, 260, 300, 335)


def linear_to_srgb(value):
    if value <= 0.0031308:
        return 12.92 * value
    return 1.055 * value ** (1.0 / 2.4) - 0.055


def oklch_to_srgb(lightness, chroma, hue_degrees):
    hue = math.radians(hue_degrees)
    a = chroma * math.cos(hue)
    b = chroma * math.sin(hue)
    l_value = (lightness + 0.3963377774 * a + 0.2158037573 * b) ** 3
    m_value = (lightness - 0.1055613458 * a - 0.0638541728 * b) ** 3
    s_value = (lightness - 0.0894841775 * a - 1.291485548 * b) ** 3
    return (
        linear_to_srgb(
            4.0767416621 * l_value - 3.3077115913 * m_value
            + 0.2309699292 * s_value
        ),
        linear_to_srgb(
            -1.2684380046 * l_value + 2.6097574011 * m_value
            - 0.3413193965 * s_value
        ),
        linear_to_srgb(
            -0.0041960863 * l_value - 0.7034186147 * m_value
            + 1.7076147010 * s_value
        ),
    )


def safe_chroma(hue):
    low = 0.0
    high = 0.3
    for _ in range(30):
        candidate = (low + high) / 2.0
        rgb = oklch_to_srgb(LIGHTNESS, candidate, hue)
        if min(rgb) >= 0.0 and max(rgb) <= 1.0:
            low = candidate
        else:
            high = candidate
    return low * 0.96


previous_hue = None
for palette_hue in HUES:
    chroma = safe_chroma(palette_hue)
    red, green, blue = oklch_to_srgb(LIGHTNESS, chroma, palette_hue)
    hsv_hue, saturation, value = colorsys.rgb_to_hsv(red, green, blue)
    hsv_hue *= 256.0
    if previous_hue is not None:
        while hsv_hue < previous_hue:
            hsv_hue += 256.0
    previous_hue = hsv_hue
    rgb8 = tuple(round(channel * 255.0) for channel in (red, green, blue))
    print(
        f"{{{hsv_hue:.2f}f, 245.0f, 255.0f}},  "
        f"// source OKLCH C={chroma:.4f}; RGB={rgb8}; "
        f"HSV S={saturation * 255.0:.2f} V={value * 255.0:.2f}"
    )
