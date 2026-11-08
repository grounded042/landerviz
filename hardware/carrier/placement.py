"""Unrouted carrier placement study. Run with KiCad's bundled Python.

Standard library + installed pcbnew only. Never writes the bare-chip project.
The explicit provisional envelopes are NOT purchase-ready footprints.
"""
import argparse
import html
import json
from pathlib import Path
import uuid
import xml.etree.ElementTree as ET

import pcbnew as pcb
import mechanical
import encoder_footprint

HERE = Path(__file__).resolve().parent
LIB = HERE / 'carrier.pretty'
STANDARD = Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints')
BOARD = HERE / 'landerviz-carrier.kicad_pcb'
WIDTH, HEIGHT = 90, 75

# Official PGA2350 top view: A..J top to bottom, 1..10 left to right.
# Empty central 6x6 area. Six ground positions are individually numbered here.
PGA = {
    'A1':'GND1','A2':'GP1','A3':'VB','A4':'3V3_EN','A5':'RUN',
    'A6':'U+','A7':'U-','A8':'BS','A9':'GP46','A10':'GND2',
    'B1':'GP2','B2':'GND3','B3':'GP0','B4':'3V3','B5':'VREF',
    'B6':'SWCLK','B7':'SWDIO','B8':'GP47','B9':'GND4','B10':'GP45',
    'C1':'GP4','C2':'GP3','C9':'GP43','C10':'GP44',
    'D1':'GP6','D2':'GP5','D9':'GP41','D10':'GP42',
    'E1':'GP8','E2':'GP7','E9':'GP39','E10':'GP40',
    'F1':'GP10','F2':'GP9','F9':'GP37','F10':'GP38',
    'G1':'GP12','G2':'GP11','G9':'GP35','G10':'GP36',
    'H1':'GP14','H2':'GP13','H9':'GP33','H10':'GP34',
    'I1':'GP16','I2':'GP15','I3':'GP19','I4':'GP21','I5':'GP23',
    'I6':'GP25','I7':'GP27','I8':'GP29','I9':'GP31','I10':'GP32',
    'J1':'GND5','J2':'GP17','J3':'GP18','J4':'GP20','J5':'GP22',
    'J6':'GP24','J7':'GP26','J8':'GP28','J9':'GP30','J10':'GND6',
}

R = 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal'
D = 'Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal'
FOOTPRINTS = {
    **{f'R{i}':R for i in range(1,14)},
    **{f'D{i}':D for i in range(1,5)},
    'D5':'LED_THT:LED_D3.0mm',
    'U1':'Carrier:PGA2350_Socket_Grid',
    'U2':'Carrier:Level_Converter_PROVISIONAL',
    'J1':'Carrier:Soldered_333011', 'J2':'Carrier:Soldered_333011',
    'J3':'Connector_Audio:Jack_3.5mm_CUI_SJ1-3533NG_Horizontal_CircularHoles',
    'J4':'Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical',
    'C1':'Capacitor_THT:CP_Radial_D8.0mm_P3.50mm',
    'C2':'Capacitor_THT:CP_Radial_D6.3mm_P2.50mm',
    'C4':'Capacitor_THT:CP_Radial_D6.3mm_P2.50mm',
    'C3':'Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm',
    'C5':'Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm',
    'SW1':'Button_Switch_THT:SW_PUSH_6mm',
    'SW2':'Button_Switch_THT:SW_PUSH_6mm',
    'SW3':'Carrier:Encoder_SameSky_ACZ11BR1E_Vertical',
}

# KiCad anchors, not body centres, for ordinary library footprints.
PLACEMENT = {
    'U1':(45,22,180),'J1':(11,14,0),'J2':(79,14,180),
    'J3':(5.7,44,90),'U2':(13,64,0),'J4':(25.725,64.19,0),
    'SW1':(59.5,2,0),'SW2':(24,2,0),'SW3':(77,55,0),
    'C1':(77.25,34,0),
    'D5':(43.73,4,0),'R13':(49,4,0),
    'R1':(5,28,0),'R2':(5,32,0),
    'R3':(74,41,0),'R4':(74,45,0),
    'R5':(62.8,24,90),'R6':(66,24,90),
    'C2':(35,42,0),'C4':(53,42,0),
    'R7':(32,49,0),'R8':(32,54,0),'R9':(32,59,0),
    'R10':(50,49,0),'R11':(50,54,0),'R12':(50,59,0),
    'D1':(32,64,0),'D2':(32,69,0),
    'D3':(50,64,0),'D4':(50,69,0),
    'C3':(43,53,90),'C5':(62,53,90),
}


