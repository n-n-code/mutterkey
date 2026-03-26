# AGENTS.md

## Project Overview

`Mutterkey` is a native `C++ + Qt 6` push-to-talk transcription tool for `KDE Plasma`.

Current architecture:

- Global shortcut handling goes through `KGlobalAccel`
- Audio capture uses Qt Multimedia
- Transcription is in-process through vendored `whisper.cpp`
- Clipboard writes prefer `KSystemClipboard` with `QClipboard` fallback
- There is an early Qt Widgets tray shell in `mutterkey-tray`, but the daemon remains the product core
- The recommended day-to-day runtime path is the `systemd --user` service
- The installed desktop entry is intentionally hidden from normal app menus with `NoDisplay=true`
- `daemon` is the default runtime mode; `once` and `diagnose` are validation helpers

This repository is intentionally kept minimal:

- CMake is the only supported build system
- `whisper.cpp` is the only supported transcription backend
- Keep the repo free of generated build output
- Keep publication-facing files free of machine-specific paths and broken local links
- Do not reintroduce legacy qmake or external-command transcription paths unless explicitly requested

## Repository Layout

- `src/main.cpp`: CLI entrypoint and mode selection
- `src/service.*`: daemon lifecycle and background transcription wiring
- `src/hotkeymanager.*`: KDE `KGlobalAccel` integration
- `src/audio/audiorecorder.*`: microphone capture
- `src/audio/recording.h`: shared recorded-audio payload passed between the recorder, service, tests, and transcription path
- `src/clipboardwriter.*`: clipboard integration, preferring KDE system clipboard support
- `src/audio/recordingnormalizer.*`: conversion to Whisper-ready mono `float32` at `16 kHz`
- `src/transcription/whispercpptranscriber.*`: in-process Whisper integration
- `src/transcription/transcriptionengine.*`: app-owned engine/session seam for backend selection and future runtime evolution; the engine owns immutable runtime metadata such as backend capabilities
- `src/transcription/transcriptionworker.*`: worker object hosted on a dedicated `QThread`
- `src/transcription/transcriptiontypes.h`: normalized audio, typed runtime error, and capability value types
- `src/config.*`: JSON config loading and defaults
- `src/app/*`: shared CLI/runtime command helpers used by the main entrypoint
- `src/control/*`: local daemon control transport, typed snapshots, and session/client APIs
- `src/tray/*`: Qt Widgets tray-shell UI scaffolding
- `contrib/mutterkey.service`: example user service
- `contrib/org.mutterkey.mutterkey.desktop`: hidden desktop entry used for desktop identity/integration
- `scripts/check-release-hygiene.sh`: repo hygiene checks for publication-facing content
- `next_feature/`: tracked upcoming feature plans as Markdown; keep only plan `.md` files and the folder-local `.gitignore`
- `docs/Doxyfile.in`: Doxygen config template for repo-owned API docs
- `docs/mainpage.md`: Doxygen landing page used instead of the full README
- `scripts/run-valgrind.sh`: deterministic Valgrind Memcheck runner for release-readiness checks
- `scripts/update-whisper.sh`: `git subtree` helper for updating vendored `whisper.cpp`
- `.github/workflows/ci.yml`: Linux configure/build/test and hygiene CI
- `.github/workflows/release-checks.yml`: release-scoped debug build, tests, and Valgrind Memcheck workflow
- `LICENSE`: root MIT license for repo-owned source
- `THIRD_PARTY_NOTICES.md`: third-party licensing/provenance notes
- `third_party/whisper.cpp.UPSTREAM.md`: recorded metadata for the current vendored `whisper.cpp` snapshot
- `third_party/whisper.cpp`: vendored dependency; avoid editing unless the task is specifically about the dependency

## Build Commands

Before answering build/setup questions or changing build behavior, read the `Overview`, `Quick Start`, `Run As Service`, and `Development` sections in `README.md`.

Use an out-of-tree build directory outside the repo root when possible so the working tree stays clean.

Prefer `-G Ninja` when it is available so local builds match CI more closely.
If `ninja-build` is not installed, omit `-G Ninja` and let CMake use the
system default generator.

