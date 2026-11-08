"""Reproduce a two-layer carrier routing draft from the saved placement.

Uses installed KiCad plus Python's standard library. No remote autorouter.
All footprints stay fixed. Independent KiCad DRC must pass after zone filling.
"""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import shutil

import pcbnew as pcb
from route_grid import find_path
from placement import overview
from mechanical import MOUNTS, KEEP_OUT_RADIUS

HERE=Path(__file__).resolve().parent
SOURCE=HERE/'placement-source.kicad_pcb'
OUTPUT=HERE/'landerviz-carrier.kicad_pcb'
STEP=.25
NX,NY=361,301
CLEARANCE=.2
VIA_SIZE,VIA_DRILL=.8,.4


def mm(v): return pcb.ToMM(v)
def pt(x,y): return pcb.VECTOR2I(pcb.FromMM(x+50),pcb.FromMM(y+50))
def xy(p): return mm(p.x)-50,mm(p.y)-50
def cell(p): return round(p[0]/STEP),round(p[1]/STEP)


class Router:
    def __init__(self,board):
        self.board=board
        self.pads={};self.by_net=defaultdict(list);self.geometry=[];self.log=[]
        # Reserve actual screw head/spacer space on both layers, rather than
        # routing up to the smaller bare drill. Matches the board rule areas.
        for fp in board.GetFootprints():
            if fp.GetReference() in MOUNTS:
                p=xy(fp.GetPosition())
                self.geometry.append(('line',-998,-1,(p,p,KEEP_OUT_RADIUS*2)))
        for f in board.GetFootprints():
            for p in f.Pads():
                pos=xy(p.GetPosition()); size=p.GetSize()
                # Conservative rectangle includes copper and mechanical holes.
                box=p.GetBoundingBox()
                bounds=(mm(box.GetLeft())-50,mm(box.GetTop())-50,
                        mm(box.GetRight())-50,mm(box.GetBottom())-50)
                if p.GetAttribute()==pcb.PAD_ATTRIB_NPTH:
                    bounds=(pos[0]-mm(size.x)/2,pos[1]-mm(size.y)/2,
                            pos[0]+mm(size.x)/2,pos[1]+mm(size.y)/2)
                if p.GetShape()==pcb.PAD_SHAPE_CIRCLE:
                    self.geometry.append(('line',p.GetNetCode(),-1,(pos,pos,mm(size.x))))
                else:
                    self.geometry.append(('box',p.GetNetCode(),-1,bounds))
                if p.GetNumber(): self.pads[f.GetReference(),p.GetNumber()]=p
                if p.GetNetname() and not p.GetNetname().startswith('unconnected-'):
                    self.by_net[p.GetNetname()].append(p)

    def masks(self,net,width):
        blocked=[bytearray(NX*NY),bytearray(NX*NY)]
        via=bytearray(NX*NY)
        for kind,n,z,data in self.geometry:
            if n==net: continue
            for masks,radius,layers in ((blocked,width/2+CLEARANCE+.04,range(2) if z<0 else (z,)),
                                        ([via],VIA_SIZE/2+CLEARANCE+.04,(0,))):
                if kind=='box':
                    x0,y0,x1,y1=data
                    xa=max(0,math.floor((x0-radius)/STEP));xb=min(NX-1,math.ceil((x1+radius)/STEP))
                    ya=max(0,math.floor((y0-radius)/STEP));yb=min(NY-1,math.ceil((y1+radius)/STEP))
                    for yy in range(ya,yb+1):
                        for zz in layers: masks[zz][yy*NX+xa:yy*NX+xb+1]=b'\1'*(xb-xa+1)
                else:
                    a,b,own_width=data; r=radius+own_width/2
                    xa=max(0,math.floor((min(a[0],b[0])-r)/STEP));xb=min(NX-1,math.ceil((max(a[0],b[0])+r)/STEP))
                    ya=max(0,math.floor((min(a[1],b[1])-r)/STEP));yb=min(NY-1,math.ceil((max(a[1],b[1])+r)/STEP))
                    dx,dy=b[0]-a[0],b[1]-a[1]; length=dx*dx+dy*dy
                    for yy in range(ya,yb+1):
                        for xx in range(xa,xb+1):
                            # The PGA's two-row pin field must retain routing
                            # escapes. Ground-corridor reservation applies to
                            # the open carrier area, not through-hole fanout.
                            if n==-999 and 31.5<=xx*STEP<=58.5 and 8.5<=yy*STEP<=35.5:
                                continue
                            t=max(0,min(1,((xx*STEP-a[0])*dx+(yy*STEP-a[1])*dy)/length)) if length else 0
                            if (xx*STEP-a[0]-t*dx)**2+(yy*STEP-a[1]-t*dy)**2<=r*r:
                                for zz in layers: masks[zz][yy*NX+xx]=1
        edge=.4+width/2
        for yy in range(NY):
            for xx in range(NX):
                if min(xx*STEP,yy*STEP,90-xx*STEP,75-yy*STEP)<edge:
                    for z in range(2): blocked[z][yy*NX+xx]=1
                    via[yy*NX+xx]=1
        return blocked,via

    def track(self,net,width,a,b,layer=0):
        if math.dist(a,b)<.001: return
        t=pcb.PCB_TRACK(self.board);t.SetStart(pt(*a));t.SetEnd(pt(*b))
        t.SetWidth(pcb.FromMM(width));t.SetLayer(pcb.F_Cu if layer==0 else pcb.B_Cu)
        t.SetNetCode(net);self.board.Add(t)
        self.geometry.append(('line',net,layer,(a,b,width)))

    def via(self,net,p):
        v=pcb.PCB_VIA(self.board);v.SetPosition(pt(*p))
        v.SetWidth(pcb.FromMM(VIA_SIZE));v.SetDrill(pcb.FromMM(VIA_DRILL))
        v.SetViaType(pcb.VIATYPE_THROUGH);v.SetLayerPair(pcb.F_Cu,pcb.B_Cu)
        v.SetNetCode(net);self.board.Add(v)
        self.geometry.append(('box',net,-1,(p[0]-.4,p[1]-.4,p[0]+.4,p[1]+.4)))

    def manual(self,a,b,points,width=.25):
        pa,pb=self.pads[a],self.pads[b]
        assert pa.GetNetCode()==pb.GetNetCode()
        assert math.dist(xy(pa.GetPosition()),points[0])<.001
        assert math.dist(xy(pb.GetPosition()),points[-1])<.001
        for x,y in zip(points,points[1:]): self.track(pa.GetNetCode(),width,x,y)
        self.log.append({'net':pa.GetNetname(),'from':a,'to':b,'kind':'explicit USB',
                         'width_mm':width,
                         'length_mm':round(sum(math.dist(x,y) for x,y in zip(points,points[1:])),3)})

    def connect(self,a,b,width=.25):
        net=a.GetNetCode();assert net==b.GetNetCode()
        p,q=xy(a.GetPosition()),xy(b.GetPosition())
        blocked,vias=self.masks(net,width)
        start,end=cell(p),cell(q)
        path=find_path(blocked,vias,NX,NY,start,end,via_cost=24,bottom_cost=1.5)
        if path is None: raise RuntimeError(f'No route: {a.GetNetname()} {p} -> {q}')
        current=p;layer=path[0][2]; previous_direction=None
        # Compress collinear cells, preserving layer transitions as plated vias.
        for i in range(1,len(path)):
            prev,node=path[i-1],path[i]
            if node[2]!=prev[2]:
                point=(prev[0]*STEP,prev[1]*STEP)
                self.track(net,width,current,point,layer);self.via(net,point)
                current=point;layer=node[2];previous_direction=None
            else:
                direction=(node[0]-prev[0],node[1]-prev[1])
                if previous_direction is not None and direction!=previous_direction:
                    point=(prev[0]*STEP,prev[1]*STEP)
                    self.track(net,width,current,point,layer);current=point
                previous_direction=direction
        point=(path[-1][0]*STEP,path[-1][1]*STEP)
        self.track(net,width,current,point,layer);self.track(net,width,point,q,layer)
        self.log.append({'net':a.GetNetname(),'from':xy(a.GetPosition()),'to':xy(b.GetPosition()),
                         'width_mm':width,'vias':sum(x[2]!=y[2] for x,y in zip(path,path[1:]))})
        print('routed',a.GetNetname(),p,q,flush=True)

    def ground(self):
        code=next(p.GetNetCode() for p in self.by_net['/GND'])
        for layer in (pcb.F_Cu,pcb.B_Cu):
            z=pcb.ZONE(self.board);z.SetLayer(layer);z.SetNetCode(code)
            z.SetLocalClearance(pcb.FromMM(.2));z.SetMinThickness(pcb.FromMM(.2))
            z.SetPadConnection(pcb.ZONE_CONNECTION_THERMAL)
            z.SetThermalReliefGap(pcb.FromMM(.25));z.SetThermalReliefSpokeWidth(pcb.FromMM(.4))
            outline=z.Outline();outline.NewOutline()
            for x,y in ((.5,.5),(89.5,.5),(89.5,74.5),(.5,74.5)):
                v=pt(x,y);outline.Append(v.x,v.y)
            self.board.Add(z)