def vector(x,y):
    return pcb.VECTOR2I(pcb.FromMM(x),pcb.FromMM(y))


def rect(x1,y1,x2,y2,layer='F.Fab',width=.15):
    return (f'(fp_rect (start {x1} {y1}) (end {x2} {y2}) '
            f'(stroke (width {width}) (type default)) (fill none) (layer "{layer}"))')


def pad(number,x,y,drill=1,size=1.8):
    return (f'(pad "{number}" thru_hole circle (at {x} {y}) '
            f'(size {size} {size}) (drill {drill}) (layers "*.Cu" "*.Mask"))')


def write_footprint(name,body,description):
    (LIB / (name+'.kicad_mod')).write_text(f'''(footprint "{name}"
      (version 20240108) (generator "pcbnew") (layer "F.Cu")
      (descr "{description}") (attr through_hole)
      (property "Reference" "REF**" (at 0 -14 0) (layer "F.SilkS")
        (effects (font (size 1 1) (thickness .15))))
      (property "Value" "{name}" (at 0 14 0) (layer "F.Fab")
        (effects (font (size .8 .8) (thickness .12))))
      {body})\n''')


def write_converter_footprint():
    # Confirmed labelled-side orientation, HV row left and channel 4 at top.
    # Body measurements are coarse; retain the conservative courtyard.
    LIB.mkdir(exist_ok=True)
    body=[rect(-6.35,-7.15,6.35,7.15),rect(-7.5,-9,7.5,9,'F.CrtYd',.05)]
    for side,names in ((-1,['HV4','HV3','GND_H','HV','HV2','HV1']),
                       (1,['LV4','LV3','GND_L','LV','LV2','LV1'])):
        body += [pad(name,side*5.08,round((i-2.5)*2.54,3)) for i,name in enumerate(names)]
        label='HV4' if side<0 else 'LV4'
        body.append(f'(fp_text user "{label}" (at {side*5.08} -8.4) (layer "F.SilkS") '
                    '(effects (font (size .8 .8) (thickness .12))))')
    write_footprint('Level_Converter_PROVISIONAL','\n'.join(body),
                    'Owned module: HV-left/LV-right labelled side, channel 4 top; 10.16mm rows; approx 12.7x14.3mm body')


def libraries():
    LIB.mkdir(exist_ok=True)
    (HERE/'footprints.json').write_text(json.dumps(FOOTPRINTS,indent=2,sort_keys=True)+'\n')
    assert len(PGA)==64 and len(set(PGA.values()))==64
    expected={f'GP{i}' for i in range(48)} | {f'GND{i}' for i in range(1,7)}
    expected |= {'VB','3V3','3V3_EN','VREF','RUN','BS','U+','U-','SWDIO','SWCLK'}
    assert set(PGA.values())==expected
    body=[rect(-12.7,-12.7,12.7,12.7),rect(-13.5,-13.5,13.5,13.5,'F.CrtYd',.05)]
    for coord,name in PGA.items():
        row=ord(coord[0])-ord('A'); col=int(coord[1:])-1
        assert row<2 or row>7 or col<2 or col>7
        body.append(pad(name,round((col-4.5)*2.54,3),round((row-4.5)*2.54,3)))
    # Corner marker identifies manufacturer's A1, independent of carrier rotation.
    body.append(rect(-12.4,-12.4,-10.4,-10.4,'F.Fab'))
    write_footprint('PGA2350_Socket_Grid','\n'.join(body),
                    'Official 25.4mm module / 2.54mm grid; socket housing fit pending')
    body=[rect(-11,-11,11,11),rect(-11.3,-11.3,11.3,11.3,'F.CrtYd',.05),
          rect(-10.5,-4.8,-3,4.8,'F.Fab')]
    body += [pad(i,9.5,round((i-4.5)*2.54,3)) for i in range(1,9)]
    # Module's two 3.2mm mounting holes; carrier repeats these as clearance holes.
    for y in (-8,8):
        body.append(f'(pad "" np_thru_hole circle (at -8 {y}) (size 3.2 3.2) '
                    '(drill 3.2) (layers "*.Cu" "*.Mask"))')
    write_footprint('Soldered_333011','\n'.join(body),
                    'Manufacturer v1.2.0 22x22mm module; opening faces local -X')
    # User ruler measurement: 9/16 inch along pins, just under 1/2 across.
    # Round the body envelope upward; reserve extra room for ruler uncertainty
    # and socket housings rather than treating the conversion as precise CAD.
    write_converter_footprint()
    encoder_footprint.write_library()
    libs=['(lib (name "Carrier") (type "KiCad") (uri "${KIPRJMOD}/carrier.pretty") (options "") (descr "Carrier study"))']
    for name in sorted({v.split(':')[0] for v in FOOTPRINTS.values()}-{'Carrier'}):
        libs.append(f'(lib (name "{name}") (type "KiCad") '
                    f'(uri "${{KICAD10_FOOTPRINT_DIR}}/{name}.pretty") (options "") (descr ""))')
    (HERE/'fp-lib-table').write_text('(fp_lib_table\n'+'\n'.join(libs)+'\n)\n')


