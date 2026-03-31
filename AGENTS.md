# AGENTS.md

## Overview

`Mutterkey` is a native `C++ + Qt 6` push-to-talk transcription tool for `KDE Plasma`.

Current product shape:

- Global shortcut handling uses `KGlobalAccel`
- Audio capture uses Qt Multimedia
- Clipboard writes prefer `KSystemClipboard` with `QClipboard` fallback
- The public runtime seam is app-owned and streaming-first:
  `TranscriptionEngine`, `TranscriptionSession`, `AudioChunk`,
  `TranscriptEvent`, `TranscriptUpdate`, `BackendCapabilities`,
  `RuntimeDiagnostics`, and `RuntimeError`
- Native Mutterkey model packages are the canonical artifact
- Raw whisper.cpp-compatible `.bin` files remain only as migration/import input
- A native CPU runtime path now exists with decoder-oriented execution metadata
  (`mkasr-v2`), app-owned session/search/tokenizer/timestamp helper seams, and
  explicit legacy native fixture compatibility (`mkasr-v1`)
- The native CPU path now has a real tensor-backed decoder (`RealDecoderV3`)
  with product-owned mel spectrogram, encoder, decoder with KV cache, and
  greedy search; end-to-end validation with real model weights is pending;
  `whisper.cpp` remains the only validated user-facing speech decoder today
- Vendored `whisper.cpp` is optional at build time through
  `MUTTERKEY_ENABLE_LEGACY_WHISPER=OFF`
- `daemon` is the normal runtime mode; `once` and `diagnose` are validation
  helpers
- `mutterkey-tray` exists as an early tray shell, but the daemon remains the
  product core
- The recommended day-to-day path is the `systemd --user` service
- The installed desktop entry stays hidden from normal app menus with
  `NoDisplay=true`

Repository principles:

- CMake is the only supported build system
- Repo-owned code targets `C++23`; do not reintroduce `C++20` assumptions in
  app-owned build, docs, or guidance
- Keep repo-owned work KDE-first, local-only, and CLI/service-first
- Prefer product-owned runtime, package, selector, and native-loader seams over
  widening backend-shaped code
- Keep the repo free of generated build output, machine-specific paths, broken
  local links, and committed model binaries
- Do not reintroduce qmake, external-command transcription paths, plugin
  systems, or broad cross-platform abstractions unless explicitly requested

## Key Paths

- `src/main.cpp`: CLI entrypoint and mode selection
- `src/service.*`: daemon lifecycle and transcription wiring
- `src/app/*`: shared CLI/runtime command helpers
- `src/control/*`: local daemon control transport and typed snapshots
- `src/tray/*`, `src/traymain.cpp`: early tray-shell UI
- `src/audio/*`: recorder, recording payload, normalization
- `src/transcription/transcriptionengine.*`: app-owned engine/session seam
- `src/transcription/transcriptiontypes.h`: core runtime value types
- `src/transcription/runtimeselector.*`: runtime-selection policy and reasons
- `src/transcription/cpureferencemodel.*`: native model parsing/loading
- `src/transcription/cpureferencetranscriber.*`: native CPU reference runtime
- `src/transcription/cpusessionstate.*`, `cputimestamps.*`: native session
  state and transcript timestamp helpers
- `src/transcription/cputensor.*`: product-owned 2D tensor type and operations
  for the native CPU runtime
- `src/transcription/cpumelspectrogram.*`: log-mel spectrogram frontend (FFT,
  Hann window, mel filterbank)
- `src/transcription/cpumodelweights.*`: MKCPUR3 tensor format loader and
  encoder/decoder weight structures
- `src/transcription/cpuencoderforward.*`: transformer encoder forward pass
- `src/transcription/cpudecoderforward.*`: transformer decoder forward pass
  with incremental KV cache
- `src/transcription/cpugreedysearch.*`: greedy argmax token generation loop
- `src/transcription/cputokenizer.*`, `cputokensearch.*`,
  `cpudecodergraph.*`, `cpufeatureextractor.*`: staged native decoder helper
  modules
- `src/transcription/whispercpptranscriber.*`: vendored whisper adapter
- `src/transcription/modelpackage.*`, `modelvalidator.*`, `modelcatalog.*`:
  package contract, validation, artifact inspection