Example:

```bash
BUILD_DIR="$(mktemp -d /tmp/mutterkey-build-XXXXXX)"
cmake -S . -B "$BUILD_DIR" -G Ninja
cmake --build "$BUILD_DIR" -j"$(nproc)"
```

If a sandboxed build fails with `ccache: error: Read-only file system`, treat
that as an environment limitation rather than a repo regression and rerun the
build with `CCACHE_DISABLE=1`.

If a sandboxed build still routes through vendored `ggml`'s own `ccache`
detection, also reconfigure with `GGML_CCACHE=OFF` before concluding the repo is
broken.

If the task affects install layout, licensing, or packaging, also validate a temporary install prefix:

```bash
INSTALL_DIR="$(mktemp -d /tmp/mutterkey-install-XXXXXX)"
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"
```

If you need a stable local build directory for iteration, use `build/`, but remove it before finishing unless the user explicitly asks to keep it.

## Validation Commands

Preferred lightweight validation after code changes:

```bash
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" --help
```

If the change affects startup, config, or wiring, also run:

```bash
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" diagnose 1
```

Notes:

- `once` mode requires microphone access and a valid Whisper model path
- Real transcription verification needs a configured model in `~/.config/mutterkey/config.json` or a custom config path
- A small `Qt Test` + `CTest` suite exists for config loading and audio normalization, including malformed JSON, wrong-type config inputs, and recording-normalizer edge cases
- Config loading is intentionally forgiving: invalid runtime values fall back to defaults and log warnings
- Use `ctest --test-dir "$BUILD_DIR" --output-on-failure` for changes that affect covered code
- Keep Qt GUI or Widgets tests headless under `CTest`: set `QT_QPA_PLATFORM=offscreen` in the test registration or test properties rather than relying on the caller environment
- Use `bash scripts/run-valgrind.sh "$BUILD_DIR"` or `cmake --build "$BUILD_DIR" --target valgrind` when validating memory behavior for release readiness or after fixing memory-lifetime issues
- On Debian-family systems, install `libc6-dbg` if Valgrind fails at startup with a `ld-linux` / mandatory redirection error
- Use `cmake --build "$BUILD_DIR" --target clang-tidy` after C++ changes when static-analysis noise is likely to matter
- Use `cmake --build "$BUILD_DIR" --target clazy` after Qt-facing changes when `clazy-standalone` is available
- Use `cmake --build "$BUILD_DIR" --target lint` when you want the repo's full static-analysis pass in one command
- Use `bash scripts/check-release-hygiene.sh` when touching publication-facing files such as `README.md`, licenses, `contrib/`, CI, or helper scripts
- Use `cmake --build "$BUILD_DIR" --target docs` when touching repo-owned public headers, Doxygen config, the Doxygen main page, or CI/docs wiring
- If install rules or licensing files change, confirm the temporary install contains the expected files under `share/licenses/mutterkey`
- If you add or change public methods in repo-owned headers, expect `cmake --build "$BUILD_DIR" --target docs` to fail until the new API is documented; treat that as part of the normal implementation loop, not follow-up polish

## Tooling Best Practices

