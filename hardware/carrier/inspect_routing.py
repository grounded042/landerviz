import hashlib
import json
import math
from collections import defaultdict
from pathlib import Path
import pcbnew as p

import argparse
parser=argparse.ArgumentParser(description='Inspect saved carrier copper against its fixed placement')
parser.add_argument('--output', type=Path, required=True)
args=parser.parse_args()
file=Path(__file__).with_name('landerviz-carrier.kicad_pcb')
b=p.LoadBoard(str(file))
source=p.LoadBoard(str(file.with_name('placement-source.kicad_pcb')))
fps={f.GetReference():f for f in b.GetFootprints()}
for f in source.GetFootprints():
    g=fps[f.GetReference()]
    assert f.GetPosition()==g.GetPosition()
    assert f.GetOrientationDegrees()==g.GetOrientationDegrees()
    def pads(fp):
        return sorted((q.GetNumber(),q.GetPosition().x,q.GetPosition().y,q.GetNetname()) for q in fp.Pads())
    assert pads(f)==pads(g), f.GetReference()
lengths=defaultdict(float)
layers=defaultdict(int)
widths=defaultdict(int)
vias=0
usb=['/USB_DP','/USB_DM','/HOST_GP0_DP','/HOST_GP1_DM','/KBD_DP','/KBD_DM']
covered=defaultdict(float)
total=defaultdict(float)
bottom=[z for z in b.Zones() if z.GetLayer()==p.B_Cu and z.GetNetname()=='/GND']
assert len(bottom)==1 and bottom[0].HasFilledPolysForLayer(p.B_Cu)
gaps=[]
for t in b.GetTracks():
    name=t.GetNetname()
    if isinstance(t,p.PCB_VIA):
        vias+=1
        assert name not in usb
        continue
    length=p.ToMM(t.GetLength())
    lengths[name]+=length
    layers[b.GetLayerName(t.GetLayer())]+=1
    widths[str(round(p.ToMM(t.GetWidth()),3))]+=1
    if name not in usb: continue
    assert t.GetLayer()==p.F_Cu
    a,c=t.GetStart(),t.GetEnd()
    count=max(1,math.ceil(length/.1))
    run=0
    for i in range(count):
        q=(i+.5)/count
        x,y=round(a.x+(c.x-a.x)*q),round(a.y+(c.y-a.y)*q)
        if bottom[0].HitTestFilledArea(p.B_Cu,p.VECTOR2I(x,y)):
            covered[name]+=length/count
            if run>.5: gaps.append({'net':name,'gap_mm':round(run,2),'segment_start_mm':[p.ToMM(a.x)-50,p.ToMM(a.y)-50]})
            run=0
        else: run+=length/count
        total[name]+=length/count
    if run>.5: gaps.append({'net':name,'gap_mm':round(run,2),'segment_start_mm':[p.ToMM(a.x)-50,p.ToMM(a.y)-50]})
result={'board_sha256':hashlib.sha256(file.read_bytes()).hexdigest(),
        'unchanged_footprints':len(fps),'vias':vias,'segments_by_layer':dict(layers),
        'segments_by_width_mm':dict(widths),
        'length_by_net_mm':{k:round(v,3) for k,v in sorted(lengths.items())},
        'bottom_ground_under_usb_centerline_percent':{k:round(100*covered[k]/v,1) for k,v in total.items()},
        'ground_centerline_gaps_over_0_5mm':gaps}
args.output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
