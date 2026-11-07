"""Validate independent equations and waveform measurements used in the report."""

import math
import unittest

from check_candidate import audio_transfer, crossing


class AnalysisTests(unittest.TestCase):
    def test_midband_resistive_limit(self):
        # Shorted coupling cap / open filter cap reduce this to two dividers.
        source, upper, lower, series, adc = 1000, 22000, 22000, 10000, 100000
        bias = 1 / (1 / upper + 1 / lower + 1 / (series + adc))
        expected = bias / (source + bias) * adc / (series + adc)
        actual = audio_transfer(100, source, upper, lower, series, 1e9, 1e-20, adc)
        self.assertAlmostEqual(actual.real, expected, places=10)
        self.assertAlmostEqual(actual.imag, 0, places=10)

    def test_coupling_cap_rejects_dc(self):
        self.assertLess(abs(audio_transfer(1e-9)), 1e-8)

    def test_shunt_cap_rejects_high_frequency(self):
        self.assertLess(abs(audio_transfer(1e12)), 1e-7)

    def test_small_signal_passive_network_never_has_gain(self):
        for index in range(100):
            frequency = math.pow(10, -2 + index / 10)
            self.assertLessEqual(abs(audio_transfer(frequency)), 1)

    def test_crossing_interpolates_first_rising_edge(self):
        self.assertEqual(crossing([[0, 0], [2, 4], [4, 0], [6, 4]], 1, 3), 1.5)

    def test_missing_crossing_is_not_reported_as_success(self):
        with self.assertRaises(ValueError):
            crossing([[0, 0], [1, 1]], 1, 2)


if __name__ == '__main__':
    unittest.main()