- Treat `clang-tidy` and `clazy` as repo-maintained checks, not optional extras; if a change introduces new warnings in repo-owned code, fix them or explain why they are being deferred
- The repo-owned `clang-tidy` path is intentionally strict and now relies on `.clang-tidy` `RemovedArgs` support for `-mno-direct-extern-access` instead of rewriting `compile_commands.json`; prefer extending that config rather than adding more build-dir munging
- Prefer `cmake --build "$BUILD_DIR" --target clang-tidy` in a real configured build tree when validating analyzer changes, because Qt test sources depend on generated `*_autogen` / `.moc` outputs that ad hoc one-off `clang-tidy` calls may not have available
- Treat Valgrind Memcheck as the release memory gate and ASan/UBSan as the faster developer complements; they overlap, but they are not interchangeable
- Treat the release-hygiene script and GitHub CI workflow as repo-maintained checks too; keep them passing when build inputs, docs, or repository metadata change
- Treat the Doxygen `docs` target as a repo-maintained check too when touching repo-owned API docs or docs/CI wiring; repo-owned Doxygen warnings should be fixed rather than ignored
- Keep the default Valgrind lane deterministic and headless: prefer `configtest`, `recordingnormalizertest`, and `mutterkey --help` over microphone, clipboard, or KGlobalAccel-heavy paths unless the task is specifically about those integrations
- The release-hygiene script intentionally ignores generated build trees while scanning for machine-specific home-directory paths and absolute Markdown links, then reports generated-artifact roots separately; if it flags `./build`, remove the repo-local build directory rather than weakening the content checks
- Keep the Doxygen main page in `docs/mainpage.md` small and API-focused. The release-facing `README.md` may link to files outside the Doxygen input set and should not be used as the Doxygen main page unless the input set is expanded deliberately
- Keep analyzer fixes targeted to `src/` and `tests/`; do not churn `third_party/` or generated Qt autogen output to satisfy tooling
- Reconfigure the build directory after installing new tools so cached `find_program()` results are refreshed
- When validating inside a restricted sandbox, be ready to disable `ccache` with `CCACHE_DISABLE=1` if the cache location is read-only; that is an execution-environment issue, not a Mutterkey build failure
- Prefer fixing the code over weakening `.clang-tidy` or the Clazy check set; only relax tool config when the warning is clearly low-value for this repo
- If `clang-tidy` flags a new small enum for `performance-enum-size`, prefer an explicit narrow underlying type such as `std::uint8_t` instead of suppressing the warning
- In this Qt-heavy repo, treat `misc-include-cleaner` and `readability-redundant-access-specifiers` as low-value `clang-tidy` noise unless the underlying tool behavior improves; they conflict with Qt header-provider reality and `signals` / `slots` / `Q_SLOTS` sectioning more than they improve safety
- Prefer anonymous-namespace `Q_LOGGING_CATEGORY` for file-local logging categories; `Q_STATIC_LOGGING_CATEGORY` is not portable enough across the Qt versions this repo may build against
- Do not add broad Valgrind suppressions by default; only add narrow suppressions after reproducing stable third-party noise and keep them clearly scoped
- When adding tests, prefer small `Qt Test` cases that run headlessly under `CTest` and avoid microphone, clipboard, or KDE session dependencies unless the task is specifically integration-focused
- For tool-driven cleanups, preserve the existing design and behavior; do not perform broad rewrites just to satisfy style-oriented recommendations
- Keep forward-looking feature plans under `next_feature/` as tracked Markdown files; do not leave scratch notes, binaries, or generated artifacts there

## Coding Guidelines

- Stay within the existing style and structure; do not reformat unrelated code
- Prefer small, direct classes over adding abstraction layers without a concrete need
- Keep Qt usage idiomatic: `QObject` ownership, signal/slot wiring, and `QThread` boundaries should remain explicit
- Prefer anonymous-namespace `Q_LOGGING_CATEGORY` for translation-unit-local logging categories when no cross-file declaration is needed; keep the pattern compatible with older Qt builds used in CI
- When refactoring Qt class declarations, remember that `moc` still cares about section structure: keep explicit `signals`, `slots`, `Q_SLOTS`, and access sections valid for Qt even if a generic style check suggests flattening them
- Prefer explicit validation and safe fallback behavior for config-driven runtime values
- Avoid introducing optional backends, plugin systems, or cross-platform abstractions unless the task requires them
- Keep the audio path explicit: recorder output may not already match Whisper input requirements, so preserve normalization behavior
- Prefer narrow shared value types across subsystems; for example, consumers that only need captured audio should include `src/audio/recording.h`, not the full recorder class
- Keep JSON and other transport details at subsystem boundaries; prefer typed C++ snapshots/results once data crosses into app-owned control, tray, or service code
- Prefer dependency injection for tray-shell and control-surface code from the first implementation so headless Qt tests stay simple
- When preparing the transcription path for future runtime work, prefer app-owned engine/session seams and injected sessions over leaking concrete backend types into CLI, service, or worker orchestration. Keep immutable capability reporting and backend metadata on the engine side, and keep the session side focused on mutable decode state, warmup, and transcription
- Prefer product-owned runtime interfaces, model/session separation, and deterministic backend selection before adding new inference backends or widening cross-platform support
- Keep backend-specific validation out of `src/config.*` when practical. Product config parsing should normalize and preserve user input, while backend support checks should live in the app-owned runtime layer near `src/transcription/*`
- Preserve the current product direction: embedded `whisper.cpp`, KDE-first, CLI/service-first

