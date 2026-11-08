"""Audit the saved study against the exported schematic and critical mechanics.

Use KiCad's bundled Python. This is not a routing/manufacturing release check.
Critical coordinates are independent spot checks against manufacturer drawings,
chosen placement and the recorded U2 breadboard fit, not imported from the
footprint generator's pin table.
"""
import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import pcbnew as pcb


def check(board, netlist, allow_routing=False):
    root=ET.parse(netlist).getroot()
    fps={f.GetReference():f for f in board.GetFootprints()}
    comps={c.get('ref'):c for c in root.findall('./components/comp')}
    errors=[]
    mounts={'H1':(27.25,12.0),'H2':(63.0,11.75),'H3':(24.5,58.5),'H4':(86.0,71.0)}
    if set(fps)-set(mounts)!=set(comps): errors.append('Board and schematic component sets differ')
    for ref,(x,y) in mounts.items():
        fp=fps.get(ref)
        if fp is None:
            errors.append(ref+': missing mounting hole');continue
        pos=fp.GetPosition()
        if abs(pcb.ToMM(pos.x)-50-x)>.001 or abs(pcb.ToMM(pos.y)-50-y)>.001:
            errors.append(ref+': mounting coordinate differs')
        required=pcb.FP_BOARD_ONLY|pcb.FP_EXCLUDE_FROM_BOM|pcb.FP_EXCLUDE_FROM_POS_FILES
        if fp.GetAttributes() & required != required:
            errors.append(ref+': mounting hole must be board-only and excluded from assembly')
        holes=list(fp.Pads())
        if len(holes)!=1 or holes[0].GetAttribute()!=pcb.PAD_ATTRIB_NPTH:
            errors.append(ref+': mounting hole must have one unplated drill')
        elif any(abs(pcb.ToMM(v)-3.2)>.001 for v in (holes[0].GetDrillSize().x,holes[0].GetDrillSize().y)):
            errors.append(ref+': mounting drill must be 3.2 mm')
        areas=[z for z in board.Zones() if z.GetIsRuleArea() and z.GetZoneName()=='M3 clearance '+ref]
        if len(areas)!=1:
            errors.append(ref+': missing hardware keepout');continue
        area=areas[0]
        if not all(area.IsOnLayer(layer) for layer in (pcb.F_Cu,pcb.B_Cu)) or not all((
                area.GetDoNotAllowTracks(),area.GetDoNotAllowVias(),area.GetDoNotAllowZoneFills())):
            errors.append(ref+': hardware keepout must protect both copper layers')
        box=area.GetBoundingBox()
        actual=[pcb.ToMM(box.GetLeft())-50,pcb.ToMM(box.GetTop())-50,
                pcb.ToMM(box.GetRight())-50,pcb.ToMM(box.GetBottom())-50]
        if any(abs(a-b)>.03 for a,b in zip(actual,[x-3.65,y-3.65,x+3.65,y+3.65])):
            errors.append(ref+': hardware keepout geometry differs')
        # NPTH mechanical pads are allowed inside their own rule area. Protect
        # electrical pads independently, including any future copper pad there.
        for other in fps.values():
            for pad in other.Pads():
                if pad.GetAttribute()==pcb.PAD_ATTRIB_NPTH: continue
                pb=pad.GetBoundingBox()
                dx=max(pcb.ToMM(pb.GetLeft())-50-x,0,x-(pcb.ToMM(pb.GetRight())-50))
                dy=max(pcb.ToMM(pb.GetTop())-50-y,0,y-(pcb.ToMM(pb.GetBottom())-50))
                if dx*dx+dy*dy<3.6*3.6:
                    errors.append(ref+': electrical pad inside hardware clearance')
    expected={}
    for net in root.findall('./nets/net'):
        for node in net.findall('node'):
            expected[node.get('ref'),node.get('pin')]=net.get('name')
    pads={}
    for ref,fp in fps.items():
        comp=comps.get(ref)
        if comp is None: continue
        if str(fp.GetFPID().GetLibNickname())+':'+str(fp.GetFPID().GetLibItemName()) != comp.findtext('footprint'):
            errors.append(ref+': schematic footprint mismatch')
        if not fp.GetPath().AsString().endswith('/'+comp.findtext('tstamps')):
            errors.append(ref+': schematic identity mismatch')
        for pad in fp.Pads():
            if not all(pad.IsOnLayer(layer) for layer in (pcb.F_Mask,pcb.B_Mask)):
                errors.append(ref+': through-hole pad is missing a solder-mask opening')
            number=pad.GetNumber()
            if not number or number=='SH': continue
            pads[ref,number]=pad
            actual=pad.GetNetname().replace('{slash}','/')
            if actual != expected.get((ref,number),''):
                errors.append(f'{ref}.{number}: net {actual!r} differs from schematic')
        if fp.GetAttributes() & pcb.FP_SMD: errors.append(ref+': unexpected surface-mount footprint')
    for key in expected:
        if key not in pads: errors.append(str(key)+': schematic pin has no board pad')
    # Global board origin is (50,50) mm; checks below use carrier-relative mm.
    spot_checks={
        ('U1','GP40'):(33.57,23.27),('U1','GP41'):(36.11,25.81),
        ('U1','GP0'):(51.35,30.89),('U1','U+'):(43.73,33.43),
        ('U1','U-'):(41.19,33.43),
        ('J1','1'):(20.5,5.11),('J1','8'):(20.5,22.89),
        ('J2','1'):(69.5,22.89),('J2','8'):(69.5,5.11),
        ('J4','1'):(25.725,64.19),('J4','4'):(25.725,71.81),
        # User confirmed labelled-side HV-left orientation: channel 4 at top,
        # channel 1 at bottom, ground third and supply fourth in BOTH rows.
        ('U2','HV4'):(7.92,57.65),('U2','HV3'):(7.92,60.19),
        ('U2','GND_H'):(7.92,62.73),('U2','HV'):(7.92,65.27),
        ('U2','HV2'):(7.92,67.81),('U2','HV1'):(7.92,70.35),
        ('U2','LV4'):(18.08,57.65),('U2','LV3'):(18.08,60.19),
        ('U2','GND_L'):(18.08,62.73),('U2','LV'):(18.08,65.27),
        ('U2','LV2'):(18.08,67.81),('U2','LV1'):(18.08,70.35),
        ('D5','1'):(43.73,4),('D5','2'):(46.27,4),
        ('J3','S'):(5.7,44),('J3','T'):(8.1,42),('J3','R'):(13.6,42),
    }
    for key,(x,y) in spot_checks.items():
        pad=pads.get(key)
        if pad is None: continue
        point=pad.GetPosition()
        if abs(pcb.ToMM(point.x)-50-x)>.001 or abs(pcb.ToMM(point.y)-50-y)>.001:
            errors.append(str(key)+': reference-coordinate spot check failed')
    # Same Sky ACZ11BR1E-20FD1-12C, manufacturer top-view pattern, p.4.
    # Independent of the footprint generator. The two shell holes deliberately
    # enclose the full 1.8 x 2.1 mm recommended rectangles; see ENCODER.md.
    encoder=fps.get('SW3')
    if encoder:
        if str(encoder.GetFPID().GetLibItemName())!='Encoder_SameSky_ACZ11BR1E_Vertical':
            errors.append('SW3: approved hand-solder encoder footprint required')
        actual={p.GetNumber():p for p in encoder.Pads() if p.GetNumber()!='SH'}
        for number,(x,y) in {'A':(74.5,62.5),'C':(77,62.5),'B':(79.5,62.5),
                             'S1':(74.5,48),'S2':(79.5,48)}.items():
            pad=actual.get(number)
            if pad is None or any(abs(a-b)>.001 for a,b in zip(
                    (pcb.ToMM(pad.GetPosition().x)-50,pcb.ToMM(pad.GetPosition().y)-50), (x,y))):
                errors.append('SW3.'+number+': encoder terminal coordinate differs')
            elif any(abs(pcb.ToMM(v)-1.2)>.001 for v in (pad.GetDrillSize().x,pad.GetDrillSize().y)):
                errors.append('SW3.'+number+': encoder terminal drill must be 1.2 mm')
        shells=sorted((p for p in encoder.Pads() if p.GetNumber()=='SH'),key=lambda p:p.GetPosition().x)
        if len(shells)!=2:errors.append('SW3: two encoder shell holes required')
        for pad,x in zip(shells,(72.3,81.7)):
            if abs(pcb.ToMM(pad.GetPosition().x)-50-x)>.001 or abs(pcb.ToMM(pad.GetPosition().y)-105)>.001:
                errors.append('SW3: encoder shell coordinate differs')
            if pad.GetDrillShape()!=pcb.PAD_DRILL_SHAPE_CIRCLE or any(abs(pcb.ToMM(v)-3.2)>.001
                    for v in (pad.GetDrillSize().x,pad.GetDrillSize().y)):
                errors.append('SW3: encoder shell must use 3.2 mm round clearance holes')
    if len([k for k in pads if k[0]=='U1'])!=64: errors.append('PGA grid is not 64 pins')
    if len(fps)!=36: errors.append('Study must contain 32 electrical positions plus 4 mounting holes')
    if not allow_routing and (len(board.GetTracks()) or any(not z.GetIsRuleArea() for z in board.Zones())):
        errors.append('Study unexpectedly contains routing/zones; use --allow-routing for the routing draft')
    bounds=board.GetBoardEdgesBoundingBox()
    if abs(pcb.ToMM(bounds.GetWidth())-90)>.1 or abs(pcb.ToMM(bounds.GetHeight())-75)>.1:
        errors.append('Outline is not 90 x 75 mm')
    # Conservative courtyard bounding boxes. No exemptions for provisional modules.
    boxes={}
    for ref,fp in fps.items():
        shapes=[s.GetBoundingBox() for s in fp.GraphicalItems() if s.GetLayer()==pcb.F_CrtYd]
        if not shapes:
            errors.append(ref+': missing courtyard');continue
        boxes[ref]=(min(b.GetLeft() for b in shapes), min(b.GetTop() for b in shapes),
                    max(b.GetRight() for b in shapes),max(b.GetBottom() for b in shapes))
    names=sorted(boxes)
    for index,a in enumerate(names):
        ax,ay,bx,by=boxes[a]
        for b in names[index+1:]:
            cx,cy,dx,dy=boxes[b]
            if min(bx,dx)-max(ax,cx)>pcb.FromMM(.01) and min(by,dy)-max(ay,cy)>pcb.FromMM(.01):
                errors.append(a+'/'+b+': courtyard envelopes overlap')
    return errors


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('netlist',type=Path)
    parser.add_argument('--board',type=Path,default=Path(__file__).with_name('landerviz-carrier.kicad_pcb'))
    parser.add_argument('--allow-routing',action='store_true')
    args=parser.parse_args()
    errors=check(pcb.LoadBoard(str(args.board)),args.netlist,allow_routing=args.allow_routing)
    print(json.dumps({'status':'FAIL' if errors else 'PASS','errors':errors},indent=2))
    raise SystemExit(bool(errors))
