# Mutterkey Documentation

<img width="2048" height="1024" alt="mutterkey_logo" src="https://github.com/user-attachments/assets/e1a2cc70-430b-4ba9-8a43-c3c1e01b5670" />

`Mutterkey` is a native `C++ + Qt 6` push-to-talk transcription tool for
`KDE Plasma`.

This documentation is generated from the repo-owned C++ headers under `src/`.
It focuses on the application's core interfaces, ownership boundaries, and the
main daemon-mode workflow.

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
- tray-shell work has started, but the daemon remains the product core

Core API surface covered here:

- `HotkeyManager` registers the global push-to-talk shortcut through KDE.
- `AudioRecorder` captures microphone audio while the shortcut is held.
- `RecordingNormalizer` converts captured audio to Whisper-ready mono `float32`
  samples at `16 kHz`.
- `WhisperCppTranscriber` performs in-process transcription through vendored
  `whisper.cpp`.
- `ClipboardWriter` copies the resulting text to the clipboard.
- `MutterkeyService` coordinates those pieces on the main thread plus a
  dedicated transcription worker thread.

For build, runtime, and service setup use the repository `README.md`.