def route(source,output):
    board=pcb.LoadBoard(str(source))
    if len(board.GetTracks()) or any(not z.GetIsRuleArea() for z in board.Zones()):
        raise RuntimeError('Source must be the unrouted placement')
    settings=board.GetDesignSettings()
    settings.m_MinClearance=pcb.FromMM(.2)
    settings.m_CopperEdgeClearance=pcb.FromMM(.4)
    settings.m_ViasMinAnnularWidth=pcb.FromMM(.15)
    router=Router(board)
    # USB pairs have explicit F.Cu paths. Pin-field/connector escapes fan apart;
    # these are prototype routes, not a claimed controlled-impedance structure.
    usb=[
        (('J1','5'),('U1','U+'),[(20.5,15.27),(22,15.27),(22,17.5),(27,22.5),(27,36.5),(29,38.5),(42.23,38.5),(43.73,37),(43.73,33.43)]),
        (('J1','4'),('U1','U-'),[(20.5,12.73),(23,12.73),(23,16.5),(28,21.5),(28,35.5),(30,37.5),(39.69,37.5),(41.19,36),(41.19,33.43)]),
        (('U1','GP0'),('R5','1'),[(51.35,30.89),(51.35,29.62),(58,29.62),(62.8,24.82),(62.8,24)]),
        (('U1','GP1'),('R6','1'),[(53.89,33.43),(55.16,34.7),(58.3,34.7),(66,27),(66,24)]),
        (('R5','2'),('J2','5'),[(62.8,16.38),(64.05,16.38),(65.73,14.7),(67.53,14.7),(69.5,12.73)]),
        (('R6','2'),('J2','4'),[(66,16.38),(67.11,15.27),(69.5,15.27)]),
    ]
    for a,b,points in usb:
        # The open native pair has 1 mm centre spacing along its long runs:
        # 0.8 mm copper leaves 0.2 mm edge spacing. Host pin-field escapes
        # retain 0.25 mm copper. This is not an impedance certification.
        router.manual(a,b,points,width=.8 if a[0]=='J1' else .25)
    # Reserve the opposite side of USB paths for ground copper. These
    # are routing obstacles only: the manufactured board has no ground keepout.
    # Through-hole pad clearances still interrupt this corridor at pin escapes.
    for _,_,points in usb:
        for a,b in zip(points,points[1:]):
            router.geometry.append(('line',-999,1,(a,b,.8)))
    # Main keyboard supply connects the ports through the bulk-capacitor branch;
    # it does not travel through the processor module's VB header.
    for a,b in [(('J1','8'),('C1','1')),(('C1','1'),('J2','8'))]:
        router.connect(router.pads[a],router.pads[b],1.5)
    routed={router.pads[a].GetNetname() for a,_,_ in usb}
    priority=['/+5V','/+3V3','/LINE_L','/LINE_R','/ADC_L','/ADC_R','/BIAS_L','/BIAS_R',
              '/STATUS_LED_GP6','/ENC_PUSH']
    for name in priority+sorted(n for n in router.by_net if n not in priority+['/GND'] and n not in routed):
        pads=router.by_net[name]
        if len(pads)<2: continue
        done=[pads[0]];remaining=list(pads[1:])
        if name=='/+5V':
            done=[router.pads[x] for x in [('J1','8'),('C1','1'),('J2','8')]]
            remaining=[p for p in pads if p not in done]
        while remaining:
            a,b=min(((a,b) for a in done for b in remaining),
                    key=lambda pair:math.dist(xy(pair[0].GetPosition()),xy(pair[1].GetPosition())))
            width=.5 if name in ('/+5V','/+3V3') else .25
            if name=='/+3V3' and router.pads['U1','3V3'] in (a,b):
                width=.25  # Low-current bias/display branch escapes the inner pin row.
            router.connect(a,b,width);done.append(b);remaining.remove(b)
    router.ground()
    for drawing in board.GetDrawings():
        if isinstance(drawing,pcb.PCB_TEXT) and drawing.GetText().startswith('UNROUTED PLACEMENT STUDY'):
            drawing.SetText('ROUTED CARRIER REVIEW DRAFT - NOT FOR FABRICATION\nIndependent follow-up review pending')
    pcb.SaveBoard(str(output),board)
    main_output=output.resolve()==OUTPUT.resolve()
    preview=HERE/'routing-overview.svg' if main_output else output.with_suffix('.svg')
    report=HERE/'routing-report.json' if main_output else output.with_suffix('.routing.json')
    overview(board,copper=True,output=preview)
    report.write_text(json.dumps({'stage':'routing draft; ground fill/DRC required',
        'grid_mm':STEP,'clearance_mm':CLEARANCE,'routes':router.log},indent=2)+'\n')
    print('Saved',output,flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=SOURCE)
    parser.add_argument('--output',type=Path,default=OUTPUT)
    args=parser.parse_args()
    if not args.source.exists():
        b=pcb.LoadBoard(str(OUTPUT))
        if len(b.GetTracks()) or len(b.Zones()): raise RuntimeError('Cannot snapshot an already routed board')
        shutil.copyfile(OUTPUT,args.source)
    route(args.source,args.output)
