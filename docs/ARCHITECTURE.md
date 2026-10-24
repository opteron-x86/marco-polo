# Architecture

`src/core` is portable C++ with no game dependency. `Economy` owns the complete save-specific state; JSON encoding is versioned and validated before replacing live state. Fixed daily ticks and a serialized xorshift state make catch-up independent of batch size. Terminal contract history is bounded. Unsupported schemas fail rather than reset a running economy.

Schema 2 adds household goods. Loading schema 1 initializes the new commodity at the saved day when it is absent from every market and present in the current definitions. Established goods, prices, merchants, shipments, contracts, time and RNG state are preserved. Existing household stock in custom configurations is retained. Missing established goods, partially missing household stock and invalid values still reject the load without replacing live state. New saves cannot be loaded by pre-0.1.4 DLLs.

```mermaid
flowchart LR
  Config[JSON definitions] --> Core[Economy model]
  Time[Game calendar and Civil War] --> Core
  Core <--> Save[SKSE co-save]
  Trade[Confirmed barter transaction] --> Core
  Core --> Price[SkyUI list, card and quantity quotes]
  Core <--> Chest[Observed merchant inventories]
  Core --> Boards[Missives reports and supply orders]
  Core --> Ledger[Menu Framework ledger]
```

Markets contain warehouses; merchant stock is separate. Both contribute to local availability. Production responds to storage, demand responds to price, food can spoil, and recipes consume inputs. Routes reserve stock when departing and deliver later, with losses decided and saved on departure. Taxes and NPC purchasing are explicit outside money sources; merchants spend cash to restock.

Each merchant item separates managed chest quantity from `externalQuantity`, observed in Skyrim's sale inventory through `ExtraOriginalReference` source handles. Shop displays and other sale sources contribute to availability but are never subject to simulated turnover or chest replenishment. The adapter validates the selected sale row's count, lets the engine choose the source, and accounts for the confirmed chest and external portions of the transfer separately. It preserves external counts between transactions because Skyrim's temporary sale inventory is being rebuilt. Opening the next barter session observes the external sources again. Older schema-1 saves default this new count to zero until observation.

The price function integrates stock change at unit midpoints, rounds each unit and sums the result. Symmetric trades retrace the curve before the merchant spread. `Config::BarterMultiplier` compresses the live game multipliers using a configurable exponent and a minimum margin on both sides. Speech, perks and merchant modifiers still affect offers. One `Price` method supplies all quotes. A confirmed `Trade` updates item counts, merchant cash and regional source/sink totals. There is no quote-time inventory mutation.

Regional `priceFactors` represent structural production costs and import dependence. Daily scarcity multiplies these factors; global bounds and adjustment limits still apply. New games seed their prices from the factors. Existing saved prices restore exactly and converge through ordinary ticks; the save schema is unchanged. Background customer receipts and warehouse purchases share the regional value scale, so import goods do not inherently drain merchant cash. Simulated cash stays within the ledger limit.

`src/runtime` adapts the model to Skyrim. Only game-thread tasks use game objects. A small timer requests work; it does not touch engine state. UI rendering reads a copied snapshot. A generation counter invalidates queued work after changing saves. Runtime form keys store the originating plugin and local ID, so load-order changes do not change identity; dynamic forms are excluded.

The native integration targets desktop SE 1.5.97 and AE 1.6.x / 1.7.x using pinned alandtse CommonLib definitions. Both legacy Query and modern Version exports are provided, without a version whitelist. Inventory resets select SE ID 19375 or AE ID 19802; other hooks use CommonLib vtable IDs or paired relocations. `BarterMenu::Accept` redirects the `ItemSelect` callback. SkyUI method wrappers and a list data processor provide native quotes. Stock/gold are verified around the original callback; fractional unit-price rounding is reconciled only after observing the complete transfer. A detour on `TESObjectREFR::ResetInventory` protects registered merchant chests from vanilla replenishment. All these boundaries need in-game validation.

Missives uses only owned BOOK records and existing board references. Each hold has a non-takeable bulletin, a non-takeable order posting and a separate carried receipt. The original contract IDs remain receipts to preserve old saves. ContainerMenu callback and SkyUI method wrappers turn selecting a posting into reading without transferring it. Closing an order posting presents native buttons; carried receipts never offer actions. Queued reads and modal actions capture the save generation and contract ID, then revalidate the issuing board and current status before acting.

A description detour filters by the owned forms before touching economy state. A paired-ID BookMenu detour renders receipt terms using an order number stored in the item's ordinary ExtraTextDisplayData name. This prevents papers in other containers from showing a later order. The core co-save remains schema 2. Deferred reconciliation removes obsolete/duplicate carried papers, keeps one receipt per accepted order, and maintains one copy of each live posting per board. Missing receipts are issued on acceptance, load or reading the accepted posting at its board, rather than on every cleanup. Reconciliation waits until BookMenu closes before freeing inventory extra data. Other mods' forms are untouched.

Contracts persist exact requested forms and names with fixed terms, reserve funds when posted, and settle only once. The Missives adapter is the only runtime source of new offers and contract actions; the Menu Framework ledger displays market information only. No additional quest engine or Papyrus loop is involved.

First-release limits: no per-mine simulation, service-price adapters, NPC household budgets, physical caravans, custom-board discovery, VR support. Commodity classification is broad and configurable; production and demand coefficients are starting balance choices. Plugin interoperability and economic balance are part of the game-test phase.
