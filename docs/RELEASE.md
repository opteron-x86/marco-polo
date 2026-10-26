# Skyrim True Economy and Merchantry 0.3.4

Dragging the ledger's corner now changes its width and height without enlarging
the text. Columns expand with the window, and taller windows display more rows.
Both dimensions are remembered between sessions.

Escape dismisses the hold selector or closes the ledger. Tab and Shift + Tab
change tabs. Keyboard navigation uses physical keys so Skyrim's menu-key
translation cannot confuse Escape with Tab. Tooltips fit their text and wrap
long reports at a maximum width.

The hold selector blocks tooltips from the goods beneath it, including the gaps
between buttons. Changing views also clears any open tooltip.

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
