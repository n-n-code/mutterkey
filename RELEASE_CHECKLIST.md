# Release Checklist

Use this checklist before publishing the repository or cutting a release.

## Repository Hygiene

- Confirm the working tree does not contain generated build artifacts.
- Run:

```bash
bash scripts/check-release-hygiene.sh
```

- Verify there are no machine-specific home-directory paths or absolute local
  Markdown links in tracked files.
- Confirm `third_party/whisper.cpp` is marked vendored in `.gitattributes`.

## Licensing And Provenance

- Confirm the root [LICENSE](LICENSE) file matches the intended project license.
- Review [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for accuracy.
- Review [third_party/whisper.cpp.UPSTREAM.md](third_party/whisper.cpp.UPSTREAM.md)
  and make sure the recorded upstream version/ref is current.
- Confirm no Whisper model binaries or other large third-party artifacts are
  tracked in the repository.

## Build And Test

- Configure a fresh out-of-tree build:

```bash
BUILD_DIR="$(mktemp -d /tmp/mutterkey-build-XXXXXX)"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
```

- Build:

```bash
cmake --build "$BUILD_DIR" -j"$(nproc)"
```

- Run tests:

```bash
ctest --test-dir "$BUILD_DIR" --output-on-failure
```

- Run repo-maintained analyzers when they are available:

```bash
cmake --build "$BUILD_DIR" --target clang-tidy
cmake --build "$BUILD_DIR" --target clazy
```

- Validate headless startup:

```bash
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" --help
```

- If the change affects startup, service wiring, or config handling, also run:

```bash
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" diagnose 1
```

## Install Validation

- Install into a temporary prefix:

```bash
INSTALL_DIR="$(mktemp -d /tmp/mutterkey-install-XXXXXX)"
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"
```

- Confirm the installed tree contains:
  - `bin/mutterkey`
  - required `libwhisper` / `ggml` shared libraries
  - the desktop file under `share/applications`
  - license files under `share/licenses/mutterkey`

## Documentation And User Flow

- Review [README.md](README.md) for consistency with current behavior.
- Confirm the documented recommended path is still the `systemd --user` service.
- Confirm [contrib/mutterkey.service](contrib/mutterkey.service) matches the
  recommended installed-binary setup.
- Confirm [contrib/org.mutterkey.mutterkey.desktop](contrib/org.mutterkey.mutterkey.desktop)
  still reflects the intended desktop behavior, including `NoDisplay=true`.

## Vendored whisper.cpp Updates

- If `third_party/whisper.cpp` changed, review the diff carefully and prefer
  app-side integration changes over vendored patches.
- If the vendored snapshot was updated, run:

```bash
bash scripts/update-whisper.sh <upstream-tag-or-commit>
```

- After a vendored update, refresh:
  - [third_party/whisper.cpp.UPSTREAM.md](third_party/whisper.cpp.UPSTREAM.md)
  - [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

## Final Sanity Pass

- Re-read the planned release notes or repository description.
- Confirm the current state is suitable for a clean public snapshot.
- If working from a plain directory snapshot rather than a git checkout, initialize git and review the first commit contents before pushing.
- If publishing to GitHub, verify the CI workflow file exists at
  `.github/workflows/ci.yml`.
