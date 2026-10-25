# Trade-rumor responses

## Runtime findings, 25 October 2026

The generic **Of course.** response and rumor callback worked for Elda,
Corpulus, Kleppr and Orgnar in the current test. Keerava and Delphine selected
their intended addon INFO, but its `dataInfo` pointer was null and no rumor
callback followed. Their voice-specific response links need correction.
The other specific variants still need an in-game check; valid plugin records
and matching FUZ files alone did not establish that these links work.

## Intended response selection

The dialogue generator references these Skyrim.esm records. The English strings,
voice types and FUZ recordings were inspected in the installed vanilla master
and voice archive. No recordings are redistributed. DNAM shares response data
only; source INFO scripts, conditions, dialogue links and quest actions are not
copied. Every chosen source contains one response.

| Voice type | VTYP | Source INFO | Reply | Example innkeeper |
|---|---|---|---|---|
| MaleNord | 00013AE6 | 001041A4 | Let's see here.... | Wilhelm, Hadring |
| MaleYoungEager | 00013AD1 | 0003DF63 | Let's see here... | Mikael, if running the inn |
| MaleOldKindly | 00013AD6 | 000E3A37 | I can certainly share a bit of what I know. | Nils, if running the inn |
| FemaleNord | 00013AE7 | 000D8DAD | Sure. | Hulda |
| FemaleArgonian | 00013AEF | 000C34A5 | Sure. | Keerava |
| FemaleUniqueDelphine | 00014432 | 00098E53 | Certainly. | Delphine |
| Any other voice | — | 000DBA22 | Of course. | Elda, Kleppr, Corpulus, Jonna |

The six specific replies each have a recording for their selected voice. The
default response has 42 vanilla voice recordings. It covers all 17 non-unique
voice types found on base-game innkeeper/room-rental faction actors, including
potential replacements. Delphine's unique voice is handled separately. Runtime
faction membership still determines whether the topic is available.

Voice selection uses condition function 426, GetIsVoiceType. Each specific INFO
requires its voice; the default requires that all six comparisons are false.
This avoids depending on INFO ordering to select a single reply. The explicit
PNAM chain preserves order in editors. Original addon forms 800–804 remain;
six new INFOs use 805–80A.

The INFO VMAD sets the End-fragment flag (2), not Begin (1), and calls the existing
Fragment_0 entry point. PEX files are unchanged. Goodbye and Force Subtitle are
set; No LIP is cleared so shared lip-sync remains available. Custom voices with
no matching vanilla recording retain the default localized subtitle; actual
fallback timing needs an in-game check. Mod overrides to the source responses
also affect the shared dialogue.
