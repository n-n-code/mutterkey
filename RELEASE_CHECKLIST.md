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
```

- If the release is intended to ship an accelerated Whisper backend, configure
  the build with the relevant Mutterkey options:

```bash
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMUTTERKEY_ENABLE_WHISPER_CUDA=ON
```

```bash
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMUTTERKEY_ENABLE_WHISPER_VULKAN=ON
```

```bash
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMUTTERKEY_ENABLE_WHISPER_BLAS=ON -DMUTTERKEY_WHISPER_BLAS_VENDOR=OpenBLAS
```

- Acceleration option notes:
  - `MUTTERKEY_ENABLE_WHISPER_CUDA=ON`: NVIDIA GPU build through vendored `ggml`
  - `MUTTERKEY_ENABLE_WHISPER_VULKAN=ON`: Vulkan GPU build through vendored `ggml`
  - `MUTTERKEY_ENABLE_WHISPER_BLAS=ON`: faster CPU inference, not GPU execution
  - choose the backend intentionally for the release artifact and record that choice in release notes or packaging docs when relevant

- Build:

```bash
cmake --build "$BUILD_DIR" -j"$(nproc)"
```

- Run tests:

```bash
ctest --test-dir "$BUILD_DIR" --output-on-failure
```

- Run the release-memory diagnostics lane:

```bash
bash scripts/run-valgrind.sh "$BUILD_DIR"
```

- Run repo-maintained analyzers when they are available:

```bash
cmake --build "$BUILD_DIR" --target clang-tidy
cmake --build "$BUILD_DIR" --target clazy
```

- If the release changes repo-owned public headers, Doxygen config, or docs/CI
  wiring, also run:

```bash
cmake --build "$BUILD_DIR" --target docs
```

- Treat repo-owned Doxygen warnings as release blockers. Keep the Doxygen main
  page in `docs/mainpage.md` API-focused instead of pointing Doxygen at the
  full `README.md` unless the Doxygen input set is expanded deliberately.

- Validate headless startup:

```bash
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" --help
```

- Validate tray-shell startup in a headless environment:

```bash
timeout 2s env QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey-tray"
```

- Treat exit code `124` from the tray-shell smoke check as expected when the
  process stays alive until `timeout` stops it.

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
  - `bin/mutterkey-tray`
  - required `libwhisper` / `ggml` shared libraries
  - the desktop file under `share/applications`
  - license files under `share/licenses/mutterkey`
- If acceleration was enabled for the release, also confirm the installed tree
  contains the expected backend library:
  - `libggml-cuda.so*` for `MUTTERKEY_ENABLE_WHISPER_CUDA=ON`
  - `libggml-vulkan.so*` for `MUTTERKEY_ENABLE_WHISPER_VULKAN=ON`
  - `libggml-blas.so*` for `MUTTERKEY_ENABLE_WHISPER_BLAS=ON`
- Do not expect vendored upstream public headers to be installed; Mutterkey's
  install rules ship the runtime libraries but intentionally clear vendored
  `PUBLIC_HEADER` metadata to avoid upstream header-install warnings.

## Documentation And User Flow

- Review [README.md](README.md) for consistency with current behavior.
- Review `docs/mainpage.md` and `docs/Doxyfile.in` if the release touched
  repo-owned API docs or docs/CI wiring.
- Confirm the documented recommended path is still the `systemd --user` service.
- Confirm [contrib/mutterkey.service](contrib/mutterkey.service) matches the
  recommended installed-binary setup.
- Confirm [contrib/org.mutterkey.mutterkey.desktop](contrib/org.mutterkey.mutterkey.desktop)
  still reflects the intended desktop behavior, including `NoDisplay=true`.
- If the release is intended to use accelerated Whisper inference, verify the
  runtime logs on a representative machine show the expected backend instead of
  CPU-only fallback. For example:
  - CUDA/Vulkan releases should not log only `registered backend CPU`
  - CPU-accelerated BLAS releases may still be CPU-only, but should be tested
    against a representative Whisper model and expected performance target

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
