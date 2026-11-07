"""Bounded ngspice checks of the frozen candidate, using KiCad's bundled library.

Standard library only. TI model ZIP is supplied locally, never redistributed.
This is not a complete board, ADC, USB, thermal or fault-survival simulation.
"""

import argparse
import cmath
import ctypes
import hashlib
import itertools
import json
import math
from pathlib import Path
import zipfile


LIBRARY = Path('/Applications/KiCad/KiCad.app/Contents/PlugIns/sim/libngspice.0.dylib')


def audio_transfer(frequency, source=100, upper=22000, lower=22000,
                   series=10000, coupling=10e-6, shunt=2.2e-9, adc=1e12):
    """Independent complex-impedance solution for checking the SPICE AC result."""
    s = 2j * math.pi * frequency
    z_adc = 1 / (1 / adc + s * shunt)
    z_bias = 1 / (1 / upper + 1 / lower + 1 / (series + z_adc))
    return z_bias / (source + 1 / (s * coupling) + z_bias) * z_adc / (series + z_adc)


def crossing(data, column, level):
    for a, b in zip(data, data[1:]):
        if a[column] < level <= b[column]:
            return a[0] + (b[0] - a[0]) * (level - a[column]) / (b[column] - a[column])
    raise ValueError(f'No upward crossing of {level}')


class Spice:
    def __init__(self, library):
        self.log = []
        self.lib = ctypes.CDLL(str(library))
        callback = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p,
                                   ctypes.c_int, ctypes.c_void_p)

        @callback
        def output(message, identifier, user):
            self.log.append(message.decode(errors='replace'))
            return 0

        self.callback = output  # Keep callback alive while ngspice owns it.
        self.lib.ngSpice_Init.argtypes = [callback] + [ctypes.c_void_p] * 6
        self.lib.ngSpice_Command.argtypes = [ctypes.c_char_p]
        self.lib.ngSpice_Init(output, None, None, None, None, None, None)
        self.initialization_log = list(self.log)
        for name in ('analog', 'spice2poly', 'xtradev'):
            self.command(f'codemodel {library.parent / "ngspice" / (name + ".cm")}')
        self.command('set ngbehavior=ps')

    def command(self, value):
        return self.lib.ngSpice_Command(value.encode())

    def run(self, directory, name, circuit, analysis, vectors):
        self.command('destroy all')
        self.log.clear()
        path = directory / (name + '.cir')
        data = directory / (name + '.dat')
        path.write_text(circuit + '\n.control\n' + analysis +
                        '\nset wr_singlescale\nset wr_vecnames\n' +
                        f'wrdata {data} {vectors}\n.endc\n.end\n')
        status = self.command(f'source {path}')
        (directory / (name + '.log')).write_text('\n'.join(self.log) + '\n')
        errors = [line for line in self.log if any(t in line.lower() for t in
                  ('error', 'failed', 'aborted', 'timestep too small', 'singular matrix'))]
        if status or errors or not data.exists():
            raise RuntimeError(f'{name}: simulator failure; inspect saved log: {errors[:3]}')
        result = [[float(v) for v in line.split()] for line in data.read_text().splitlines()[1:]]
        if not result or not all(math.isfinite(v) for row in result for v in row):
            raise RuntimeError(f'{name}: empty or nonfinite result')
        return result


