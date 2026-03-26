# Mutterkey Documentation

<img width="2048" height="1024" alt="mutterkey_logo" src="https://github.com/user-attachments/assets/e1a2cc70-430b-4ba9-8a43-c3c1e01b5670" />

`Mutterkey` is a native `C++ + Qt 6` push-to-talk transcription tool for
`KDE Plasma`.

This documentation is generated from the repo-owned C++ headers under `src/`.
It focuses on the application's ownership boundaries, runtime contracts, and
daemon-oriented workflow rather than end-user setup.

Current behavior:

- registers a global shortcut through `KGlobalAccel`
- records microphone audio while the shortcut is held
- transcribes locally through an embedded `whisper.cpp` backend
- copies the resulting text to the clipboard
- expects you to paste the text yourself with `Ctrl+V`

Current runtime shape:

- `TranscriptionEngine` is the immutable runtime/provider boundary
- `TranscriptionSession` is the mutable per-session decode boundary
- `BackendCapabilities` reports engine-owned runtime metadata used for
  diagnostics and orchestration
- `RuntimeError` and `RuntimeErrorCode` provide typed runtime failures
- `TranscriptionWorker` hosts transcription on a dedicated `QThread` and
  creates live sessions lazily on that worker thread
- config parsing under `src/config.*` stays product-shaped and permissive, while
  backend-specific support checks live in the runtime layer

Core API surface covered here:

- `HotkeyManager` registers the global push-to-talk shortcut through KDE.
- `AudioRecorder` captures microphone audio while the shortcut is held.
- `RecordingNormalizer` converts captured audio to Whisper-ready mono `float32`
  samples at `16 kHz`.
- `TranscriptionEngine` and `TranscriptionSession` define the app-owned runtime
  seam.
- `WhisperCppTranscriber` performs in-process transcription through vendored
  `whisper.cpp`.
- `ClipboardWriter` copies the resulting text to the clipboard.
- `MutterkeyService` coordinates those pieces on the main thread plus a
  dedicated transcription worker thread.

Current product direction:

- KDE-first
- local-only transcription
- CLI/service-first operation
- tray-shell work exists, but the daemon remains the product core
- `whisper.cpp` is still the only supported backend implementation

For build, runtime, release, and service setup use the repository `README.md`
and `RELEASE_CHECKLIST.md`.