- `src/transcription/rawwhisperprobe.*`, `rawwhisperimporter.*`: raw-file
  migration path
- `src/transcription/audiochunker.*`, `transcriptassembler.*`,
  `transcriptioncompat.*`, `transcriptionworker.*`: streaming helpers,
  compatibility path, worker-thread orchestration
- `src/config.*`: JSON config loading/defaults
- `tests/*`: deterministic Qt Test / `CTest` coverage
- `contrib/mutterkey.service`, `contrib/org.mutterkey.mutterkey.desktop`:
  shipped service/desktop assets
- `docs/mainpage.md`, `docs/Doxyfile.in`: repo-owned API docs inputs
- `scripts/check-test-commentary.sh`, `scripts/check-release-hygiene.sh`,
  `scripts/run-valgrind.sh`, `scripts/update-whisper.sh`
- `next_feature/`: tracked feature plans as Markdown only
- `third_party/whisper.cpp`: vendored dependency
- `third_party/whisper.cpp.UPSTREAM.md`: vendor provenance notes

## Build And Validation

Read `README.md` first, especially `Overview`, `Quick Start`, `Run As Service`,
and `Development`, before changing build/setup behavior.

Use an out-of-tree build when possible:

```bash
BUILD_DIR="$(mktemp -d /tmp/mutterkey-build-XXXXXX)"
cmake -S . -B "$BUILD_DIR" -G Ninja
cmake --build "$BUILD_DIR" -j"$(nproc)"
```

If `ninja-build` is unavailable, omit `-G Ninja`.

Validate no-legacy runtime paths with:

```bash
cmake -S . -B "$BUILD_DIR" -G Ninja -DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF
```

If a sandboxed build hits `ccache: error: Read-only file system`, rerun with
`CCACHE_DISABLE=1`. If vendored `ggml` still routes through its own cache
logic, reconfigure with `GGML_CCACHE=OFF`. Treat those as environment limits,
not repo regressions.

Lightweight default validation:

```bash
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" --help
```

Add this when startup, config, or service wiring changes:

```bash
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" diagnose 1
```

Use the smallest validation set that proves the change, then extend as needed:

- `ctest --test-dir "$BUILD_DIR" --output-on-failure` for covered code
- `cmake --build "$BUILD_DIR" --target clang-tidy` for non-trivial C++ changes
- `cmake --build "$BUILD_DIR" --target clazy` for Qt-heavy changes when
  available
- `cmake --build "$BUILD_DIR" --target lint` for the full analyzer pass
- `cmake --build "$BUILD_DIR" --target docs` when touching repo-owned public
  headers, Doxygen comments, `docs/mainpage.md`, or docs/CI wiring
- `bash scripts/run-valgrind.sh "$BUILD_DIR"` for memory, lifetime, shutdown,
  or release-hardening work
- `bash scripts/check-release-hygiene.sh` for publication-facing files,
  repository metadata, or helper scripts

If install layout, licensing, or shipped assets change, validate a temporary
install prefix:

```bash
INSTALL_DIR="$(mktemp -d /tmp/mutterkey-install-XXXXXX)"
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"
```

Check installed license files under `share/licenses/mutterkey`.

## Testing Rules

- Keep repo-owned Qt tests deterministic and headless under `CTest`
- For Qt GUI/Widgets tests, set `QT_QPA_PLATFORM=offscreen` in test
  registration or test properties, not only in the caller environment
- Keep `WHAT/HOW/WHY` commentary near the start of real repo-owned test bodies;
  `scripts/check-test-commentary.sh` and
  `scripts/check-release-hygiene.sh` enforce that contract
- Prefer fake/injected engines for orchestration tests and fake sessions for
  narrow worker behavior
- Prefer pure tests around parsing, normalization, chunking, transcript
  assembly, compatibility wrappers, control payloads, and small helper seams
  before adding session-, device-, KDE-, clipboard-, or microphone-heavy tests
- For recorder, clipboard, hotkey, or other boundary-heavy code, extract narrow
  pure seams before trying to mock the full desktop/runtime environment
- Keep `daemoncontrolclientservertest` under normal `ctest`, but do not assume
  it is a good direct-launch Memcheck target in restricted sandboxes; some
  environments block direct `AF_UNIX` bind/listen with `EPERM`

## Coding Guidance