def verify_snapshot(snapshot):
    manifest = json.loads((snapshot / 'manifest.json').read_text())
    for name, expected in manifest['artifact_sha256'].items():
        assert hashlib.sha256((snapshot / name).read_bytes()).hexdigest() == expected, name
    parts = {c['reference']: c for c in json.loads((snapshot / 'audit.json').read_text())['components']}
    expected = {
        'R14': ('22k 1%', {'1': '+3V3', '2': '/Audio frontend/BIAS_L'}),
        'R15': ('22k 1%', {'1': '/Audio frontend/BIAS_L', '2': 'GND'}),
        'R16': ('10k 1%', {'1': '/Audio frontend/BIAS_L', '2': '/Audio frontend/AUDIO_L_ADC'}),
        'C26': ('10uF 16V', {'1': '/Audio frontend/BIAS_L', '2': '/Audio frontend/LINE_L'}),
        'C28': ('2.2nF C0G 5%', {'1': '/Audio frontend/AUDIO_L_ADC', '2': 'GND'}),
        'R24': ('100k', {'1': '/USB and power/VBUS_FUSED', '2': 'Net-(U6-EN)'}),
        'R25': ('20k', {'1': 'Net-(U6-EN)', '2': 'GND'}),
        'C30': ('1uF', {'1': '/USB and power/VBUS_FUSED', '2': 'GND'}),
        'C31': ('10nF 50V', {'1': 'Net-(U6-CT)', '2': 'GND'}),
        'U6': ('TPS22953DQCR', {
            '1': '/USB and power/VBUS_FUSED', '2': '/USB and power/VBUS_FUSED',
            '3': '/USB and power/VBUS_FUSED', '4': 'Net-(U6-EN)', '5': 'GND',
            '6': 'Net-(U6-CT)', '7': 'GND', '8': '+5V', '9': '+5V',
            '10': '+5V', '11': 'GND'}),
    }
    for left, right in (('R14', 'R17'), ('R15', 'R18'), ('R16', 'R19'),
                        ('C26', 'C27'), ('C28', 'C29')):
        value, nets = expected[left]
        expected[right] = (value, {pin: net.replace('_L', '_R') for pin, net in nets.items()})
    for ref, (value, nets) in expected.items():
        assert parts[ref]['value'] == value, ref
        assert {p['number']: p['net'] for p in parts[ref]['pads'] if p['number']} == nets, ref
    for ref, channel in (('D1', 'L'), ('D2', 'R')):
        assert parts[ref]['value'] == 'BAT54S-7-F'
        assert {p['number']: p['net'] for p in parts[ref]['pads'] if p['number']} == {
            '1': 'GND', '2': '+3V3', '3': f'/Audio frontend/AUDIO_{channel}_ADC'}


