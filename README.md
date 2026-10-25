# Skyrim True Economy and Merchantry

A Skyrim economy mod with changing regional prices, merchant inventories, and profitable trade routes. Buy goods where they are cheap and sell them where they are worth more. Production, consumption, trade, and your own purchases affect local markets.

Market reports remember the conditions you last encountered. Travel to refresh them, or ask innkeepers for news through the optional Trade Rumors addon.

## Requirements

- Skyrim SE 1.5.97 or AE 1.6.x / 1.7.x
- SKSE and Address Library matching your game version
- SkyUI

## Installation

Install the core archive with Mod Organizer 2 or Vortex.

Optional additions:

- **SKSE Menu Framework 3:** view remembered markets and possible routes under **Marco Polo → Trade Status**.
- **Trade Rumors addon:** install the matching addon archive and enable `MarcoPolo - Trade Rumors.esp` to ask innkeepers **Any news of trade?** Some voiced replies have [known issues](docs/RUMOR-VOICES.md).

Settings are in `SKSE/Plugins/MarcoPolo/`. Restart Skyrim after changing them.

## Documentation

- [Release notes](docs/RELEASE.md)
- [Game testing](docs/TESTING.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Build and release automation](docs/RELEASING.md)

## License

GPL-3.0 with [additional permissions](EXCEPTIONS.md). See [third-party notices](THIRD_PARTY_NOTICES.md).