def make(netlist, output=BOARD):
    if output.exists():
        existing=pcb.LoadBoard(str(output))
        if len(existing.GetTracks()) or any(not z.GetIsRuleArea() for z in existing.Zones()):
            raise RuntimeError('Refusing to overwrite routing; choose --output for a new placement file')
    libraries()
    root=ET.parse(netlist).getroot()
    board=pcb.BOARD()
    board.SetCopperLayerCount(2)
    board.GetDesignSettings().m_CopperEdgeClearance=pcb.FromMM(.3)
    pin_nets={}
    for net in root.findall('./nets/net'):
        name=net.get('name')
        # XML writes pin-name slashes literally; PCB/schematic parity uses
        # KiCad's escaped spelling inside autogenerated no-connect net names.
        if name.startswith('unconnected-'): name=name.replace('/','{slash}')
        info=pcb.NETINFO_ITEM(board,name);board.Add(info)
        for node in net.findall('node'):
            pin_nets[node.get('ref'),node.get('pin')]=info
    for comp in root.findall('./components/comp'):
        ref=comp.get('ref'); lib,name=FOOTPRINTS[ref].split(':')
        fp=pcb.FootprintLoad(str(LIB if lib=='Carrier' else STANDARD/(lib+'.pretty')),name)
        if fp is None: raise ValueError('Cannot load '+name)
        fp.SetReference(ref);fp.SetValue(comp.findtext('value'))
        for field in comp.findall('./fields/field'):
            if field.get('name')=='Footprint': continue
            fp.SetField(field.get('name'),field.text or '')
            fp.GetField(field.get('name')).SetVisible(False)
        fp.SetFPIDAsString(FOOTPRINTS[ref])
        root_uuid=uuid.uuid5(uuid.UUID('8b9c8114-c1b1-4689-9e91-3c486a2dc2b1'),'root')
        path=pcb.KIID_PATH();path.push_back(pcb.KIID(str(root_uuid)))
        path.push_back(pcb.KIID(comp.findtext('tstamps')));fp.SetPath(path)
        board.Add(fp)
        for p in fp.Pads():
            net=pin_nets.get((ref,p.GetNumber()))
            if net is not None: p.SetNet(net)
        x,y,angle=PLACEMENT[ref]
        fp.SetPosition(vector(x+50,y+50));fp.SetOrientationDegrees(angle)
        fp.Value().SetVisible(False)
        fp.Reference().SetTextSize(vector(.9,.9))
        fp.Reference().SetTextThickness(pcb.FromMM(.13))
        ref_pos={'J1':(18,26),'J2':(79,26),'J3':(21,44),
                 'U2':(13,73.5),'J4':(25.725,61.5),'SW1':(63,10),'SW2':(27,10),
                 'R1':(16,28),'R2':(16,32),'R3':(70.7,41),'R4':(70.7,45)}
        if ref in ref_pos:
            rx,ry=ref_pos[ref]
            fp.Reference().SetPosition(vector(rx+50,ry+50))
            fp.Reference().SetTextAngle(pcb.EDA_ANGLE(0,pcb.DEGREES_T))
    for start,end in [((0,0),(WIDTH,0)),((WIDTH,0),(WIDTH,HEIGHT)),
                      ((WIDTH,HEIGHT),(0,HEIGHT)),((0,HEIGHT),(0,0))]:
        shape=pcb.PCB_SHAPE();shape.SetShape(pcb.SHAPE_T_SEGMENT)
        shape.SetStart(vector(start[0]+50,start[1]+50));shape.SetEnd(vector(end[0]+50,end[1]+50))
        shape.SetLayer(pcb.Edge_Cuts);shape.SetWidth(pcb.FromMM(.05));board.Add(shape)
    for value,x,y,size in [('LCD',25.725,59,.9),
                          ('1:GND 2:5V 3:SDA 4:SCL',30,74,.8),
                          ('HUB IN',11,1.5,1),('KEYBOARD',79,1.5,1),
                          ('AUX',11,38,1),('BOOT',63,12,1),('RESET',27,12,1),
                          ('STATUS',45,7,.8),
                          ('LANDERVIZ',76,70,1.5),('CARRIER / DRAFT',76,72.5,.9)]:
        text=pcb.PCB_TEXT(board);text.SetText(value);text.SetPosition(vector(x+50,y+50))
        text.SetTextSize(vector(size,size));text.SetTextThickness(pcb.FromMM(.14))
        text.SetLayer(pcb.F_SilkS);board.Add(text)
    text=pcb.PCB_TEXT(board);text.SetText('UNROUTED PLACEMENT STUDY - NOT FOR FABRICATION\nDimensional basis accepted; full module mating is an assembly step')
    text.SetPosition(vector(95,130));text.SetTextSize(vector(1,1));text.SetLayer(pcb.Dwgs_User);board.Add(text)
    mechanical.apply(board)
    pcb.SaveBoard(str(output),board)
    overview(board)
    print(f'Wrote {output}: {WIDTH} x {HEIGHT} mm, 32 electrical positions + 4 mounts, zero tracks')


