#!/usr/bin/env python3
"""Compare seasonal supply, storage, losses and recovery in paired experiments."""
import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLOWS = ('wanted', 'supplied', 'produced', 'spoiled', 'transit_loss', 'imports', 'exports')
MEANS = ('mean_stock', 'mean_price', 'ceiling_fraction', 'floor_fraction', 'empty_fraction')


def accumulate(target, row):
    target['rows'] += 1
    for key in FLOWS + MEANS:
        target[key] += float(row[key])
    if row['good'] == 'grain':
        target['grain_rows'] += 1
        target['grain_stock'] += float(row['mean_stock'])


def values(row):
    return {
        'unmet_fraction': 1 - row['supplied'] / row['wanted'] if row['wanted'] else 0,
        'spoil_fraction': row['spoiled'] / row['produced'] if row['produced'] else 0,
        'transit_loss_fraction': row['transit_loss'] / row['imports'] if row['imports'] else 0,
        **{key: row[key] / row['rows'] for key in MEANS},
    }


def load(paths, config):
    seasonal = {g['id'] for g in config['commodities'] if g.get('seasonal')}
    yearly = defaultdict(lambda: defaultdict(float))
    monthly = defaultdict(lambda: defaultdict(float))
    goods = defaultdict(lambda: defaultdict(float))
    roads = defaultdict(lambda: defaultdict(float))
    keys, metadata = set(), []
    for path in paths:
        metadata.append(json.loads((path / 'summary.json').read_text()))
        with (path / 'annual.csv').open() as stream:
            for row in csv.DictReader(stream):
                if row['good'] not in seasonal:
                    continue
                identity = tuple(row[k] for k in ('seed', 'scenario', 'period', 'market', 'good'))
                if identity in keys:
                    raise ValueError('Duplicate seasonal observation')
                keys.add(identity)
                group = (row['scenario'], int(row['period']))
                accumulate(yearly[group], row)
                accumulate(goods[group + (row['market'], row['good'])], row)
        with (path / 'monthly.csv').open() as stream:
            for row in csv.DictReader(stream):
                if row['good'] in seasonal:
                    accumulate(monthly[(row['scenario'], int(row['period']))], row)
        with (path / 'roads.csv').open() as stream:
            for row in csv.DictReader(stream):
                road = roads[(row['scenario'], int(row['year']), row['route'])]
                road['rows'] += 1
                for key in ('capacity', 'shipped', 'full_fraction'):
                    road[key] += float(row[key])
    if len({m['years'] for m in metadata}) != 1:
        raise ValueError('Mixed campaign lengths')
    return {'yearly': yearly, 'monthly': monthly, 'goods': goods, 'roads': roads,
            'keys': keys, 'metadata': metadata}


def summarize(data):
    years = data['metadata'][0]['years']
    seeds = sorted({int(k[0]) for k in data['keys']})
    scenarios = sorted({k[1] for k in data['keys']})
    out = {'seeds': seeds, 'years': years, 'runs': len(seeds) * len(scenarios),
           'maximum_stock_accounting_error': max(m['maximum_stock_accounting_error'] for m in data['metadata']),
           'maximum_sampled_save_bytes': max(m['maximum_sampled_save_bytes'] for m in data['metadata']),
           'scenarios': {}}
    for scenario in scenarios:
        result = {'years': [values(data['yearly'][scenario, year]) for year in range(1, years + 1)]}
        result['final_year_goods'] = [
            {'market': market, 'good': good, **values(row)}
            for (world, year, market, good), row in sorted(data['goods'].items())
            if world == scenario and year == years]
        result['final_year_goods'].sort(key=lambda r: -r['unmet_fraction'])
        routes = [
            {'route': route, 'utilization': row['shipped'] / row['capacity'],
             'full_fraction': row['full_fraction'] / row['rows']}
            for (world, year, route), row in data['roads'].items()
            if world == scenario and year == years]
        result['roads'] = sorted(routes, key=lambda r: -r['utilization'])
        if scenario.endswith('_disruption') and 'peace' in scenarios:
            result['excess_unmet_percentage_points_by_year'] = [
                100 * (values(data['yearly'][scenario, year])['unmet_fraction'] -
                       values(data['yearly']['peace', year])['unmet_fraction'])
                for year in range(1, years + 1)]
        out['scenarios'][scenario] = result
    return out


def plot(before, after, config, output):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    count = len({k[0] for k in after['keys']})
    end = after['metadata'][0]['years']
    grain_demand = sum(m['demand'].get('grain', 0) for m in config['markets'])
    markets = len(config['markets'])

    def series(data, scenario, field):
        result = []
        for (world, period), row in sorted(data['monthly'].items()):
            if world != scenario:
                continue
            value = (row['grain_stock'] * markets / row['grain_rows'] / grain_demand
                     if field == 'grain_days' else values(row)[field])
            result.append((min(end, period * 30 / 365), value))
        return result

    fig, axes = plt.subplots(2, 2, figsize=(13, 8.5), layout='constrained')
    panels = [('unmet_fraction', 'Seasonal demand unfulfilled (%)', 100),
              ('grain_days', 'Grain in warehouses (days of base demand)', 1),
              ('mean_price', 'Seasonal goods: regional price index', 1),
              ('unmet_fraction', 'Northern supply interruption: demand unfulfilled (%)', 100)]
    for i, (ax, (field, label, scale)) in enumerate(zip(axes.flat, panels)):
        for data, color, name in ((before, '#858585', '0.7.0'), (after, '#31745f', '0.8.0')):
            points = series(data, 'winter_disruption' if i == 3 else 'peace', field)
            ax.plot([x for x, y in points], [y * scale for x, y in points],
                    label=name, color=color, linewidth=1.8)
            if i == 3:
                normal = series(data, 'peace', field)
                ax.plot([x for x, y in normal], [y * scale for x, y in normal],
                        color=color, linestyle='--', linewidth=1, alpha=.6)
        ax.set(xlabel='Game years', ylabel=label, xlim=(0, end))
        if i == 3:
            ax.set_xlim(2.1, 3.7)
            ax.axvspan(838 / 365, 898 / 365, color='#d6b477', alpha=.25)
            ax.set_title('60-day disruption; dashed lines show peace', fontsize=10, loc='left')
        ax.grid(alpha=.18)
        ax.spines[['top', 'right']].set_visible(False)
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc='outside lower center', ncol=2, frameon=False)
    fig.suptitle(f'Seasonal supply and storage — {count} paired seeds, {end} years', fontsize=17)
    axes[0, 0].set_title('Peace scenarios except the interruption panel', fontsize=10, loc='left')
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=170)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before', nargs='+', type=Path, required=True)
    parser.add_argument('--after', nargs='+', type=Path, required=True)
    parser.add_argument('--config', type=Path, default=ROOT / 'package/SKSE/Plugins/MarcoPolo/economy.json')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--plot', type=Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    before, after = load(args.before, config), load(args.after, config)
    if before['keys'] != after['keys']:
        raise ValueError('Seasonal comparison is not paired')
    result = {'before': summarize(before), 'after': summarize(after)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    if args.plot:
        plot(before, after, config, args.plot)
    print(f'Compared {result["after"]["runs"]} paired seasonal campaigns')


if __name__ == '__main__':
    main()
