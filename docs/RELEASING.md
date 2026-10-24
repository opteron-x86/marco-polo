# Builds and publishing

Every push/PR runs portable sanitizer tests. Windows builds start only after those pass, use pinned vcpkg and alandtse CommonLib, run the same core tests on Windows, audit the PE metadata, then assemble archives. Artifacts contain:

- Main mod, optional Missives addon, PDB symbols and source ZIPs.
- `build-info.json` with source commit, dirty state, runtime target and DLL SHA-256.
- Release notes and `SHA256SUMS.txt`.

Version tags must match `CMakeLists.txt`. `v0.1.0-alpha.1` creates a GitHub prerelease. Standard `v0.1.0` creates a normal release. Only the tested build's artifacts are published. Rerunning a completed release validates existing assets rather than replacing a public release with a different build.

The initial build is an in-game test candidate, not a verified gameplay release. Update the release notes and `in_game_validation` metadata when actual game results justify it. Do not describe automated simulation tests as game tests.

Nexus uploads use the official [Nexus Mods upload action](https://github.com/Nexus-Mods/upload-action), following the sibling projects' workflow. Configure the repository's `nexus` environment:

| Setting | Kind | Purpose |
|---|---|---|
| `NEXUSMODS_API_KEY` | Secret | Upload credential. |
| `NEXUSMODS_MOD_ID` | Variable | Existing Skyrim SE mod page. |
| `NEXUSMODS_FILE_ID` | Variable | Existing core file entry to version. |
| `NEXUSMODS_MISSIVES_FILE_ID` | Optional variable | Addon file entry to version. |

Run **Upload tested release to Nexus** from `master`, selecting a published standard tag. It verifies the tag's commit, successful tagged CI run, complete asset checksums and exact DLL. Prereleases are rejected. It uploads those existing archives, updates the mod version/changelog and archives the prior file versions. It does not create a Nexus page or discover/upload credentials. If the optional addon file ID is absent, that upload is skipped.
