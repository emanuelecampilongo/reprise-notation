# Reprise Notation — versioning and release policy

## Public version

- Current project version: **0.1.3** (without alpha/beta suffix).
- Source of truth: `REPRISE_APP_VERSION` in root `version.cmake`.
- It is exported to C++ by `src/framework/cmake/muse_framework_config.h.in`.
- The splash screen and the Reprise About dialog display this public version.
- The exact Git commit identifies each private build. A private build is **not** a public release or a guarantee of stability.

## MuseScore compatibility layer

The upstream compatibility version comes from `MUSE_APP_VERSION` (currently 4.7.5).
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

Reprise Notation 0.1.3 uses MSIX version 1.1.1.0. Keep the Store package version increasing and leave its fourth component at zero.
