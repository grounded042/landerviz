"""Reproduce the carrier's review schematic; standard library, no board edits.

Uses installed KiCad symbols. Module pin names are carrier pad identifiers.
The carrier is routed; independent follow-up review remains before ordering.
"""
import math
import json
from pathlib import Path
import re
import uuid

HERE = Path(__file__).resolve().parent
SYMBOLS = Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols')
PROJECT = 'landerviz-carrier'
NAMESPACE = uuid.UUID('8b9c8114-c1b1-4689-9e91-3c486a2dc2b1')
FOOTPRINTS = json.loads((HERE / 'footprints.json').read_text())


def uid(name):
    return str(uuid.uuid5(NAMESPACE, name))


def quoted(value):
    return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def block(text, start):
    depth, string, escaped = 0, False, False
    for index in range(start, len(text)):
        char = text[index]
        if string:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == '"':
                string = False
        elif char == '"':
            string = True
        elif char == '(':
            depth += 1
        elif char == ')':
            depth -= 1
            if not depth:
                return text[start:index + 1]
    raise ValueError('Unbalanced KiCad source')


class Capture:
    def __init__(self):
        self.libraries, self.objects, self.pins, self.points = {}, [], {}, {}
        self.root = uid('root')

    def standard(self, name):
        library, symbol = name.split(':')
        text = (SYMBOLS / (library + '.kicad_sym')).read_text()
        source = block(text, text.index('(symbol ' + quoted(symbol) + '\n'))
        self.libraries[name] = source.replace('(symbol ' + quoted(symbol),
                                              '(symbol ' + quoted(name), 1)
        self.pins[name] = {pin: (float(x), float(y), int(angle)) for x, y, angle, pin in re.findall(
            r'\(pin \S+ \S+\s+\(at ([\d.-]+) ([\d.-]+) (\d+)\).*?\(number "([^"]+)"',
            source, re.S)}

    def module(self, name, left, right, halfwidth=15.24):
        """Pin definitions are (identifier, visible name, electrical type)."""
        short = name.split(':')[-1]
        count = max(len(left), len(right))
        height = max(10.16, (count + 2) * 1.27)
        geometry, pins = [], {}
        for side, entries in ((-1, left), (1, right)):
            for index, (number, label, kind) in enumerate(entries):
                x, y, angle = side * (halfwidth + 5.08), (len(entries)-1)*1.27-index*2.54, 0 if side == -1 else 180
                pins[number] = (x, y, angle)
                geometry.append(f'''(pin {kind} line (at {x:.4f} {y:.4f} {angle}) (length 5.08)
                  (name {quoted(label)} (effects (font (size 0.9 0.9))))
                  (number {quoted(number)} (effects (font (size 0.75 0.75)))))''')
        self.pins[name] = pins
        self.libraries[name] = f'''(symbol {quoted(name)}
          (pin_names (offset 0.762)) (in_bom yes) (on_board yes)
          (property "Reference" "U" (at 0 {height+3.81} 0) (effects (font (size 1.27 1.27))))
          (property "Value" {quoted(short)} (at 0 {height+1.27} 0) (effects (font (size 1.27 1.27))))
          (symbol {quoted(short+'_0_1')}
            (rectangle (start {-halfwidth} {height}) (end {halfwidth} {-height})
              (stroke (width 0.254) (type default)) (fill (type background))))
          (symbol {quoted(short+'_1_1')} {''.join(geometry)}))'''
        return height

    def instance(self, name, ref, value, x, y, angle=0, properties=None, ref_y=None):
        symbol_id = uid(ref)
        pos = self.pins[name]
        theta = math.radians(angle)
        self.points[ref] = {number: (round(x+px*math.cos(theta)-py*math.sin(theta), 4),
                                   round(y-px*math.sin(theta)-py*math.cos(theta), 4))
                            for number, (px, py, _) in pos.items()}
        props = {'Reference': ref, 'Value': value, 'Footprint': FOOTPRINTS.get(ref, ''),
                 'Assembly': 'Hand solder; physical mounting pending review'}
        props.update(properties or {})
        property_text = []
        for index, (key, val) in enumerate(props.items()):
            visible = key in ('Reference', 'Value')
            py = (ref_y if ref_y is not None else y-6.35) + index*2.54 if visible else y
            property_text.append(f'(property {quoted(key)} {quoted(val)} (at {x} {py} 0) '
                                 f'{"" if visible else "(hide yes)"} (effects (font (size 1 1))))')
        self.objects.append(f'''(symbol (lib_id {quoted(name)}) (at {x} {y} {angle}) (unit 1)
          (exclude_from_sim no) (in_bom {"no" if ref.startswith('#') else "yes"})
          (on_board {"no" if ref.startswith('#') else "yes"}) (dnp no) (uuid {symbol_id})
          {''.join(property_text)}
          {''.join(f'(pin {quoted(p)} (uuid {uid(ref+"/"+p)}))' for p in pos)}
          (instances (project "{PROJECT}" (path "/{self.root}" (reference {quoted(ref)}) (unit 1)))))''')

    def wire(self, start, end):
        self.objects.append(f'(wire (pts (xy {start[0]} {start[1]}) (xy {end[0]} {end[1]})) '
                            f'(stroke (width 0) (type default)) (uuid {uid("wire"+str(start)+str(end))}))')

    def label(self, net, point, left=False):
        self.objects.append(f'(label {quoted(net)} (at {point[0]} {point[1]} 0) '
                            f'(effects (font (size 0.9 0.9)) (justify {"right" if left else "left"} bottom)) '
                            f'(uuid {uid("label"+net+str(point))}))')

    def junction(self, point):
        self.objects.append(f'(junction (at {point[0]} {point[1]}) (diameter 0) '
                            f'(color 0 0 0 0) (uuid {uid("junction"+str(point))}))')

    def connect(self, ref, pin, net, dx=5.08, dy=0):
        point = self.points[ref][pin]
        if net is None:
            self.objects.append(f'(no_connect (at {point[0]} {point[1]}) (uuid {uid("nc"+ref+pin)}))')
        else:
            end = (round(point[0]+dx, 4), round(point[1]+dy, 4))
            self.wire(point, end)
            self.label(net, end, dx < 0)

    def text(self, value, x, y, size=1.27):
        self.objects.append(f'(text {quoted(value)} (at {x} {y} 0) '
                            f'(effects (font (size {size} {size})) (justify left top)) '
                            f'(uuid {uid("text"+value)}))')

    def save(self):
        cache = '\n'.join(self.libraries.values())
        (HERE / (PROJECT+'.kicad_sch')).write_text(f'''(kicad_sch (version 20260306)
          (generator "eeschema") (generator_version "10.0") (uuid {self.root}) (paper "A3")
          (title_block (title "LanderViz PGA2350 carrier — electrical draft")
            (date "2026-11-07") (rev "DRAFT") (company "Jon Carl")
            (comment 1 "Routed carrier review draft; independent follow-up pending"))
          (lib_symbols {cache}) {' '.join(self.objects)}
          (sheet_instances (path "/" (page "1"))) (embedded_fonts no))\n''')
        own = '\n'.join(v.replace('(symbol "Carrier:', '(symbol "', 1)
                        for k, v in self.libraries.items() if k.startswith('Carrier:'))
        (HERE / 'carrier.kicad_sym').write_text(
            '(kicad_symbol_lib (version 20241209) (generator "kicad_symbol_editor")\n'+own+'\n)\n')


