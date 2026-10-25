# Marco Polo 0.2.2 — Voiced innkeeper replies

Innkeepers now give a short vanilla voiced reply before opening the trade-rumor
popup. This replaces the unvoiced placeholder and moves the popup from the
response's Begin fragment to its End fragment.

Replies match the speaker's voice type: MaleNord and MaleYoungEager use
**Let's see here...**, MaleOldKindly offers to share what he knows, FemaleNord
and FemaleArgonian use **Sure.**, and Delphine uses **Certainly.** Other voices
use **Of course.** Custom voices without the matching recording retain the
shared subtitle. The addon references the game's own responses and lip-sync;
no voice assets or source quest actions are copied.

The existing quest, topic, availability global and default INFO retain their
form IDs. Six new INFOs provide the voice-specific choices. Economy and market
information save formats are unchanged. Debug dialogue logs now report the
matching INFO and shared response across all seven choices.

Install the matching core and `-rumors.zip`, keep **MarcoPolo - Trade Rumors.esp**
enabled, then fully restart Skyrim and load the current save. Test Elda's default
reply and Hulda's or Keerava's specific reply, including whether the popup waits
until the speech ends. Existing 0.2.x saves require no quest reset or new game.

Record and script checks verify exclusive voice selection, availability gates,
shared responses, stable existing IDs and End-only callbacks. The previous
0.2.1 topic and popup were confirmed in game; the new voice playback and timing
still need in-game confirmation. Targets desktop SE 1.5.97 and AE 1.6.x / 1.7.x
through alandtse CommonLibSSE-NG, with no new native hooks or runtime restrictions.
