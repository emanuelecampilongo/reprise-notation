# Reprise Notation for Windows

Reprise Notation is developed by [Emanuele Campilongo](https://emanuelecampilongo.com/) and is based on MuseScore Studio. See the accompanying LICENSE.txt for the program license and the upstream copyright notice.

Development source is available in the [Reprise Notation repository](https://github.com/emanuelecampilongo/reprise-notation). For a public binary, provide a link alongside its download to the exact corresponding source and build instructions for that version, including required third-party source and notices.

Visit the [Reprise Notation website](https://emanuelecampilongo.com/software/reprise-notation) for product information. Send problem reports to [info@emanuelecampilongo.com](mailto:info@emanuelecampilongo.com).

## Preparing a Store package

1. Build and stage the intended x64 Windows release with CMake. Run the matching Qt SDK's `windeployqt --release --no-translations --no-compiler-runtime --qmldir <source>/src "<stage>/bin/Reprise Notation.exe"`. Test the staged executable on Windows without developer environment variables or a Qt SDK on the machine.
2. Run `pack-reprise-msix.ps1` against the staging directory. The default identity matches the reserved Partner Center app (Store ID `9P6V1L676P93`). Review `AppxManifest.xml` for the package version, desktop shortcut and package settings. Use `-PackageVersion` to override the version for a later build, with a final component of zero. For Store distribution, pass the corresponding public Git commit with `-SourceRevision` and the Qt license and attribution files with `-QtNoticesDirectory`.
3. For local signed testing, supply the thumbprint of an existing code-signing certificate in the current user's store. Its subject must match the package publisher. Test installation and coexistence with MuseScore in a clean Windows environment.
4. Attach the corresponding source and applicable notices to the release, then validate the package and Store listing in Partner Center. Verify the desktop shortcut and update flow by installing one Store build and offering a newer one with the same package identity.