- Keep changes targeted; do not reformat unrelated code
- Prefer small direct classes and explicit ownership/lifetime over broad
  abstraction layers
- Keep Qt usage idiomatic: explicit `QObject` ownership, signals/slots, and
  `QThread` boundaries
- Prefer anonymous-namespace `Q_LOGGING_CATEGORY` for file-local logging
  categories; avoid `Q_STATIC_LOGGING_CATEGORY`
- Keep Qt class section structure valid for `moc`; do not flatten `signals`,
  `slots`, or `Q_SLOTS` sections just to satisfy generic style advice
- Favor `const`, `override`, `explicit`, `[[nodiscard]]`, narrow enums, RAII,
  rule-of-zero types, and clear invariants
- Treat raw pointers and references as non-owning observers unless ownership
  transfer is explicit
- Prefer standard library / Qt value types over raw arrays and manual bounds
  handling
- When helper functions take multiple adjacent same-shaped parameters, prefer a
  small request struct if it improves clarity and avoids
  `bugprone-easily-swappable-parameters`

Runtime- and model-specific guidance:

- Keep runtime-selection policy in `src/transcription/runtimeselector.*`, not
  buried in `createTranscriptionEngine()`
- Keep static backend support in `BackendCapabilities` and runtime/device/model
  inspection in `RuntimeDiagnostics`
- Keep `TranscriptionSession` focused on mutable behavior such as warmup, chunk
  ingestion, finish, and cancellation
- Keep native model parsing/loading separate from mutable session state; prefer
  `src/transcription/cpureferencemodel.*` or similar app-owned loader code
- Keep decoder-facing execution metadata in the package contract rather than
  overloading `ModelMetadata`; prefer `manifest.nativeExecution` for native
  decoder/search/frontend/timestamp invariants
- Treat `mkasr-v2` as the decoder-oriented native package contract and
  `mkasr-v1` as explicit legacy native-fixture compatibility; do not blur those
  meanings in selection or validation logic
- Keep package validation, metadata extraction, and compatibility checks
  app-owned; `whisper.cpp` should not be the first place that reports malformed
  or incompatible artifacts
- For ongoing Phase 5B work, prefer extracting more native decoder-owned seams
  before widening the transcriber adapter. The target flow is
  model loader -> frontend -> tokenizer/search -> timestamping -> session
  adapter, not one large `cpureferencetranscriber.cpp`
- Keep backend-specific validation out of `src/config.*` when practical.
  Product config should normalize input and apply defaults; backend support
  checks belong in app-owned runtime code
- Keep compatibility shims explicit in naming. One-shot daemon/CLI behavior
  implemented on the streaming seam should stay clearly identified as a
  compatibility path
- Use product-owned terminology in app-owned code: runtime audio, chunks,
  events, diagnostics, packages, manifests, metadata, compatibility markers,
  model artifact path
- Reserve backend-shaped wording for the whisper adapter and raw-file migration
  path

## Tooling Guidance

- Treat `clang-tidy`, `clazy`, Doxygen, the hygiene scripts, and CI workflow
  expectations as maintained checks, not optional extras
- Prefer the repo-owned `clang-tidy` target from a real configured build tree so
  Qt test `*_autogen` / `.moc` outputs exist
- `clang-tidy` should only be run on files present in the active build's
  `compile_commands.json`; conditionally built sources such as the legacy
  whisper adapter need a matching configured build or should be skipped
- When touching `src/transcription/whispercpptranscriber.*` or
  `tests/whispercpptranscribertest.cpp`, prefer validating analyzer behavior
  from a legacy-enabled build tree so vendored include paths and generated test
  MOC files are available
- Keep analyzer fixes targeted to `src/` and `tests/`; do not churn
  `third_party/` or generated Qt output
- Prefer fixing code over weakening `.clang-tidy` or Clazy config
- In this Qt-heavy repo, treat `misc-include-cleaner` and
  `readability-redundant-access-specifiers` as low-value `clang-tidy` noise
  unless the tool behavior improves
- If `clang-tidy` flags a new small enum for `performance-enum-size`, prefer an
  explicit narrow underlying type such as `std::uint8_t`
- If it flags a small binary header type, prefer
  `std::array<std::byte, N>` or `std::array<char, N>` with value
  initialization over C-style arrays