def run(args):
    verify_snapshot(args.snapshot)
    args.out.mkdir(parents=True, exist_ok=False)
    directory = args.out.resolve()
    if ' ' in str(directory):
        raise ValueError('Use an output path without spaces for SPICE control commands')
    with zipfile.ZipFile(args.model_zip) as archive:
        model = archive.read('SLVMAS6A/TPS22953_TRANS.LIB').decode('cp1252')
    # No model equations modified; only normalize text encoding for ngspice.
    model_path = directory / 'TPS22953_TRANS.LIB'
    model_path.write_text(model)
    spice = Spice(args.library)
    report = {'snapshot': args.snapshot.name,
              'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'snapshot_manifest_sha256': hashlib.sha256((args.snapshot / 'manifest.json').read_bytes()).hexdigest(),
              'model_url': 'https://www.ti.com/lit/zip/slvmas6b',
              'model_zip_sha256': hashlib.sha256(args.model_zip.read_bytes()).hexdigest(),
              'simulator': spice.initialization_log, 'audio': [], 'startup': [], 'hotplug': []}
    for adc in (1e12, 100000):
        data = spice.run(directory, f'audio-{adc:g}', f'''Audio small-signal model
V33 v33 0 3.3
Vmid mid 0 1.65
Vin input 0 dc 0 ac 1
Rsource input line 100
C26 line bias 10u
R14 v33 bias 22k
R15 bias 0 22k
R16 bias adc 10k
C28 adc 0 2.2n
* Effective AC loading only; not a model of ADC DC leakage or sampling.
Radc adc mid {adc:g}''', 'ac dec 100 0.1 100000', 'v(adc)')
        for f, real, imag in data:
            expected = audio_transfer(f, adc=adc)
            assert abs(complex(real, imag) - expected) < 2e-7, f
        levels = {}
        for target in (20, 60, 100, 1000, 5000, 7812.5, 15625):
            h = audio_transfer(target, adc=adc)
            levels[str(target)] = {'gain_db': 20 * math.log10(abs(h)),
                                   'phase_degrees': math.degrees(cmath.phase(h))}
        report['audio'].append({'effective_ac_load_ohm': adc, 'levels': levels})
    # Deterministic component/source corners, independently solved AC network.
    corners = []
    for source, ru, rd, rs, cc, cf, adc in itertools.product(
            (100, 1000), (.99, 1.01), (.99, 1.01), (.99, 1.01),
            (.8, 1.2), (.95, 1.05), (1e12, 100000)):
        corners.append({str(f): 20 * math.log10(abs(audio_transfer(
            f, source, 22000*ru, 22000*rd, 10000*rs, 10e-6*cc, 2.2e-9*cf, adc)))
            for f in (20, 60, 1000, 7812.5)})
    report['audio_corner_count'] = len(corners)
    report['audio_corner_gain_db'] = {f: [min(c[f] for c in corners), max(c[f] for c in corners)]
                                     for f in corners[0]}
    # Equivalent resistive loads: not models of processor/display/USB sequencing.
    for name, vs, source, bulk, ct, load in (
            ('unloaded', 5, .15, 500e-6, 10e-9, 1e9),
            ('idle-equivalent', 5, .15, 500e-6, 10e-9, 5/.285),
            ('peak-equivalent', 5, .15, 500e-6, 10e-9, 5/.724),
            ('low-source', 4.75, .5, 600e-6, 9e-9, 5/.724)):
        data = spice.run(directory, 'startup-' + name, f'''U6 {name}; assumed equivalent resistive load
.include {model_path}
Vsource supply 0 PWL(0 0 1u {vs} 60m {vs})
Rsource supply vin {source}
C30 vin 0 1u
R24 vin en 100k
R25 en 0 20k
C31 ct 0 {ct}
* Model order: VIN1 VIN2 BIAS EN OUT1 OUT2 PG SNS CT GND PAD
XU6 vin vin vin en out out 0 out ct 0 0 TPS22953_TRANS
Resr out reservoir 0.1
Cbulk reservoir 0 {bulk}
Rload out 0 {load}''', 'tran 5u 60m', 'v(vin) v(out) v(en) i(vsource)')
        ten = crossing(data, 2, .1 * vs)
        ninety = crossing(data, 2, .9 * vs)
        report['startup'].append({'name': name, 'source_volts': vs,
            'source_ohms': source, 'bulk_f': bulk, 'timing_f': ct, 'load_ohms': load,
            't10_ms': ten*1000, 't90_ms': ninety*1000, 'rise10_90_ms': (ninety-ten)*1000,
            'final_out_v': data[-1][2],
            'max_source_a_after_100us': max(-r[4] for r in data if r[0] >= 100e-6)})
    # Isolated passive input network: DOES NOT model U6 overstress survival.
    for inductance, resistance, capacitance in itertools.product((.1e-6, 1e-6, 3e-6),
                                                               (.1, .3, .8), (.6e-6, 1e-6)):
        name = f'hotplug-{inductance:g}-{resistance:g}-{capacitance:g}'
        data = spice.run(directory, name, f'''Assumed cable RLC; linear input-only sensitivity
Vsource source 0 PWL(0 0 1u 5.2 100u 5.2)
Rseries source a {resistance}
Lcable a vin {inductance}
Cinput vin 0 {capacitance}
Rleak vin 0 1meg''', 'tran 10n 100u', 'v(vin)')
        report['hotplug'].append({'cable_h': inductance, 'series_ohms': resistance,
                                  'input_f': capacitance, 'peak_v': max(r[1] for r in data)})
    (directory / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('simulator', 'hotplug')}, indent=2))
    print('Input-only assumed-network peak range:', min(r['peak_v'] for r in report['hotplug']),
          max(r['peak_v'] for r in report['hotplug']))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', type=Path, required=True)
    parser.add_argument('--model-zip', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--library', type=Path, default=LIBRARY)
    run(parser.parse_args())
