#!/usr/bin/env python3
"""Combine paired campaign shards and optionally plot their long-term results."""
import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path
from statistics import mean


def load(paths):
    runs, monthly, goods = [], [], []
    meta = []
    for path in paths:
        j = json.loads((path / 'summary.json').read_text())
        meta.append(j)
        runs.extend(j['runs'])
        with (path / 'monthly.csv').open() as f: monthly.extend(csv.DictReader(f))
        with (path / 'goods.csv').open() as f: goods.extend(csv.DictReader(f))
    keys = {(r['seed'], r['world'], r['player']) for r in runs}
    if len(keys) != len(runs): raise ValueError('Duplicate campaign seeds/scenarios')
    if len({j['end_day'] for j in meta}) != 1: raise ValueError('Mixed campaign lengths')
    return runs, monthly, goods, meta


def summarize(data):
    runs, monthly, goods, meta = data
    grouped = defaultdict(list)
    for r in runs: grouped[(r['world'], r['player'])].append(r)
    result = {'seeds': sorted({r['seed'] for r in runs}), 'runs': len(runs),
              'end_day': meta[0]['end_day'], 'scenarios': {},
              'maximum_stock_accounting_error': max(j['maximum_stock_accounting_error'] for j in meta),
              'maximum_save_bytes': max(j['maximum_save_bytes'] for j in meta),
              'maximum_shipments': max(j['maximum_shipments'] for j in meta)}
    for (world, player), rows in sorted(grouped.items()):
        scenario = {'years': [], 'player_totals': {}}
        for year in range(len(rows[0]['years'])):
            scenario['years'].append({key: mean(r['years'][year][key] for r in rows) for key in rows[0]['years'][year]})
        for key in rows[0]['player_totals']:
            values = [r['player_totals'][key] for r in rows]
            scenario['player_totals'][key] = {'mean': mean(values), 'min': min(values), 'max': max(values)}
        result['scenarios'][world + '/' + player] = scenario
    prices = {(r['world'], r['player'], r['seed'], r['year'], r['market'], r['good']): float(r['mean_price']) for r in goods}
    differences = defaultdict(list)
    empty = defaultdict(lambda: defaultdict(int))
    for r in goods:
        if int(r['year']) != len(runs[0]['years']): continue
        if r['player'] == 'observer':
            other = prices[(r['world'], 'explorer', r['seed'], r['year'], r['market'], r['good'])]
            differences[r['world']].append(abs(other / float(r['mean_price']) - 1)*100)
            empty[r['world']][r['seed']] += float(r['empty_fraction']) >= .99
    result['final_year_discovery_price_difference_pct'] = {w: mean(v) for w, v in differences.items()}
    result['final_year_chronically_empty_categories'] = {w: mean(v.values()) for w, v in empty.items()}
    worst = defaultdict(list)
    for r in goods:
        if r['player']=='observer' and int(r['year'])==len(runs[0]['years']):
            worst[(r['world'], r['market'], r['good'])].append(r)
    result['persistent_shortages'] = []
    for (world, market, good), rows in worst.items():
        unmet = mean(float(r['unmet_demand_fraction']) for r in rows)
        ceiling = mean(float(r['ceiling_fraction']) for r in rows)
        if unmet > .1 or ceiling > .9:
            result['persistent_shortages'].append({'world': world, 'market': market, 'good': good,
                'unmet_fraction': unmet, 'ceiling_fraction': ceiling,
                'empty_fraction': mean(float(r['empty_fraction']) for r in rows),
                'price': mean(float(r['mean_price']) for r in rows)})
    result['persistent_shortages'].sort(key=lambda r: (r['world'], -r['unmet_fraction']))
    return result


def plot(before, after, output):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    colors = {'unrest':'#a85839', 'progressive_clear':'#33856f', 'war_to_peace':'#3f6d9d', 'recurring_unrest':'#8a629e'}
    names = {'unrest':'Continued unrest', 'progressive_clear':'Gradual clearing', 'war_to_peace':'War ends after year 1', 'recurring_unrest':'Recurring unrest'}
    def series(rows, world, player, metric):
        grouped = defaultdict(list)
        for r in rows:
            if r['world']==world and r['player']==player: grouped[int(r['period'])].append(float(r[metric]))
        return [(min(after[3][0]['end_day'],30+month*30)/365,mean(values)) for month,values in sorted(grouped.items())]
    fig, axes = plt.subplots(2,2,figsize=(13,8.5),layout='constrained')
    panels = [('observer','empty_offer_fraction','Empty shop offers (%)',100),
              ('observer','price_ceiling_fraction','Regional prices at maximum (%)',100),
              ('observer','unmet_demand_fraction','Unfulfilled regional demand (%)',100),
              ('trader','mean_player_gold','Trader’s gold',1)]
    for ax,(player,metric,label,scale) in zip(axes.flat,panels):
        for world in colors:
            points=series(after[1],world,player,metric)
            ax.plot([x for x,y in points],[y*scale for x,y in points],color=colors[world],label=names[world],linewidth=1.7)
        points=series(before[1],'unrest',player,metric)
        ax.plot([x for x,y in points],[y*scale for x,y in points],color='#777777',linestyle='--',linewidth=1.4,label='Previous retail model, continued unrest')
        ax.set(xlabel='Game years',ylabel=label,xlim=(0,after[3][0]['end_day']/365));ax.grid(alpha=.18)
        ax.spines[['top','right']].set_visible(False)
        ax.ticklabel_format(axis='y',style='plain')
    handles,labels=axes[0,0].get_legend_handles_labels()
    fig.legend(handles,labels,loc='outside lower center',ncol=3,frameon=False,fontsize=9)
    years = after[3][0]['end_day']/365
    count = len({r['seed'] for r in after[0]})
    fig.suptitle(f'Skyrim True Economy and Merchantry — {years:g}-year campaigns',fontsize=17)
    axes[0,0].set_title(f'Revised retail model; {count}-seed means',loc='left',fontsize=10)
    fig.savefig(output,dpi=170)
    plt.close(fig)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before',nargs='+',type=Path,required=True)
    p.add_argument('--after',nargs='+',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--plot',type=Path)
    args=p.parse_args()
    before,after=load(args.before),load(args.after)
    keys=lambda d:{(r['seed'],r['world'],r['player']) for r in d[0]}
    if keys(before)!=keys(after):raise ValueError('Comparison is not paired')
    if before[3][0]['end_day'] != after[3][0]['end_day']:
        raise ValueError('Comparison has different campaign lengths')
    result={'before':summarize(before),'after':summarize(after)}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    if args.plot:
        args.plot.parent.mkdir(parents=True,exist_ok=True)
        plot(before,after,args.plot)
    print(f'Compared {len(after[0])} paired campaigns, {len(result["after"]["seeds"])} seeds')

if __name__=='__main__':main()
