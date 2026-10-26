# Skyrim True Economy and Merchantry 0.7.0 — test build

Route security now covers 39 locations: 25 bandit sites, five Forsworn sites and
nine Civil War forts. Road exposure is shared across those locations, so adding
more camps does not multiply a road's total threat. Clearing affects transport;
military occupation also adds local demand. It does not grant production bonuses.

Scarce goods can now reach shop shelves before household consumption takes all
available supply. Background shop sales count toward regional demand, fractional
turnover uses the correct expected rate, and restocking favors depleted offers.
Merchants still pay for stock from the warehouse.

The [five-year campaign experiment](CAMPAIGN-SIMULATION.md) compares 512 runs
across four world histories and four player strategies. Persistent empty offers
were eliminated in the tested shops. Seasonal shortages and growing wealth remain
balance issues; the report includes the results and assumptions.

Save schema 4 is unchanged. Existing markets, merchants and cargo load without a
reset. Schemas 1–3 still migrate; pre-0.5.0 DLLs cannot load schema 4. New security
observations affect future ticks. There are no new engine hooks, ESPs or scripts.
The optional innkeeper dialogue addon is unchanged; both menus remain removed.

See [game testing](TESTING.md) for the expanded camp/fort checks and scarce-stock
test. Security can be disabled in `SKSE/Plugins/MarcoPolo/security.json`.
