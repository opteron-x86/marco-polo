# Skyrim True Economy and Merchantry 0.8.0 — test build

Seasonal markets now build reserves ahead of winter. Productive months increase
output around the configured annual rate, while each commodity has its own
seasonal sensitivity and storage allowance. Durable goods can carry longer
reserves; perishables receive smaller buffers bounded by shelf life.

In 16 paired seeds over five years, unmet demand for seasonal goods fell from
12.60% to 0.79% in peace and from 12.74% to 0.90% during continued unrest.
Temporary supply disruptions still caused shortages, followed by recovery.
The separate player campaigns retained profitable trading. Warehouse spoilage
increased as more goods were held in storage.

See the [results and charts](SEASONAL-SIMULATION.md) for the comparisons and
remaining limits, and [game testing](TESTING.md) for the live checks.

Existing markets keep their saved stock, prices and cargo. Reserves accumulate
through ordinary production and deliveries; updating does not refill inventories.
Save schema 4 is unchanged. Install the updated core with Skyrim closed.
