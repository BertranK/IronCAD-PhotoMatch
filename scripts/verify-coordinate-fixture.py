"""Create an isolated scene and check known vertices plus camera round trips.

Run with the project venv while exactly one IronCAD host is open. This creates
new test geometry and leaves its scene open; existing documents are preserved.
"""
import argparse
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'gui'))
from bridge import Bridge


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', type=int)
    parser.add_argument('--output', type=Path, default=ROOT / 'evidence' / 'coordinate-roundtrip.json')
    args = parser.parse_args()
    bridge = Bridge(args.host, timeout_ms=30000)
    state = bridge.call('status')
    state = bridge.call('test_coordinate_fixture', state['session'])
    report = {'fixture': state, 'roundtrips': []}
    session = state['session']
    cameras = [
        {'position': [.55, -.6, .65], 'direction': [-.5, .71, -.49], 'up': [0, 0, 1], 'field_sdk': .8},
        {'position': [.222, .095, .453], 'direction': [-.13, .13, -.12], 'up': [0, 0, 1], 'field_sdk': .45},
    ]
    try:
        for camera in cameras:
            applied = bridge.call('apply', session, camera)
            measured = bridge.call('measure', session)
            for _ in range(60):
                if not measured['measuring']:
                    break
                time.sleep(.1)
                measured = bridge.call('status')
            if measured['measuring']:
                raise RuntimeError('IronCAD did not deliver a draw measurement')
            restored = bridge.call('restore', session)
            report['roundtrips'].append({'applied': applied, 'measured': measured, 'restored': restored})
        report['passed'] = (state['coordinate_fixture']['passed'] and all(
            row['restored']['restored'] and row['restored']['model_transforms_bounds_unchanged']
            and not row['restored']['errors'] and any(
                c['name'] == 'minimum_radians_full' and c['max_error_px'] is not None and c['max_error_px'] <= 1
                for c in row['measured']['fov_candidates']) for row in report['roundtrips']))
    finally:
        current = bridge.call('status')
        if current['session'] == session and current['captured']:
            report['cleanup'] = bridge.call('restore', session)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('PASS' if report['passed'] else 'FAIL')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