## C++ Core Guidelines Priorities

Apply the C++ Core Guidelines selectively and pragmatically. For this repo, the highest-value rules are ownership, lifetime, invariants, initialization, and type-safe interfaces.

- Prefer code that expresses intent directly and keeps interfaces simple; hide low-level Qt, audio, and `whisper.cpp` details behind small, explicit types and helper functions
- Favor static type safety and compile-time checking over comments or conventions: use `const`, `override`, `explicit`, `enum class`, `[[nodiscard]]`, and narrow types when they clarify behavior
- Treat raw pointers and references as non-owning observers; never transfer ownership through `T*` or `T&`. Use `std::unique_ptr` or another explicit owner only when ownership really must move
- For ordinary function parameters, prefer `T*` or `T&` over smart pointers unless the callee is participating in ownership or lifetime management
- Use RAII consistently for every owned resource and cleanup path: memory, worker-thread lifetime, temporary state transitions, and any future locks or file handles. Avoid naked `new`/`delete` and manual `lock()`/`unlock()`
- Prefer the rule of zero. If a type must manage a resource directly and cannot rely on member types with correct behavior, define or `=delete` the full copy/move/destructor set deliberately
- Always initialize objects and members. Prefer constructor member initializers, default member initializers, and creating objects in the narrowest useful scope
- Preserve clear class invariants and valid states across startup, shutdown, and error paths; after a failed operation, objects should remain usable or fail in an obvious, contained way
- Prefer immutable data after construction where practical, especially for config snapshots, recordings passed across threads, and transcription results
- Prefer standard library and Qt container/value types over raw arrays, manual memory management, and ad hoc bounds handling. Keep indexing and buffer conversions explicit and bounds-safe
- When touching older code, move it toward these rules incrementally; do not force broad rewrites unless the task is specifically a modernization pass

## Dependency Guidance

- Treat `third_party/whisper.cpp` as vendored source
- The current vendored snapshot and provenance notes are tracked in `third_party/whisper.cpp.UPSTREAM.md`
- Prefer `bash scripts/update-whisper.sh <upstream-tag-or-commit>` for future `whisper.cpp` updates from a real Git work tree
- `scripts/update-whisper.sh` requires a clean Git work tree before it will fetch or run subtree operations
- Treat `third_party/whisper.cpp` as subtree-managed vendor content and update it through the helper script rather than manual directory replacement
- Prefer changing app-side integration code before patching vendored dependency code
- Prefer fixing vendored target metadata from the top-level CMake when the issue is Mutterkey packaging or warning noise, instead of patching upstream vendored files directly
- If you must modify vendored code, document why in the final response and record the deviation in `third_party/whisper.cpp.UPSTREAM.md`
- Do not restore the removed upstream examples/tests/scripts unless the task requires them

## Release And Licensing

- Repo-owned source is MIT-licensed in `LICENSE`
- Third-party licensing and provenance notes live in `THIRD_PARTY_NOTICES.md`
- `whisper.cpp` model files are not bundled; do not add model binaries to the repository
- Do not introduce machine-specific home-directory paths, absolute local Markdown links, or generated build artifacts into tracked files
- If a task changes install layout or shipped assets, keep the CMake install rules and license installs aligned with the new behavior
- The installed shared-library payload is runtime-focused; do not start installing vendored upstream public headers unless the package contract intentionally changes

## Config Expectations

Important runtime config fields live under `transcriber`:

- `model_path`
- `language`
- `translate`
- `threads`
- `warmup_on_start`