- Prefer the repo-owned Valgrind runner over ad hoc Memcheck commands so leak
  policy and deterministic target selection stay aligned
- Keep the default Valgrind lane deterministic and headless
- Do not add broad Valgrind suppressions by default; keep any needed
  suppressions narrow and stable

## Dependencies And Release Rules

- Treat `third_party/whisper.cpp` as vendored subtree-managed source
- Keep repo-owned `C++23` requirements separate from vendored
  `whisper.cpp` / `ggml` language settings unless a vendor task explicitly
  requires boundary changes
- Prefer app-side integration changes before touching vendored code
- Prefer `bash scripts/update-whisper.sh <tag-or-commit>` for vendor updates
  from a clean Git work tree
- Do not restore removed upstream examples/tests/scripts unless explicitly
  required
- If vendored code changes, document why and update
  `third_party/whisper.cpp.UPSTREAM.md`
- Prefer fixing vendored target metadata from top-level CMake when the issue is
  packaging or warning noise

- Repo-owned source is MIT-licensed in `LICENSE`
- Third-party licensing and provenance notes live in
  `THIRD_PARTY_NOTICES.md`
- Do not commit raw Whisper `.bin` files, `.gguf` files, or native Mutterkey
  model packages
- If a release needs a model, include it only in the release artifact or as a
  separate release asset outside Git
- Keep publication-facing files free of machine-specific home-directory paths,
  broken links, and generated output
- If install layout or shipped assets change, keep CMake install rules, README,
  `RELEASE_CHECKLIST.md`, service/desktop assets, and license installs aligned
- If legacy whisper install behavior changes, keep `README.md`,
  `RELEASE_CHECKLIST.md`, `docs/mainpage.md`, install rules, and license
  installs aligned
- Do not start installing vendored upstream public headers unless the package
  contract intentionally changes

## Config Expectations

Important `transcriber` fields:

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
~/.local/share/mutterkey/models/<package-id>
```

`transcriber.model_path` semantics:

- package directory is canonical
- `model.json` manifest path is accepted
- raw whisper.cpp-compatible `.bin` files are migration compatibility only

## Agent Workflow

- Read `README.md` first, then the touched files before editing
- Prefer targeted changes over speculative cleanup
- If daemon, tray, or control behavior grows, prefer extracting focused
  repo-owned modules under `src/app/`, `src/control/`, or similar instead of
  piling more orchestration into `src/main.cpp`
- Update `README.md` and `config.example.json` when behavior or setup changes
- Update `RELEASE_CHECKLIST.md` for release-facing packaging, assets, or bundle
  guidance changes
- Update `contrib/mutterkey.service` and
  `contrib/org.mutterkey.mutterkey.desktop` when service/desktop behavior
  changes
- Update `LICENSE`, `THIRD_PARTY_NOTICES.md`, install rules, and
  `third_party/whisper.cpp.UPSTREAM.md` when packaging, licensing, or vendor
  behavior changes
- Keep `README.md`, `AGENTS.md`, and relevant local skills aligned when the
  repo workflow changes
- Store forward-looking feature plans under `next_feature/` as tracked Markdown
  files; update the existing plan rather than scattering notes
- If a task changes the real status of a tracked phase, update the existing
  roadmap under `next_feature/` so the recorded phase state matches the repo
  rather than leaving stale "planned" markers behind
- Keep architecture evolution incremental: preserve the shipped whisper path
  while moving ownership of interfaces, tests, and packaging toward app-owned
  code
- Treat `mutterkey-tray` as a shipped artifact once it is installed or checked
  in CI; keep install rules and release docs aligned
- Validate a no-legacy build when the task affects runtime selection, native
  model loading, or legacy-whisper toggles
- If `diagnose 1` fails only in a headless environment after model loading or
  during KDE/session-dependent startup, note the environment limitation instead
  of assuming a regression
- Do not leave generated artifacts in the repo tree
- Do not assume the workspace is a valid Git repo; if Git commands fail,
  continue with file-based validation and note the limitation

## When Unsure

- Optimize for the current implementation, not hypothetical future backends
- Prefer repo-owned native/runtime/package seams before widening
  `whisper.cpp`-shaped code
- Ask before making architectural expansions beyond KDE, Qt, and the current
  local transcription design
- Keep the repo in a state that would be reasonable for a clean initial commit
