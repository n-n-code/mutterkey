# Mutterkey
[![Actions Status](https://github.com/n-n-code/mutterkey/workflows/CI/badge.svg)](https://github.com/n-n-code/mutterkey/actions)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)
## Overview

`Mutterkey` is a native `C++ + Qt 6` push-to-talk transcription tool for
`KDE Plasma`.

Current behavior:

- registers a global shortcut through `KGlobalAccel`
- records microphone audio while the shortcut is held
- transcribes locally through an embedded `whisper.cpp` backend
- copies the resulting text to the clipboard
- expects you to paste the text yourself with `Ctrl+V`

Current direction:

- KDE-first
- local-only transcription
- CLI/service-first operation
- no GUI yet
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

1. Qt 6 development packages with `Core`, `Gui`, and `Multimedia`
2. KDE Frameworks development packages for `KGlobalAccel` and `KGuiAddons`
3. `g++`
4. `cmake`

Runtime requirements:

1. a local Whisper model file
2. a config file at `~/.config/mutterkey/config.json` or a custom `--config` path

Optional developer tooling:

- Qt 6 `Test`
- `clang-tidy`
- `clazy-standalone`

The repository vendors `whisper.cpp`, but it does not bundle Whisper model
files. Any model file you download separately may be subject to its own license
or usage terms.

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

```bash
BUILD_DIR="$(mktemp -d /tmp/mutterkey-build-XXXXXX)"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build "$BUILD_DIR" -j"$(nproc)"
cmake --install "$BUILD_DIR"
```

This installs:

- `~/.local/bin/mutterkey`
- `~/.local/lib/libwhisper.so*` and the required `ggml` libraries
- `~/.local/share/applications/org.mutterkey.mutterkey.desktop`

### 2. Put a Whisper model on disk

Example location:

```text
~/.local/share/mutterkey/models/ggml-base.en.bin
```

### 3. Create the config file

```bash
mkdir -p ~/.config/mutterkey
cp config.example.json ~/.config/mutterkey/config.json
```

Edit `~/.config/mutterkey/config.json` and set at least:

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
    "model_path": "/absolute/path/to/ggml-base.en.bin",
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
- invalid numeric values fall back to safe defaults and log a warning
- empty `shortcut.sequence` or `transcriber.model_path` values fall back to defaults and log a warning

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
before enabling it.

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

`Embedded Whisper model not found: ...`

- the embedded backend is active
- the configured model path does not exist
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
- `src/transcription/whispercpptranscriber.*`: embedded Whisper integration
- `src/transcription/transcriptionworker.*`: worker object on a dedicated `QThread`
- `src/transcription/transcriptiontypes.h`: normalized-audio and transcription result value types
- `src/clipboardwriter.*`: clipboard writes with KDE-first fallback behavior
- `src/config.*`: JSON config loading and defaults
- `contrib/mutterkey.service`: example user service

Build and test:

```bash
BUILD_DIR="$(mktemp -d /tmp/mutterkey-build-XXXXXX)"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
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

Notes for contributors:

- prefer an out-of-tree build so the repository stays clean
- keep changes targeted to repo-owned code in `src/`, `tests/`, and top-level project files
- avoid editing `third_party/whisper.cpp` unless the task is specifically about the vendored dependency
- run `bash scripts/check-release-hygiene.sh` when changing publication-facing files such as this README, licenses, `contrib/`, or CI

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

- the repo exports `compile_commands.json` by default
- `lint` runs both analyzer targets
- tests are small headless `Qt Test` cases
- `config` and `recordingnormalizer` currently have the main unit-test coverage because they contain the most deterministic logic without KDE session or device dependencies
- GitHub Actions CI runs the hygiene job on Ubuntu 24.04 and the configure/build/test job in a Debian Trixie container because the needed KF6 dev packages are not available on the stock Ubuntu 24.04 runner image
- runtime validation for microphone capture, clipboard behavior, and KDE global
  shortcut registration still relies on `once`, `daemon`, and `diagnose`
- keep `third_party/whisper.cpp` treated as vendored code unless a task
  specifically requires touching it
