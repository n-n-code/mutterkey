# mutterkey
[![Actions Status](https://github.com/n-n-code/mutterkey/workflows/CI/badge.svg)](https://github.com/n-n-code/mutterkey/actions)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)

<img width="2048" height="1024" alt="mutterkey_logo" src="https://github.com/user-attachments/assets/e1a2cc70-430b-4ba9-8a43-c3c1e01b5670" />


## Overview

`mutterkey` is a native `C++ + Qt 6` push-to-talk transcription tool for
`KDE Plasma`.

Current behavior:

- registers a global shortcut through `KGlobalAccel`
- records microphone audio while the shortcut is held
- transcribes locally through an embedded `whisper.cpp` backend
- copies the resulting text to the clipboard
- expects you to paste the text yourself with `Ctrl+V`

Current runtime shape:

- `TranscriptionEngine` is the immutable runtime/provider boundary
- `TranscriptionSession` is the mutable per-session decode boundary
- internal audio flow is streaming-first through normalized chunks and transcript events
- `BackendCapabilities` reports static backend support, while `RuntimeDiagnostics`
  reports runtime/device/model inspection data
- the current daemon and `once` user flows still collapse the streaming path back
  into a final clipboard-friendly transcript

Current direction:

- KDE-first
- local-only transcription
- CLI/service-first operation
- tray-shell work has started, but the daemon remains the product core
- minimal and developer-oriented rather than a hardened end-user security product

Recommended startup path:

- run it as a `systemd --user` service
- use `once` and `diagnose` as validation tools, not the default daily workflow

Runtime modes:

- `daemon`: register the global shortcut and stay resident in the background
- `once <seconds>`: record once, transcribe once, print the text, and copy it to the clipboard
- `diagnose <seconds> [invoke]`: start the daemon wiring temporarily and print diagnostic JSON

This repository's own source code is MIT-licensed. See [LICENSE](LICENSE).
Vendored third-party code keeps its own notices and license files. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Requirements

Supported environment:

- Linux
- KDE Plasma
- a user session with access to microphone, clipboard, and `KGlobalAccel`

Build requirements:

1. Qt 6 development packages with `Core`, `Gui`, `Multimedia`, `Network`, and `Widgets`
2. KDE Frameworks development packages for `KGlobalAccel` and `KGuiAddons`
3. `g++`
4. `cmake`

Runtime requirements:

1. a local Mutterkey model package, or a raw Whisper `.bin` file for migration compatibility
2. a config file at `~/.config/mutterkey/config.json` or a custom `--config` path

Optional developer tooling:

- Qt 6 `Test`
- `clang-tidy`
- `clazy-standalone`
- `doxygen`
- `ninja-build`
- `valgrind`
- `libc6-dbg` on Debian-family systems so Valgrind Memcheck can start cleanly

The repository vendors `whisper.cpp`, but it does not bundle speech model
artifacts. Any model file you download separately may be subject to its own
license or usage terms.

If CMake fails before compilation starts, the most common cause is missing Qt 6
development packages for `Core`, `Gui`, `Multimedia`, or KDE Frameworks
packages for `KF6GlobalAccel` / `KF6GuiAddons`.

## Privacy And Security Notes

Mutterkey is designed to keep transcription local to the machine:

- microphone audio is captured through Qt Multimedia
- transcription runs in-process through the vendored `whisper.cpp` backend
- the project does not send audio or transcript text to a remote service

That does not make it a hardened privacy boundary:

- transcript text is written to the clipboard, so other local software or desktop
  integrations with clipboard access may still observe it
- service logs are intended for operational status and troubleshooting, not as a
  secure audit store
- the project currently targets KDE Plasma and assumes a normal desktop user
  session with microphone, clipboard, and `KGlobalAccel` access

If you need stronger isolation, treat Mutterkey as a convenience tool for a
trusted local session, not as a sandboxed dictation or secret-handling system.

## Quick Start

### 1. Build and install

Prefer `Ninja` when it is available so local builds match CI more closely.
If you do not have `ninja-build` installed, omit `-G Ninja` and let CMake use
the default generator for your machine.

```bash
BUILD_DIR="$(mktemp -d /tmp/mutterkey-build-XXXXXX)"
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build "$BUILD_DIR" -j"$(nproc)"
cmake --install "$BUILD_DIR"
```

This installs:

- `~/.local/bin/mutterkey`
- `~/.local/bin/mutterkey-tray`
- `~/.local/lib/libwhisper.so*` and the required `ggml` libraries
- `~/.local/share/applications/org.mutterkey.mutterkey.desktop`

If you configure with `-DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF`, Mutterkey builds
without the vendored `whisper.cpp` runtime and does not install the legacy
`libwhisper` / `ggml` shared libraries.

Optional acceleration flags:

```bash
cmake -S . -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
  -DMUTTERKEY_ENABLE_WHISPER_CUDA=ON
```

```bash
cmake -S . -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
  -DMUTTERKEY_ENABLE_WHISPER_VULKAN=ON
```

```bash
cmake -S . -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
  -DMUTTERKEY_ENABLE_WHISPER_BLAS=ON \
  -DMUTTERKEY_WHISPER_BLAS_VENDOR=OpenBLAS
```

Notes:

- `MUTTERKEY_ENABLE_WHISPER_CUDA=ON` is for NVIDIA GPUs and requires a working CUDA toolchain
- `MUTTERKEY_ENABLE_WHISPER_VULKAN=ON` is for Vulkan-capable GPUs and requires Vulkan development headers and loader libraries
- `MUTTERKEY_ENABLE_WHISPER_BLAS=ON` improves CPU inference speed rather than enabling GPU execution
- these options are forwarded to the vendored `whisper.cpp` / `ggml` build and install any resulting backend libraries alongside Mutterkey
- `-DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF` disables the vendored runtime entirely and skips all `whisper.cpp` / `ggml` install targets

### 2. Put a model on disk

Preferred Phase 4 path:

1. place a raw Whisper `.bin` file somewhere temporary
2. import it into a native Mutterkey package:

```bash
~/.local/bin/mutterkey model import /path/to/ggml-base.en.bin
```

This creates a package directory under:

```text
~/.local/share/mutterkey/models/<package-id>/
```

You can inspect a package or a legacy raw file with:

```bash
~/.local/bin/mutterkey model inspect /path/to/ggml-base.en.bin
```

Legacy compatibility path:

```text
~/.local/share/mutterkey/models/ggml-base.en.bin
```

### 3. Create the config file

```bash
mutterkey config init --model-path ~/.local/share/mutterkey/models/<package-id>
```

`mutterkey config init` writes the Linux config file to:

```text
~/.config/mutterkey/config.json
```

When run from a terminal, Mutterkey can also create this file automatically on
first launch if it does not exist yet. The interactive bootstrap asks for:

- `transcriber.model_path`
- `shortcut.sequence`

You can update saved values later with `mutterkey config set <key> <value>`.

Set at least:

- `shortcut.sequence`
- `transcriber.model_path`

Accepted log levels:

- `DEBUG`
- `INFO`
- `WARNING`
- `ERROR`

Minimal example:

```json
{
  "shortcut": {
    "sequence": "F8"
  },
  "transcriber": {
    "model_path": "/absolute/path/to/mutterkey-model-package",
    "language": "en",
    "translate": false,
    "threads": 0,
    "warmup_on_start": false
  }
}
```

See [config.example.json](config.example.json) for the full config.

Config notes:

- `transcriber.threads: 0` means auto-detect based on the local machine
- `transcriber.language` accepts a Whisper language code such as `en` or `fi`, or `auto` for language detection
- `transcriber.model_path` may point to a native Mutterkey package directory, a `model.json` manifest, or a legacy raw Whisper `.bin` file
- invalid numeric values fall back to safe defaults and log a warning
- invalid `transcriber.language` values fall back to the default and log a warning
- empty `shortcut.sequence` or `transcriber.model_path` values fall back to defaults and log a warning
- runtime flags such as `--model-path`, `--shortcut`, `--language`, `--translate`, `--threads`, and `--warmup-on-start` override the saved config for the current process only

### 4. Sanity-check the installed binary

```bash
QT_QPA_PLATFORM=offscreen ~/.local/bin/mutterkey --help
```

### 5. Start the user service

This is the recommended way to run Mutterkey.

```bash
mkdir -p ~/.config/systemd/user
cp contrib/mutterkey.service ~/.config/systemd/user/mutterkey.service
systemctl --user daemon-reload
systemctl --user enable --now mutterkey.service
```

The default service file assumes:

- installed binary at `%h/.local/bin/mutterkey`
- config file at `%h/.config/mutterkey/config.json`

If your paths differ, edit [contrib/mutterkey.service](contrib/mutterkey.service)
before enabling it. If the config file does not exist, the service will fail
fast and instruct you to run `mutterkey config init` from a terminal first.

### 6. Use the hotkey

1. hold the configured shortcut
2. speak
3. release the shortcut
4. paste with `Ctrl+V`

Optional validation commands:

```bash
~/.local/bin/mutterkey once 4
~/.local/bin/mutterkey diagnose 10
~/.local/bin/mutterkey diagnose 10 invoke
```

Notes:

- `once` and `diagnose` require a positive duration in seconds
- `once` also requires microphone access and a valid model path

## Run As Service

The template unit is [contrib/mutterkey.service](contrib/mutterkey.service).

The installed-binary setup from Quick Start is the default recommendation. Once
enabled, the service starts in your user session and keeps the global shortcut
registered in the background.

Useful commands:

```bash
systemctl --user status mutterkey.service
systemctl --user restart mutterkey.service
systemctl --user stop mutterkey.service
journalctl --user -u mutterkey.service -f
```

If you want a custom config path, change `ExecStart` in the unit file. A typical
installed setup looks like:

```text
%h/.local/bin/mutterkey daemon --config %h/.config/mutterkey/config.json
```

Useful config commands:

```bash
~/.local/bin/mutterkey config init --model-path ~/.local/share/mutterkey/models/<package-id>
~/.local/bin/mutterkey model inspect ~/.local/share/mutterkey/models/<package-id>
~/.local/bin/mutterkey config set shortcut.sequence Meta+F8
~/.local/bin/mutterkey config set transcriber.language fi
```

The desktop entry
[contrib/org.mutterkey.mutterkey.desktop](contrib/org.mutterkey.mutterkey.desktop)
is intentionally hidden from normal app menus because the project currently
works best as a background service rather than an interactive desktop app.

## Troubleshooting

First-line diagnostics:

```bash
~/.local/bin/mutterkey once 4
~/.local/bin/mutterkey diagnose 10
~/.local/bin/mutterkey diagnose 10 invoke
journalctl --user -u mutterkey.service -f
```

Common failures:

`Model artifact not found: ...`

- the configured package path, manifest path, or raw compatibility artifact does not exist
- fix `transcriber.model_path`

`Recorder returned no audio`

- microphone capture did not produce usable PCM data
- check your input device and session permissions

`once duration must be a positive number of seconds`

- the duration argument was missing, malformed, or non-positive
- use values such as `once 4` or `diagnose 10`

`error while loading shared libraries: libwhisper.so...`

- the app was installed without its runtime libraries, or the install is stale
- rerun `cmake --install "$BUILD_DIR"` or reinstall into your chosen prefix
- confirm `ldd ~/.local/bin/mutterkey | grep -E 'whisper|ggml'` resolves to `~/.local/lib`

`diagnose` shows zero press/release events

- the shortcut path is still the problem
- check that `KGlobalAccel` registered the shortcut
- confirm you are running inside a KDE Plasma user session

`once` works but the hotkey path does not

- audio capture and transcription are probably fine
- focus on the service, desktop session, and shortcut registration path

## Development

Repository layout:

- `src/main.cpp`: CLI entrypoint and mode selection
- `src/service.*`: daemon lifecycle and background transcription wiring
- `src/hotkeymanager.*`: KDE `KGlobalAccel` integration
- `src/audio/audiorecorder.*`: microphone capture
- `src/audio/recording.h`: shared recorded-audio payload passed between subsystems
- `src/audio/recordingnormalizer.*`: conversion to Whisper-ready mono `float32` at `16 kHz`
- `src/transcription/audiochunker.*`: fixed-size normalized streaming chunk generation
- `src/transcription/transcriptassembler.*`: final transcript assembly from streaming events
- `src/transcription/transcriptioncompat.*`: compatibility wrapper from one-shot recordings to the streaming runtime path
- `src/transcription/modelpackage.*`: product-owned manifest and validated package value types
- `src/transcription/modelvalidator.*`: package integrity and compatibility validation
- `src/transcription/modelcatalog.*`: model artifact inspection and resolution
- `src/transcription/rawwhisperprobe.*`: lightweight raw Whisper header inspection
- `src/transcription/rawwhisperimporter.*`: migration path from raw Whisper files to native packages
- `src/transcription/whispercpptranscriber.*`: embedded Whisper integration behind the app-owned runtime seam
- `src/transcription/transcriptionworker.*`: worker object on a dedicated `QThread`
- `src/transcription/transcriptiontypes.h`: runtime diagnostics, normalized-audio, chunk, event, and error value types
- `src/clipboardwriter.*`: clipboard writes with KDE-first fallback behavior
- `src/config.*`: JSON config loading and defaults
- `src/app/*`: shared CLI/runtime command helpers used by the main entrypoint
- `src/control/*`: local daemon control protocol, typed snapshots, and local-socket session/server wiring
- `src/tray/*`: Qt Widgets tray-shell UI scaffolding
- `contrib/mutterkey.service`: example user service

Build and test:

```bash
BUILD_DIR="$(mktemp -d /tmp/mutterkey-build-XXXXXX)"
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" -j"$(nproc)"
ctest --test-dir "$BUILD_DIR" --output-on-failure
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" --help
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" diagnose 1
```

Static analysis:

```bash
cmake --build "$BUILD_DIR" --target lint
cmake --build "$BUILD_DIR" --target clang-tidy
cmake --build "$BUILD_DIR" --target clazy
```

API documentation:

```bash
cmake --build "$BUILD_DIR" --target docs
```

Doxygen is an optional local dependency. When installed, the `docs` target
generates HTML documentation under `"$BUILD_DIR"/docs/doxygen/html`. CI installs
Doxygen and treats documentation warnings in repo-owned code as failures. The
generated main page comes from `docs/mainpage.md`; keep that page focused on the
repo-owned API surface instead of pointing Doxygen at the full release-facing
README, which contains links to files outside the API-doc input set. On GitHub,
the same generated HTML tree is published to GitHub Pages from successful
`main` branch CI runs. Enable `Settings -> Pages -> Source: GitHub Actions`
once in the repository so the deployment job can publish the site. The expected
published URL is <https://n-n-code.github.io/mutterkey/>.

Memory diagnostics:

```bash
BUILD_DIR_ASAN="$(mktemp -d /tmp/mutterkey-asan-build-XXXXXX)"
cmake -S . -B "$BUILD_DIR_ASAN" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMUTTERKEY_ENABLE_ASAN=ON -DMUTTERKEY_ENABLE_UBSAN=ON
cmake --build "$BUILD_DIR_ASAN" -j"$(nproc)"
ctest --test-dir "$BUILD_DIR_ASAN" --output-on-failure

bash scripts/run-valgrind.sh "$BUILD_DIR"
cmake --build "$BUILD_DIR" --target valgrind
```

Valgrind and sanitizers have different roles:

- use `MUTTERKEY_ENABLE_ASAN` / `MUTTERKEY_ENABLE_UBSAN` for fast developer iteration and CI-friendly memory or UB checks
- use `bash scripts/run-valgrind.sh "$BUILD_DIR"` or the `valgrind` target as the slower release-readiness gate
- the default Valgrind lane stays deterministic and headless: it runs the pure/configuration/control/runtime helper tests plus `mutterkey --help`, and still avoids microphone, clipboard-heavy, tray, or KDE hotkey integration paths
- the default Valgrind lane intentionally does not run live microphone capture, clipboard-heavy flows, or KDE hotkey/service integration

Notes for contributors:

- prefer an out-of-tree build so the repository stays clean
- keep changes targeted to repo-owned code in `src/`, `tests/`, and top-level project files
- avoid editing `third_party/whisper.cpp` unless the task is specifically about the vendored dependency
- run `bash scripts/check-release-hygiene.sh` when changing publication-facing files such as this README, licenses, `contrib/`, CI, or test-commentary tooling; it also enforces `WHAT/HOW/WHY` coverage in repo-owned test cases

Release hygiene:

```bash
bash scripts/check-release-hygiene.sh
```

Vendored `whisper.cpp` updates:

```bash
bash scripts/update-whisper.sh <upstream-tag-or-commit>
```

Dependency metadata for the current imported snapshot lives in
[third_party/whisper.cpp.UPSTREAM.md](third_party/whisper.cpp.UPSTREAM.md).

Notes:

- `scripts/update-whisper.sh` requires a clean Git work tree before it will fetch or run subtree operations
- `third_party/whisper.cpp` is maintained through the subtree workflow; use the helper instead of ad hoc vendor-directory replacement
- the repo exports `compile_commands.json` by default
- local docs prefer `-G Ninja` to match CI, but generator-agnostic `cmake -S . -B "$BUILD_DIR"` remains supported when Ninja is not installed
- `lint` runs both analyzer targets
- `docs` is available only when Doxygen is installed during configuration
- the top-level install rules intentionally clear vendored `PUBLIC_HEADER`
  metadata on `whisper` and `ggml` so Mutterkey can install the shared
  libraries without inheriting upstream header-install warnings
- the `valgrind` target runs the repo-owned Memcheck lane used for release readiness
- tests are small headless `Qt Test` cases
- streaming runtime helpers and worker orchestration now also have deterministic headless coverage through fake backends
- GitHub Actions CI runs the hygiene job on Ubuntu 24.04 and the configure/build/test job in a Debian Trixie container because the needed KF6 dev packages are not available on the stock Ubuntu 24.04 runner image
- successful `main` branch CI runs publish `build/docs/doxygen/html` to GitHub Pages with the official Pages actions
- GitHub Actions release checks run a separate Valgrind Memcheck lane on manual dispatch and `v*` tags so normal PR CI stays faster
- runtime validation for microphone capture, clipboard behavior, and KDE global
  shortcut registration still relies on `once`, `daemon`, and `diagnose`
- keep `third_party/whisper.cpp` treated as vendored code unless a task
  specifically requires touching it
