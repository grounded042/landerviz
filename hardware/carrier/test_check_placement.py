"""Fault injection against the actual saved board; no files are modified."""
from pathlib import Path
import sys
import unittest

import pcbnew as pcb

from check_placement import check

NETLIST=Path(sys.argv.pop(1))
BOARD=Path(__file__).with_name('placement-source.kicad_pcb')
# Preserve native owners/wrappers through the suite in KiCad 10.0.6 on macOS.
# Collecting a removed zone can invalidate SWIG's type lookup for later loads.
KEEP_ALIVE=[]


class PlacementAudit(unittest.TestCase):
    def setUp(self):
        self.board=pcb.LoadBoard(str(BOARD))
        self.fps={f.GetReference():f for f in self.board.GetFootprints()}
        KEEP_ALIVE.extend((self.board,self.fps))

    def test_clean_study(self):
        self.assertEqual(check(self.board,NETLIST),[])

    def test_wrong_usb_module_rotation(self):
        self.fps['J2'].SetOrientationDegrees(0)
        self.assertTrue(any('reference-coordinate' in e for e in check(self.board,NETLIST)))

    def test_wrong_encoder_shell_location(self):
        pad=next(p for p in self.fps['SW3'].Pads() if p.GetNumber()=='SH')
        pad.Move(pcb.VECTOR2I(pcb.FromMM(1),0))
        self.assertTrue(any('encoder shell coordinate' in e for e in check(self.board,NETLIST)))

    def test_transposed_old_encoder_slot(self):
        pad=next(p for p in self.fps['SW3'].Pads() if p.GetNumber()=='SH')
        pad.SetDrillShape(pcb.PAD_DRILL_SHAPE_OBLONG)
        pad.SetDrillSize(pcb.VECTOR2I(pcb.FromMM(2.1),pcb.FromMM(2)))
        self.assertTrue(any('round clearance holes' in e for e in check(self.board,NETLIST)))

    def test_undersized_encoder_terminal_drill(self):
        pad=next(p for p in self.fps['SW3'].Pads() if p.GetNumber()=='A')
        pad.SetDrillSize(pcb.VECTOR2I(pcb.FromMM(1),pcb.FromMM(1)))
        self.assertTrue(any('encoder terminal drill' in e for e in check(self.board,NETLIST)))

    def test_wrong_adc_net(self):
        pads={p.GetNumber():p for p in self.fps['U1'].Pads()}
        pads['GP40'].SetNet(pads['GP41'].GetNet())
        self.assertTrue(any('U1.GP40: net' in e for e in check(self.board,NETLIST)))

    def test_reversed_converter_row(self):
        # Reintroduce the old assumption without changing pad nets: ERC alone
        # would accept it, but it disagrees with the observed physical module.
        fp=self.fps['U2'];cy=fp.GetPosition().y
        for pad in fp.Pads():
            pos=pad.GetPosition()
            pad.SetPosition(pcb.VECTOR2I(pos.x,2*cy-pos.y))
        self.assertTrue(any('U2' in e and 'reference-coordinate' in e for e in check(self.board,NETLIST)))

    def test_colliding_modules(self):
        self.fps['U2'].SetPosition(self.fps['U1'].GetPosition())
        self.assertTrue(any('courtyard envelopes overlap' in e for e in check(self.board,NETLIST)))

    def test_missing_mounting_hole(self):
        self.board.Remove(self.fps['H4'])
        self.assertTrue(any('missing mounting hole' in e for e in check(self.board,NETLIST)))

    def test_missing_soldermask_opening(self):
        pad=next(iter(self.fps['R13'].Pads()))
        layers=pcb.LSET(pad.GetLayerSet());layers.RemoveLayer(pcb.F_Mask)
        pad.SetLayerSet(layers)
        self.assertTrue(any('solder-mask opening' in e for e in check(self.board,NETLIST)))

    def test_wrong_mounting_drill(self):
        next(iter(self.fps['H4'].Pads())).SetDrillSize(pcb.VECTOR2I(pcb.FromMM(2),pcb.FromMM(2)))
        self.assertTrue(any('mounting drill' in e for e in check(self.board,NETLIST)))

    def test_mounting_hole_must_be_unplated(self):
        next(iter(self.fps['H4'].Pads())).SetAttribute(pcb.PAD_ATTRIB_PTH)
        self.assertTrue(any('unplated' in e for e in check(self.board,NETLIST)))

    def test_missing_hardware_keepout(self):
        area=next(z for z in self.board.Zones() if z.GetZoneName()=='M3 clearance H1')
        self.board.Remove(area)
        KEEP_ALIVE.append(area)
        self.assertTrue(any('missing hardware keepout' in e for e in check(self.board,NETLIST)))

    def test_moved_mounting_hole(self):
        self.fps['H1'].Move(pcb.VECTOR2I(pcb.FromMM(1),0))
        self.assertTrue(any('mounting coordinate' in e for e in check(self.board,NETLIST)))


if __name__=='__main__':
    unittest.main()
