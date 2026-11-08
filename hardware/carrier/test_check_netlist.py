"""Mutation checks against an actual KiCad export; no schematic mutations.

Usage: python3 test_check_netlist.py path/to/exported-netlist.xml
"""
import copy
from pathlib import Path
import sys
import unittest
import xml.etree.ElementTree as ET

from check_netlist import checks

NETLIST = Path(sys.argv.pop(1)) if len(sys.argv) > 1 else None


class NetlistAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if NETLIST is None:
            raise RuntimeError('Pass the exported netlist path as first argument')
        cls.original = ET.parse(NETLIST).getroot()

    def setUp(self):
        self.root = copy.deepcopy(self.original)

    def move(self, ref, pin, destination):
        node = None
        for net in self.root.findall('nets/net'):
            for candidate in net.findall('node'):
                if candidate.get('ref') == ref and candidate.get('pin') == pin:
                    node = candidate
                    net.remove(node)
                    break
        self.assertIsNotNone(node)
        target = next(net for net in self.root.findall('nets/net') if net.get('name') == '/'+destination)
        target.append(node)

    def assert_rejected(self, name):
        result = next(c for c in checks(self.root) if c['name'] == name)
        self.assertFalse(result['pass'], name)

    def test_clean_export_passes(self):
        failures = [c for c in checks(self.root) if not c['pass']]
        self.assertEqual(failures, [])

    def test_reversed_coupling_capacitor_rejected(self):
        self.move('C2','1','LINE_L')
        self.move('C2','2','BIAS_L')
        self.assert_rejected('L audio bias, polarity, filter and ADC pin')

    def test_reversed_upper_clamp_rejected(self):
        self.move('D1','1','ADC_L')
        self.move('D1','2','+3V3')
        self.assert_rejected('L audio bias, polarity, filter and ADC pin')

    def test_shared_stereo_bias_rejected(self):
        self.move('C4','1','BIAS_L')
        self.assert_rejected('Audio bias nodes are distinct and isolated from 5V')

    def test_wrong_source_cc_role_rejected(self):
        self.move('R3','2','GND')
        self.assert_rejected('J2 USB-C pin order and independent CC terminations')

    def test_audio_pinned_to_wrong_adc_rejected(self):
        self.move('U1','GP40','ADC_R')
        self.assert_rejected('L ADC connects only to filter/clamps/module')

    def test_direct_5v_connection_to_adc_rejected(self):
        self.move('R9','1','+5V')
        self.assert_rejected('Audio bias nodes are distinct and isolated from 5V')

    def test_reversed_status_led_rejected(self):
        self.move('D5','1','STATUS_LED_A')
        self.move('D5','2','GND')
        self.assert_rejected('Status LED polarity and current limiting')

    def test_status_led_resistor_bypass_rejected(self):
        self.move('D5','2','STATUS_LED_GP6')
        self.assert_rejected('Status LED polarity and current limiting')

    def test_wrong_status_led_resistor_rejected(self):
        self.root.find('./components/comp[@ref="R13"]/value').text='22R'
        self.assert_rejected('Status LED polarity and current limiting')

    def test_status_led_powered_from_5v_rejected(self):
        self.move('R13','1','+5V')
        self.assert_rejected('Status LED polarity and current limiting')

    def test_swapped_audio_jack_channels_rejected(self):
        self.move('J3','T','LINE_R')
        self.move('J3','R','LINE_L')
        self.assert_rejected('TRS jack channels')


if __name__ == '__main__':
    unittest.main()