def overview(board, copper=False, output=None):
    """Readable top view from saved coordinates; optional actual copper paths."""
    stage='routing draft' if copper else 'placement study'
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 -8 98 93" width="980" height="930">',
         '<rect x="-4" y="-8" width="98" height="93" fill="#f3f5f8"/>',
         '<style>text{font-family:system-ui,sans-serif;text-anchor:middle;fill:#e6edf3} .caption{fill:#263447;font-size:1.5px}</style>',
         f'<text class="caption" x="45" y="-4">LANDERVIZ — 90 × 75 mm carrier {stage}</text>',
         '<rect width="90" height="75" rx=".5" fill="#172a3a" stroke="#405d72" stroke-width=".2"/>']
    if copper:
        # Ground fills are deliberately omitted so routes remain readable.
        for layer,color in ((pcb.B_Cu,'#5b9fe7'),(pcb.F_Cu,'#f3a17e')):
            for track in board.GetTracks():
                if isinstance(track,pcb.PCB_VIA) or track.GetLayer()!=layer: continue
                a,b=track.GetStart(),track.GetEnd()
                svg.append(f'<path d="M{pcb.ToMM(a.x)-50} {pcb.ToMM(a.y)-50}L{pcb.ToMM(b.x)-50} {pcb.ToMM(b.y)-50}" '
                           f'stroke="{color}" stroke-width="{pcb.ToMM(track.GetWidth())}" fill="none"/>')
        for track in board.GetTracks():
            if isinstance(track,pcb.PCB_VIA):
                pos=track.GetPosition()
                svg.append(f'<circle cx="{pcb.ToMM(pos.x)-50}" cy="{pcb.ToMM(pos.y)-50}" '
                           'r=".4" fill="#d2ad72"/><circle '
                           f'cx="{pcb.ToMM(pos.x)-50}" cy="{pcb.ToMM(pos.y)-50}" r=".2" fill="#111c28"/>')
    fps={fp.GetReference():fp for fp in board.GetFootprints()}
    modules={'U1':(-12.7,-12.7,25.4,25.4,'#30616d'),
             'J1':(-11,-11,22,22,'#254a66'),'J2':(-11,-11,22,22,'#254a66'),
             'J3':(-4,-1.2,8.2,14,'#465c69'),'U2':(-6.35,-7.15,12.7,14.3,'#765824')}
    for ref,fp in fps.items():
        if ref in mechanical.MOUNTS:
            pos=fp.GetPosition();x=pcb.ToMM(pos.x)-50;y=pcb.ToMM(pos.y)-50
            svg.append(f'<circle cx="{x}" cy="{y}" r="3.5" fill="none" stroke="#9bb5c8" stroke-width=".15" stroke-dasharray=".5 .4"/>')
            continue
        x,y,angle=PLACEMENT[ref]
        svg.append(f'<g transform="translate({x} {y}) rotate({-angle})">')
        if ref in modules:
            a,b,w,h,color=modules[ref]
            svg.append(f'<rect x="{a}" y="{b}" width="{w}" height="{h}" fill="{color}" stroke="#8eacba" stroke-width=".2"/>')
            if ref in ('J1','J2'):
                svg.append('<rect x="-11" y="-4.8" width="8" height="9.6" fill="#a6afb6" stroke="#e5e7eb" stroke-width=".15"/>')
            elif ref=='J3':
                svg.append('<rect x="-2.1" y="-5.2" width="6" height="4" fill="#a6afb6" stroke="#e5e7eb" stroke-width=".15"/>')
        elif ref=='D5':
            svg.append('<circle cx="1.27" cy="0" r="1.5" fill="#62d777" stroke="#a7e8b2" stroke-width=".2"/>')
        elif ref.startswith('R') or ref.startswith('D'):
            color='#c2a375' if ref.startswith('R') else '#513b35'
            svg.append(f'<path d="M0 0H7.62" stroke="#bbb" stroke-width=".35"/><rect x=".66" y="-1.25" width="6.3" height="2.5" rx=".5" fill="{color}"/>')
            if ref.startswith('D'):
                svg.append('<path d="M1.25 -1.25V1.25" stroke="#e6dfd9" stroke-width=".5"/>')
        elif ref in ('C1','C2','C4'):
            radius=4 if ref=='C1' else 3.15
            centre=1.75 if ref=='C1' else 1.25
            svg.append(f'<circle cx="{centre}" cy="0" r="{radius}" fill="#4b5a64" stroke="#b4c3c9" stroke-width=".2"/>')
        elif ref in ('C3','C5'):
            svg.append('<rect x="0" y="-1.25" width="5" height="2.5" rx="1" fill="#b96c38"/>')
        elif ref in ('SW1','SW2'):
            svg.append('<rect x=".25" y="-.75" width="6" height="6" fill="#a4a8ae"/><circle cx="3.25" cy="2.25" r="1.4" fill="#393e46"/>')
        elif ref=='SW3':
            svg.append('<rect x="-5.85" y="-7.25" width="11.7" height="13.75" fill="#a0a4a7" stroke="#ccd3db" stroke-width=".2"/><circle r="3.5" fill="#373d43"/><circle r="3" fill="#bbc3ca"/>')
        elif ref=='J4':
            svg.append('<rect x="-1.27" y="-1.27" width="2.54" height="10.16" fill="#333e4a" stroke="#8eacba" stroke-width=".2"/>')
        svg.append('</g>')
    # All hole locations come from the actual board, including module fixing holes.
    for fp in fps.values():
        for p in fp.Pads():
            pos=p.GetPosition();x=pcb.ToMM(pos.x)-50;y=pcb.ToMM(pos.y)-50
            if p.GetAttribute()==pcb.PAD_ATTRIB_NPTH:
                svg.append(f'<circle cx="{x}" cy="{y}" r="1.6" fill="#f3f5f8"/>')
            else:
                svg.append(f'<circle cx="{x}" cy="{y}" r=".8" fill="#d2ad72"/><circle cx="{x}" cy="{y}" r=".43" fill="#111c28"/>')
    labels=[('PGA2350',45,21,2),('64-pin socket grid',45,24,1.1),
            ('USB-C INPUT',11,9,1.3),('USB-C KEYBOARD',79,9,1.1),
            ('AUX / TRS',11,46,1.3),('SJ1-3533NG',11,50,.9),
            ('LCD LEVEL',13,61,1.2),('CONVERTER',13,64,1.1),('APPROX. FIT',13,67,1),
            ('LCD',28.6,62.3,.9),('J4',25.725,61.5,.9),
            ('LCD CABLE ↓',29,74,1),('AUDIO L',38,38,1),('AUDIO R',56,38,1),
            ('ENCODER',77,65.5,1.2),('LANDERVIZ',69,68.5,1.8),
            ('CARRIER REV A',69,71,.9),('2026-11',69,73,.8),
            ('RESET',27.25,-.8,.8),('BOOT',62.75,-.8,.8)]
    labels.append(('STATUS',45,7,.8))
    for ref,fp in fps.items():
        if ref in modules or ref in ('SW1','SW2','SW3','J4'): continue
        pos=fp.Reference().GetPosition()
        labels.append((ref,pcb.ToMM(pos.x)-50,pcb.ToMM(pos.y)-50+.3,.95))
    for value,x,y,size in labels:
        svg.append(f'<text x="{x}" y="{y}" font-size="{size}">{html.escape(value)}</text>')
    caption=('ROUTING DRAFT — top: peach, underside: blue; ground fills omitted' if copper else
             'UNROUTED — amber modules and kit capacitor sizes are provisional')
    svg.extend([f'<text class="caption" x="45" y="79">{caption}</text>',
                '<text class="caption" x="45" y="82">Top-view envelopes, not a 3D model or manufacturing drawing</text>','</svg>'])
    target=output or HERE/('routing-overview.svg' if copper else 'placement-overview.svg')
    target.write_text('\n'.join(svg)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('netlist',type=Path)
    parser.add_argument('--output',type=Path,default=BOARD)
    args=parser.parse_args()
    make(args.netlist,args.output)
