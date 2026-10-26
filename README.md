# Skyrim True Economy and Merchantry

A Skyrim economy mod with changing regional prices, merchant inventories, and profitable trade routes. Buy goods where they are cheap and sell them where they are worth more. Production, consumption, trade, and your own purchases affect local markets.

Ask innkeepers for market news through the optional Trade Rumors addon. News takes time to travel, so conditions may have changed by the time you arrive.

## Requirements

- Skyrim SE 1.5.97 or AE 1.6.x / 1.7.x
- SKSE and Address Library matching your game version
- SkyUI

## Installation

Install the core archive with Mod Organizer 2 or Vortex.

For innkeeper conversations, install the matching **Trade Rumors** addon archive and enable `MarcoPolo - Trade Rumors.esp`. Ask **Any news of trade?** to hear the latest reports. Some voiced replies have [known issues](docs/RUMOR-VOICES.md).

Settings are in `SKSE/Plugins/MarcoPolo/`. Restart Skyrim after changing them.

## Documentation

- [Release notes](docs/RELEASE.md)
- [Game testing](docs/TESTING.md)
- [Route security test results](docs/SECURITY-SIMULATION.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Build and release automation](docs/RELEASING.md)

## License

GPL-3.0 with [additional permissions](EXCEPTIONS.md). See [third-party notices](THIRD_PARTY_NOTICES.md).