Default config path:

```text
~/.config/mutterkey/config.json
```

Typical model location:

```text
~/.local/share/mutterkey/models/ggml-base.en.bin
```

## Agent Workflow

- Read `README.md` first, especially `Overview`, `Quick Start`, `Run As Service`, and `Development`, then read the touched source files before editing
- Prefer targeted changes over speculative cleanup
- If a change grows daemon, tray, or control-plane behavior, prefer extracting or extending repo-owned libraries under `src/app/`, `src/control/`, or other focused modules instead of piling more orchestration into `src/main.cpp`
- Update `README.md` and `config.example.json` when behavior or setup changes
- Update `contrib/mutterkey.service` and `contrib/org.mutterkey.mutterkey.desktop` when service/desktop behavior changes
- Update `LICENSE`, `THIRD_PARTY_NOTICES.md`, CMake install rules, and `third_party/whisper.cpp.UPSTREAM.md` when packaging, licensing, or vendored dependency behavior changes
- Keep `README.md`, `AGENTS.md`, and any relevant local skills aligned with the current `scripts/update-whisper.sh` workflow when the vendor-update process changes
- Store upcoming feature plans in `next_feature/` as Markdown files, and update the existing plan there when refining the same upcoming feature instead of scattering notes across the repo
- Keep architecture-evolution plans grounded in incremental slices that preserve the current shipped `whisper.cpp` path while moving ownership of interfaces, tests, and packaging toward repo-owned code
- Treat `mutterkey-tray` as a shipped artifact once it is installed or validated in CI; keep install rules, README/setup notes, release checklist items, and workflow checks aligned with that status
- Verify with a fresh CMake build when the change affects compilation or linkage
- Run `ctest` when touching covered code in `src/config.*` or `src/audio/recordingnormalizer.*`, and extend the deterministic headless tests when practical
- When touching transcription orchestration or backend seams, prefer small headless tests with fake/injected sessions or fake engines over model-dependent integration tests. Engine injection is the preferred seam for orchestration tests; direct session injection is still useful for narrow worker behavior
- When adding or fixing Qt GUI tests, make the `CTest` registration itself headless with `QT_QPA_PLATFORM=offscreen` so CI does not try to load `xcb`
- Prefer expanding tests around pure parsing, value normalization, and other environment-independent logic before adding KDE-session or device-heavy coverage
- Use `-DMUTTERKEY_ENABLE_ASAN=ON` and `-DMUTTERKEY_ENABLE_UBSAN=ON` for fast iteration on memory and UB bugs, and use the repo-owned Valgrind lane as the slower release-focused confirmation step
- Run `clang-tidy` and `clazy` targets for non-trivial C++/Qt changes when the tools are installed in the environment
- If `clang-tidy` validation needs Qt test `.moc` files, generate the relevant `*_autogen` targets first or just run the repo-owned `clang-tidy` target from a configured build tree
- Run the `docs` target when touching repo-owned public headers, Doxygen comments, `docs/Doxyfile.in`, `docs/mainpage.md`, or CI/docs wiring
- Prefer the `lint` target for a full pre-handoff analyzer pass, and use the individual analyzer targets when iterating on one class of warnings
- Run `bash scripts/run-valgrind.sh "$BUILD_DIR"` before handoff when the task is specifically about memory, ownership, lifetime, shutdown, or release hardening
- Run `bash scripts/check-release-hygiene.sh` before handoff when the task touches publication-facing files or repository metadata
- If `QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" diagnose 1` fails in a headless environment after model loading or during KDE/session-dependent startup, note that limitation explicitly rather than assuming the runtime seam or docs-only change regressed behavior
- Do not leave generated artifacts in the repository tree at the end of the task
- Do not assume every workspace copy is an initialized git repository; if `git` commands fail, continue with file-based validation and mention the limitation in the final response

## When Unsure

- Optimize for the current implementation, not hypothetical future backends
- Ask before making architectural expansions beyond KDE, Qt, and embedded Whisper
- Keep the repo in a state that would be reasonable for a clean initial commit
