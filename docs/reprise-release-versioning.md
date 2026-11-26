# Reprise Notation — versioning and release policy

## Public version

- Current project version: **0.1.4** (without alpha/beta suffix).
- Source of truth: `REPRISE_APP_VERSION` in root `version.cmake`.
- It is exported to C++ by `src/framework/cmake/muse_framework_config.h.in`.
- The splash screen and the Reprise About dialog display this public version.
- The exact Git commit identifies each private build. A private build is **not** a public release or a guarantee of stability.

## MuseScore compatibility layer

The upstream compatibility version comes from `MUSE_APP_VERSION`. The MuseScore Studio baseline is [v4.7.5](https://github.com/musescore/MuseScore/tree/v4.7.5), commit `3654226c2e99289916916953a98e585a3d3b315a`.

For the next upstream update, start an integration branch from Reprise `main` and compare the old and new MuseScore tags. Apply that upstream delta against Reprise, review overlaps with Reprise changes, then run the Windows build and regression tests. Record the new upstream tag and commit after validation. Use the tag comparison as the starting point because the public Reprise history begins with a source snapshot.
Windows builds produce `Reprise Notation.exe`. The MSIX manifest and local signing script
use the same filename.

Windows preferences and application data use the `Reprise Notation/RepriseNotation`
profile under Local AppData. The default documents folder is `RepriseNotation`
under Documents.

## When to release 1.0.0

A 0.x release is a normal numbered release, not necessarily an alpha.
Use 1.0.0 for the first public release if we have validated:
- Startup and docking layout at first launch and after resize.
- Stability of all Reprise tools, undo/redo and playback.
- Music file compatibility and regression checks.
- Reprise branding and translations.
- Licences, attributions and source distribution.
- Installer/MSIX, signing and clean-machine installation.
- Update behavior and no conflict with upstream installation.

Versioning follows Semantic Versioning as a policy, insofar as the
public product contract is documented:
- MAJOR: incompatible public behavior or format guarantees.
- MINOR: backward-compatible functionality.
- PATCH: backward-compatible fixes.
- Build/commit identifier is separate.

Reference: https://semver.org/lang/it/

## Artwork

The approved **Orbita** splash SVG is kept verbatim in
`src/appshell/resources/LoadingScreen.svg`, size 800 × 380.
Only a small runtime version label is drawn by C++ in the bottom-right
corner. The obsolete upstream website label has been removed from the
splash, while upstream legal attributions remain in About and source.

No new Home image and no welcome-screen artwork are required.

## Microsoft Store package version

Reprise Notation 0.1.4 sets MSIX version `1.1.4.0` in the source manifest. The packaging script can override that value. Use the package in Partner Center to confirm the version actually distributed. Keep the Store package version increasing and leave its fourth component at zero.

For each Store submission, record the Reprise version, the actual MSIX version, the package SHA-256 before submission, and the exact public source commit. Verify that the Store listing gives users a clear route to the corresponding source and applicable notices. Keep the record with the release materials.
