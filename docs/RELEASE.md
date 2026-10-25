# Skyrim True Economy and Merchantry 0.3.2

Moves the ledger movie directly into `Interface` for Skyrim's native menu
loader. If loading still fails, the log now distinguishes a missing game
resource from a Scaleform initialization failure. In-game verification is pending.

**Right Shift + =** opens a compact Trade Ledger with Markets, Routes and Rumors tabs. Drag
the title to move it, drag the bottom-right corner to resize it, and use the
padlock to keep it in place. Position, size and bookmarks persist between sessions.

The ledger uses Skyrim's active menu fonts, including font replacements. It
requires neither SKSE Menu Framework nor Prisma. Prices use descriptions such
as **Cheap**, **Fair** and **Expensive**; uncertain rumors stay **Uncertain**.
Report ages remain visible, with sources and dates available on hover. Routes
compare remembered reports and can be bookmarked.

SKSE Menu Framework remains an optional alternative tracker. Its normal view
also uses price descriptions; the debug page retains exact percentages. The
section now uses the public mod name.

Install the core archive and restart Skyrim. The existing Trade Rumors addon
is unchanged; its [known voice issues](RUMOR-VOICES.md) remain. No new game is
required. Economy simulation, barter, plugin forms and co-save formats are
unchanged. Window preferences live in a separate `SKSE/Plugins/MarcoPolo/ledger.json`.

Automated checks cover qualitative price ranges, report ages, information
isolation, preference validation, simulation and persistence. The menu movie is
rebuilt and verified in CI. Native font rendering, focus restoration and menu
interaction still need in-game confirmation across supported runtimes.