def main():
    c = Capture()
    for name in ('Device:R_Small', 'Device:C_Small', 'Device:C_Polarized',
                 'Device:D_Schottky', 'Device:LED', 'Connector_Audio:AudioJack3',
                 'Switch:SW_Push', 'power:PWR_FLAG'):
        c.standard(name)
    passive = lambda names: [(name, name, 'passive') for name in names]
    left = [(f'GP{i}', f'GP{i}', 'bidirectional') for i in range(16)]
    left += [(p, p, kind) for p, kind in (
        ('VB','power_in'), ('3V3','power_out'), ('3V3_EN','input'), ('VREF','passive'),
        ('RUN','input'), ('BS','passive'), ('U+','bidirectional'), ('U-','bidirectional'),
        ('SWDIO','bidirectional'), ('SWCLK','input'))]
    left += [(f'GND{i}', 'GND', 'power_in') for i in range(1,7)]
    right = [(f'GP{i}', f'GP{i}'+('/ADC'+str(i-40) if i>=40 else ''), 'bidirectional') for i in range(16,48)]
    height = c.module('Carrier:PGA2350', left, right, 20.32)
    c.instance('Carrier:PGA2350','U1','PGA2350 / RP2350B A4',104.14,95.25,
               properties={'Manufacturer':'Pimoroni','MPN':'PIM722',
                           'Datasheet':'https://shop.pimoroni.com/products/pga2350'}, ref_y=95.25-height-7.62)
    nets = {'GP0':'HOST_GP0_DP','GP1':'HOST_GP1_DM','GP2':'SDA_3V3','GP3':'SCL_3V3',
            'GP4':'ENC_A','GP5':'ENC_B','GP6':'STATUS_LED_GP6','GP8':'ENC_PUSH','GP40':'ADC_L','GP41':'ADC_R',
            'VB':'+5V','3V3':'+3V3','RUN':'RUN','BS':'BOOTSEL','U+':'USB_DP','U-':'USB_DM'}
    for pin in c.points['U1']:
        c.connect('U1',pin,'GND' if pin.startswith('GND') else nets.get(pin),
                  -5.08 if pin in dict((a,b) for a,b,_ in left) else 5.08)
    usb = [('1','GND','passive'),('2','CC2','passive'),('3','SBU2','passive'),
           ('4','D-','passive'),('5','D+','passive'),('6','SBU1','passive'),
           ('7','CC1','passive'),('8','VBUS','passive')]
    c.module('Carrier:USB_C_Breakout', [], usb, 10.16)
    for ref,y,dp,dm in [('J1',50.8,'USB_DP','USB_DM'),('J2',116.84,'KBD_DP','KBD_DM')]:
        c.instance('Carrier:USB_C_Breakout',ref,'Soldered 333011',25.4,y,
                   properties={'Manufacturer':'Soldered','MPN':'333011',
                               'Mounting':'Module header; verify delivered revision'}, ref_y=y-20.32)
        mapping={'1':'GND','2':ref+'_CC2','3':None,'4':dm,'5':dp,'6':None,'7':ref+'_CC1','8':'+5V'}
        for pin,net in mapping.items(): c.connect(ref,pin,net)
    for number, net, rail, x,y,value in (
        (1,'J1_CC1','GND',50.8,76.2,'5.1k 1%'),(2,'J1_CC2','GND',63.5,76.2,'5.1k 1%'),
        (3,'J2_CC1','+5V',50.8,142.24,'22k 1%'),(4,'J2_CC2','+5V',63.5,142.24,'22k 1%')):
        ref='R'+str(number)
        c.instance('Device:R_Small',ref,value,x,y)
        c.connect(ref,'1',net,0,-5.08); c.connect(ref,'2',rail,0,5.08)
    for ref,pin_net,port_net,y in [('R5','HOST_GP0_DP','KBD_DP',175.26),('R6','HOST_GP1_DM','KBD_DM',190.5)]:
        c.instance('Device:R_Small',ref,'22R',50.8,y,270)
        c.connect(ref,'1',pin_net,5.08);c.connect(ref,'2',port_net,-5.08)
    c.instance('Device:C_Polarized','C1','470uF / >=10V',104.14,175.26)
    c.connect('C1','1','+5V',0,-5.08);c.connect('C1','2','GND',0,5.08)
    # Direct through-hole jack. S/T/R map to manufacturer pins 1/2/3.
    c.instance('Connector_Audio:AudioJack3','J3','SJ1-3533NG',195.58,30.48,
               properties={'Manufacturer':'Same Sky','MPN':'SJ1-3533NG',
                           'Datasheet':'https://www.sameskydevices.com/product/resource/sj1-353xng.pdf'},ref_y=15.24)
    for pin,net in {'S':'GND','R':'LINE_R','T':'LINE_L'}.items():c.connect('J3',pin,net)
    for channel,y,refs in [('L',71.12,('C2','C3','R7','R8','R9','D1','D2')),
                           ('R',144.78,('C4','C5','R10','R11','R12','D3','D4'))]:
        cap, shunt, upper, lower, series, high, low = refs
        bias, adc = 'BIAS_'+channel, 'ADC_'+channel
        c.instance('Device:C_Polarized',cap,'10uF / >=16V',238.76,y,270)
        c.connect(cap,'2','LINE_'+channel,-5.08)
        c.instance('Device:R_Small',upper,'22k 1%',266.7,y-15.24)
        c.connect(upper,'1','+3V3',0,-5.08)
        c.instance('Device:R_Small',lower,'22k 1%',266.7,y+15.24)
        c.connect(lower,'2','GND',0,5.08)
        c.instance('Device:R_Small',series,'10k 1%',297.18,y,90)
        c.instance('Device:C_Small',shunt,'2.2nF C0G 5%',325.12,y+15.24)
        c.connect(shunt,'2','GND',0,5.08)
        for ref,cy,k,a in ((high,y-15.24,'+3V3',adc),(low,y+15.24,adc,'GND')):
            c.instance('Device:D_Schottky',ref,'BAT85S',358.14,cy,270,
                       properties={'Manufacturer':'Vishay','MPN':'BAT85S-TR',
                                   'Datasheet':'https://www.vishay.com/docs/85513/bat85s.pdf'})
            if ref == high: c.connect(ref,'1',k,0,-5.08)
            else: c.connect(ref,'2',a,0,5.08)
        bias_point, adc_point, diode_point = (266.7,y), (325.12,y), (358.14,y)
        for ref,pin in ((cap,'1'),(upper,'2'),(lower,'1'),(series,'1')):
            c.wire(c.points[ref][pin],bias_point)
        for ref,pin in ((series,'2'),(shunt,'1')):
            c.wire(c.points[ref][pin],adc_point)
        c.wire(adc_point,diode_point)
        for ref,pin in ((high,'2'),(low,'1')):
            c.wire(c.points[ref][pin],diode_point)
        c.label(bias,bias_point);c.label(adc,adc_point)
        for point in (bias_point,adc_point,diode_point): c.junction(point)
    for ref,value,net,x in [('SW1','BOOT','BOOTSEL',35.56),('SW2','RESET','RUN',83.82)]:
        c.instance('Switch:SW_Push',ref,value,x,231.14)
        c.connect(ref,'1',net,-5.08);c.connect(ref,'2','GND',5.08)
    converter=['LV','GND_L','LV1','LV2','LV3','LV4','HV','GND_H','HV1','HV2','HV3','HV4']
    c.module('Carrier:Level_Converter',passive(converter[:6]),passive(converter[6:]),15.24)
    c.instance('Carrier:Level_Converter','U2','Existing BSS138 module',238.76,223.52,ref_y=207.01)
    mapping={'LV':'+3V3','GND_L':'GND','LV1':'SDA_3V3','LV2':'SCL_3V3',
             'HV':'+5V','GND_H':'GND','HV1':'SDA_5V','HV2':'SCL_5V'}
    for pin in converter:c.connect('U2',pin,mapping.get(pin),-5.08 if pin in converter[:6] else 5.08)
    c.module('Carrier:LCD_Header',[],passive(['1','2','3','4']),10.16)
    c.instance('Carrier:LCD_Header','J4','LCD: GND 5V SDA SCL',297.18,223.52,ref_y=208.28)
    for pin,net in {'1':'GND','2':'+5V','3':'SDA_5V','4':'SCL_5V'}.items():c.connect('J4',pin,net)
    c.module('Carrier:Encoder',[],passive(['A','C','B','S1','S2']),10.16)
    c.instance('Carrier:Encoder','SW3','ACZ11BR1E-20FD1-12C',365.76,223.52,ref_y=207.01,
               properties={'Manufacturer':'Same Sky','MPN':'ACZ11BR1E-20FD1-12C',
                       'Datasheet':'https://www.sameskydevices.com/product/resource/acz11.pdf'})
    for pin,net in {'A':'ENC_A','C':'GND','B':'ENC_B','S1':'ENC_PUSH','S2':'GND'}.items():c.connect('SW3',pin,net)
    c.instance('Device:R_Small','R13','1k 1%',139.7,213.36)
    c.connect('R13','1','STATUS_LED_GP6',0,-5.08)
    c.connect('R13','2','STATUS_LED_A',0,5.08)
    c.instance('Device:LED','D5','Green status LED',139.7,236.22,90,
               properties={'Manufacturer':'Kingbright','MPN':'WP710A10LZGCK',
                           'Datasheet':'https://www.kingbrightusa.com/images/catalog/SPEC/WP710A10LZGCK.pdf'})
    c.connect('D5','2','STATUS_LED_A',0,-5.08)
    c.connect('D5','1','GND',0,5.08)
    for ref,net,x in [('#FLG01','+5V',139.7),('#FLG02','GND',165.1)]:
        c.instance('power:PWR_FLAG',ref,'PWR_FLAG',x,175.26)
        c.connect(ref,'1',net,0,5.08)
    c.text('PGA2350 CARRIER — ELECTRICAL DRAFT / NOT FOR MANUFACTURE',10.16,7.62,1.6)
    c.text('UPSTREAM: HUB / MAC',10.16,20.32)
    c.text('MOONLANDER ONLY / ALWAYS-POWERED 5V',10.16,96.52,1)
    c.text('STEREO AUDIO — GP40 / GP41',233.68,22.86)
    c.text('Left',233.68,43.18);c.text('Right',233.68,116.84)
    c.text('USB HOST SERIES / BULK',10.16,160.02)
    c.text('BOOT / RESET',10.16,208.28)
    c.text('DISPLAY LEVEL CONVERSION',198.12,193.04)
    c.text('ENCODER',345.44,193.04)
    c.text('STATUS LED / GP6',121.92,195.58,1)
    c.text('Module pads use signal names; see separate 90 x 75mm placement study.\nUSB breakouts: published 333011 v1.2.0 pin order; confirm delivered revision.\nNo input-ramp switch, USB-C power controller or factory assembly.\nSource advertises 1.5A; supported setup remains tested Anker data port + brightness 175.',10.16,256.54,1)
    c.text('Capacitor positive terminals face BIAS_L / BIAS_R.\nBAT85S cathode is pin 1 (band). Clamps are not isolation.\nAll grounds common; route keyboard returns away from audio.\nRouted review draft; independent follow-up pending.',233.68,256.54,1)
    c.save()


if __name__ == '__main__':
    main()
