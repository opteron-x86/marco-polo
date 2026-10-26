#!/usr/bin/env python3
"""Generate reviewed route-security profiles with a fixed threat budget per road."""
import argparse
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def generate(coverage, economy):
    routes = {r['id']: r for r in economy['routes']}
    markets = {m['id'] for m in economy['markets']}
    sums = {}
    for row in coverage['candidates']:
        if row['decision'] != 'include':
            continue
        if row['market'] not in markets or not row['position']:
            raise ValueError(f'Unverified market/position: {row["editor_id"]}')
        if row['kind'] in ('bandit', 'forsworn') and 'LocTypeClearable' not in row['keywords']:
            raise ValueError('Camp has no clearable flag')
        for road, weight in row['corridors'].items():
            if road not in routes or not math.isfinite(weight) or not 0 < weight <= 5:
                raise ValueError('Invalid road association')
            sums[road] = sums.get(road, 0) + weight
    sites = []
    for row in coverage['candidates']:
        if row['decision'] != 'include':
            continue
        site = {'location': row['location'], 'name': row['name'], 'market': row['market'],
                'kind': row['kind'], 'threat': 1.5, 'routes': {}}
        for road, weight in row['corridors'].items():
            route = routes[road]
            reverse = route['to'] + '-' + route['from']
            if reverse not in routes:
                raise ValueError('Missing reverse road')
            exposure = round(coverage['corridorThreatBudgets'][road] * weight / sums[road] / 1.5, 10)
            if not 0 < exposure <= 1:
                raise ValueError('Exposure exceeds supported range')
            site['routes'][road] = site['routes'][reverse] = exposure
        if row['kind'] == 'fort':
            site.update(initialOccupants=row['initialOccupants'], protection=1,
                        demand={'food': .12, 'grain': .05, 'medicine': .1, 'weapons': .15, 'armor': .15})
        sites.append(site)
    # Explicit negative control, retained from the original pilot.
    sites.append({'location': 'Skyrim.esm|018EE9', 'name': 'Bleak Falls Barrow',
                  'market': 'falkreath', 'kind': 'crypt', 'threat': 0, 'routes': {}})
    return {'schema': 1, 'enabled': True, 'maximumPressure': .8,
            'maximumExtraDemand': .3, 'sites': sorted(sites, key=lambda s: s['location'])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    coverage = json.loads((ROOT / 'data/security-coverage.json').read_text())
    economy = json.loads((ROOT / 'package/SKSE/Plugins/MarcoPolo/economy.json').read_text())
    target = ROOT / 'package/SKSE/Plugins/MarcoPolo/security.json'
    data = json.dumps(generate(coverage, economy), indent=2, ensure_ascii=False) + '\n'
    if args.check:
        if target.read_text() != data:
            raise ValueError('Regenerate security.json from reviewed coverage')
    else:
        target.write_text(data)
    print('Verified reviewed security profiles' if args.check else 'Generated reviewed security profiles')


if __name__ == '__main__':
    main()
