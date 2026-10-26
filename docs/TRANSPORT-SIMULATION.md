# Cargo allocation and merchant tests

Version 0.6.0. These are portable simulation results, not an in-game test.

## Allocation

Routes share cargo by the destination's missing units and proportional shortage.
A profitable price gap increases an order's weight by at most twofold. Every
route plans against the same snapshot: one export reserve per source, one
outstanding requirement per destination, and a capacity limit for each road.
Incoming shipments count toward requirements. Unused shares are redistributed.

Reordering goods, markets or roads produces identical results. Previously, the
first configured goods and roads took capacity first. Production rules, barter
spread and security strengths retain their previous values.

## Experiment

`marco_merchant_sim` uses 32 paired seeds, a shared 30-day warmup, then 60 days
under four conditions: unchanged hostile sites, Valtheim cleared, Imperial
occupation of Greymoor, or all nine configured raider sites cleared. The broad
Civil War conditions match the runtime. Every branch reloads on day 47.

Each market has six synthetic dealers. Their stock targets are 20 food, 20 drink,
20 iron, 8 weapons, 12 silver and 12 household goods. Each starts with 2,000 gold;
ordinary turnover, restocking and trades subsequently change funds and stock
without resets. Values use commodity reference values and one stock unit per
item. These are not reconstructions of individual Skyrim merchants.

Four traders buy cargo every 14 days, travel for 3–6 days, sell, and allow return
travel before buying again. Buyers with insufficient funds accept only part of
a delivery; the trader retains the rest. Native barter multipliers are 3.105
for buying and its reciprocal for selling, before the mod's existing adjustment.
Profits exclude travel expenses. The same experiment was compiled with the
0.5.0 economy implementation from `9db0761` for comparison.

## Results

Mean shop stock and single-item buy quotes over days 31–90, with sites hostile:

| Shop | Stock: 0.5.0 → 0.6.0 | Buy quote: 0.5.0 → 0.6.0 |
|---|---:|---:|
| Falkreath iron | 13.90 → 19.60 | 29.30 → 26.21 gold |
| Falkreath weapons | 5.41 → 8.00 | 220.61 → 159.12 gold |
| Pale food | 10.11 → 15.66 | 30.00 → 29.16 gold |
| Reach food | 20.00 → 17.77 | 24.14 → 27.99 gold |
| Haafingar food | 14.79 → 12.67 | 24.67 → 25.71 gold |

Some markets lose preferential access to cargo. Several inventories improve,
while other goods become dearer; allocation does not create additional supply.

With 0.6.0, clearing all configured raider sites reduced Winterhold's silver buy
quote **4.41%**, increased the Reach's **1.45%**, and reduced Falkreath iron
**0.71%**. Both silver directions held in all 32 seeds. Most sampled shops changed
less. Occupying Greymoor raised Falkreath weapon quotes **4.64%** and Eastmarch
food quotes **1.43%**. Network effects extend beyond the fort's own hold.

All four trade routes remained profitable in every seed and security scenario.
Mean cash profit from repeated journeys over 60 days, with hostile roads:

| Cargo and journey | Profit before expenses |
|---|---:|
| Drink: Rift → Pale | 757 gold |
| Weapons: Whiterun → Winterhold | 1,681 gold |
| Silver: Reach → Haafingar | 2,537 gold |
| Household goods: Rift → Winterhold | 451 gold |

The silver buyers could not afford their entire final deliveries: 71 items
remained unsold across 32 runs. Their purchase costs are included above, with
no speculative sale value. The other routes sold all purchased cargo. Repeated
bulk deliveries can still exhaust an individual merchant's funds.

## Import-dependent stress test

A separate two-market fixture has no local iron production at its destination,
a three-day road carrying up to 32 units daily, four dealers, one roadside camp,
and repeated five-item player deliveries. It uses the same paired warmup and
observation periods. Clearing its camp produced these means:

| Destination iron dealer | Hostile road | Cleared road |
|---|---:|---:|
| Stock, against a target of 30 | 13.58 | 30.08 |
| Single-item buy quote | 44.40 gold | 22.35 gold |
| Trader profit over 60 days | 365.34 gold | 12.84 gold |

The price fell **49.66%**, with a 49.05–49.96% fall across individual seeds.
Player deliveries can raise stock above its target. The much smaller margin
is expected when normal freight satisfies the shortage. This deliberately
constrained fixture demonstrates the mechanism; it does not predict a 50%
discount after clearing an ordinary Skyrim camp.

## Verification and limits

Every transfer checks item and gold accounting. The experiment checks finite
funds, remaining player cargo, exact economy restoration and per-commodity
stock accounting; its largest residual was 1.12e-11 units. Focused tests cover
shared reserves and requirements, in-flight cargo, capacity redistribution and
configuration order. Existing long-run, pricing, persistence and security tests
remain in the suite. CI also runs a two-seed merchant experiment.

A local Release benchmark with 60 merchants and 60 item forms each measured
1.63 ms per daily tick on average (1.69 ms at the 95th percentile), compared
with 2.79 ms for 0.5.0. Both used GCC `-O3 -DNDEBUG`, a 30-day warmup and 120
measured days. These timings cover the portable core, not Skyrim inventory
updates or frame time.

Economy schema 4 is unchanged. This build adds no engine hooks or save fields.
Real inventories, barter UI, save/reload and location transitions still need
Skyrim testing. The curated site coverage and fort limitations in the
[security report](SECURITY-SIMULATION.md) remain. Rumors still report prices;
they do not explain named route events or production costs.

## Reproduce

```sh
cmake -S . -B build/security-simulation -G Ninja -DMARCO_BUILD_PLUGIN=OFF -DMARCO_SANITIZERS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/security-simulation -j 6
ctest --test-dir build/security-simulation --output-on-failure
build/security-simulation/marco_merchant_sim package/SKSE/Plugins/MarcoPolo/economy.json package/SKSE/Plugins/MarcoPolo/security.json build/merchant-results 32
```

Use an empty output directory. `shops.csv` records daily inventories, quotes and
cash; `trades.csv` records transfers; `summary.json` contains means and totals.
