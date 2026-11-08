"""Check exported carrier connections independently of the schematic generator."""
import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def checks(root):
    components = {c.get('ref'): c for c in root.findall('components/comp')}
    nets = {n.get('name').removeprefix('/'): {(p.get('ref'), p.get('pin')) for p in n.findall('node')}
            for n in root.findall('nets/net')}
    pin_net = {pin: name for name, pins in nets.items() for pin in pins}
    results = []

    def expect(name, assignments, values=None, members=None):
        errors = [f'{ref}.{pin}: expected {net}, got {pin_net.get((ref,pin))}'
                  for (ref,pin), net in assignments.items() if pin_net.get((ref,pin)) != net]
        for ref, value in (values or {}).items():
            if ref not in components or components[ref].findtext('value') != value:
                errors.append(f'{ref}: expected value {value}')
        for net,pins in (members or {}).items():
            if nets.get(net) != pins:
                errors.append(f'{net}: unexpected branch connections')
        results.append({'name': name, 'pass': not errors, 'errors': errors})

    for ref, cc1, cc2, rail, value, dp, dm in (
            ('J1','R1','R2','GND','5.1k 1%','USB_DP','USB_DM'),
            ('J2','R3','R4','+5V','22k 1%','KBD_DP','KBD_DM')):
        expect(ref+' USB-C pin order and independent CC terminations', {
            (ref,'1'):'GND',(ref,'2'):ref+'_CC2',(ref,'4'):dm,(ref,'5'):dp,
            (ref,'7'):ref+'_CC1',(ref,'8'):'+5V',
            (cc1,'1'):ref+'_CC1',(cc1,'2'):rail,
            (cc2,'1'):ref+'_CC2',(cc2,'2'):rail}, {cc1:value,cc2:value})
        results.append({'name':ref+' sideband pins unconnected', 'pass': all(
            pin_net.get((ref,p)) is None or pin_net[(ref,p)].startswith('unconnected-') for p in ('3','6')),
            'errors':[]})
    expect('Native USB and power branches', {('U1','U+'):'USB_DP',('U1','U-'):'USB_DM',
        ('U1','VB'):'+5V',('U1','3V3'):'+3V3',('C1','1'):'+5V',('C1','2'):'GND'},
        {'C1':'470uF / >=10V','U1':'PGA2350 / RP2350B A4'})
    expect('PIO host data through series resistors', {
        ('U1','GP0'):'HOST_GP0_DP',('R5','1'):'HOST_GP0_DP',('R5','2'):'KBD_DP',
        ('U1','GP1'):'HOST_GP1_DM',('R6','1'):'HOST_GP1_DM',('R6','2'):'KBD_DM'},
        {'R5':'22R','R6':'22R'})
    expect('TRS jack channels', {('J3','S'):'GND',('J3','R'):'LINE_R',('J3','T'):'LINE_L'},
        {'J3':'SJ1-3533NG'})
    for channel, gp, coupling, shunt, upper, lower, series, high, low in (
            ('L','GP40','C2','C3','R7','R8','R9','D1','D2'),
            ('R','GP41','C4','C5','R10','R11','R12','D3','D4')):
        bias,adc,line='BIAS_'+channel,'ADC_'+channel,'LINE_'+channel
        expect(channel+' audio bias, polarity, filter and ADC pin', {
            ('U1',gp):adc,(coupling,'1'):bias,(coupling,'2'):line,
            (upper,'1'):'+3V3',(upper,'2'):bias,(lower,'1'):bias,(lower,'2'):'GND',
            (series,'1'):bias,(series,'2'):adc,(shunt,'1'):adc,(shunt,'2'):'GND',
            (high,'1'):'+3V3',(high,'2'):adc,(low,'1'):adc,(low,'2'):'GND'},
            {upper:'22k 1%',lower:'22k 1%',series:'10k 1%',
             coupling:'10uF / >=16V',shunt:'2.2nF C0G 5%',high:'BAT85S',low:'BAT85S'})
        results.append({'name':channel+' ADC connects only to filter/clamps/module',
                        'pass':nets.get(adc)=={('U1',gp),(series,'2'),(shunt,'1'),(high,'2'),(low,'1')},
                        'errors':[]})
    expect('LCD voltage sides and pin order', {('U1','GP2'):'SDA_3V3',('U1','GP3'):'SCL_3V3',
        ('U2','LV'):'+3V3',('U2','HV'):'+5V',('U2','GND_L'):'GND',('U2','GND_H'):'GND',
        ('U2','LV1'):'SDA_3V3',('U2','LV2'):'SCL_3V3',('U2','HV1'):'SDA_5V',('U2','HV2'):'SCL_5V',
        ('J4','1'):'GND',('J4','2'):'+5V',('J4','3'):'SDA_5V',('J4','4'):'SCL_5V'})
    expect('Encoder and service buttons', {('U1','GP4'):'ENC_A',('U1','GP5'):'ENC_B',('U1','GP8'):'ENC_PUSH',
        ('SW3','A'):'ENC_A',('SW3','B'):'ENC_B',('SW3','C'):'GND',('SW3','S1'):'ENC_PUSH',('SW3','S2'):'GND',
        ('SW1','1'):'BOOTSEL',('SW1','2'):'GND',('U1','BS'):'BOOTSEL',
        ('SW2','1'):'RUN',('SW2','2'):'GND',('U1','RUN'):'RUN'})
    expect('All six module grounds connected', {('U1',f'GND{i}'):'GND' for i in range(1,7)})
    expect('Status LED polarity and current limiting', {
        ('U1','GP6'):'STATUS_LED_GP6',('R13','1'):'STATUS_LED_GP6',
        ('R13','2'):'STATUS_LED_A',('D5','2'):'STATUS_LED_A',('D5','1'):'GND'},
        {'R13':'1k 1%','D5':'Green status LED'},
        {'STATUS_LED_GP6':{('U1','GP6'),('R13','1')},
         'STATUS_LED_A':{('R13','2'),('D5','2')}})
    results.append({'name':'Audio bias nodes are distinct and isolated from 5V',
                    'pass':pin_net.get(('C2','1')) != pin_net.get(('C4','1')) and
                    not any(ref in ('R7','R8','R9','R10','R11','R12') for ref,p in nets.get('+5V',set())),
                    'errors':[]})
    return results


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('netlist',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    result=checks(ET.parse(args.netlist).getroot())
    text=json.dumps({'scope':'Electrical connectivity only; no physical or functional validation',
                     'checks':result},indent=2)+'\n'
    if args.output:args.output.write_text(text)
    for check in result:
        print(('PASS ' if check['pass'] else 'FAIL ')+check['name'])
        for error in check['errors']:print('  '+error)
    raise SystemExit(0 if all(c['pass'] for c in result) else 1)
