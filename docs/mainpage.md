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
- transcribes locally through the configured runtime path
- copies the resulting text to the clipboard
- expects you to paste the text yourself with `Ctrl+V`

Current runtime shape:

- `TranscriptionEngine` is the immutable runtime/provider boundary
- `TranscriptionSession` is the mutable per-session decode boundary
- native Mutterkey model packages are now the canonical model artifact
- internal audio flow is streaming-first through normalized chunks and
  transcript events
- `BackendCapabilities` reports static backend support used for orchestration
- `RuntimeDiagnostics` reports runtime/device/model inspection data separately
  from static capabilities, including runtime-selection reasoning
- `RuntimeError` and `RuntimeErrorCode` provide typed runtime failures
- `ModelCatalog`, `ModelPackage`, and `ModelValidator` own model inspection,
  compatibility checks, and integrity validation before backend load
- raw Whisper `.bin` files are handled only through an explicit compatibility
  path and import flow
- `RuntimeSelector` owns runtime-selection policy instead of burying that logic
  in the generic factory
- `CpuReferenceModelHandle` and related native model helpers own the current
  product-owned CPU reference model loading boundary
- `TranscriptionWorker` hosts transcription on a dedicated `QThread` and
  creates live sessions lazily on that worker thread
- the shipped daemon and `once` flows still use a compatibility wrapper that
  assembles a final transcript from the streaming runtime path
- config parsing under `src/config.*` stays product-shaped and permissive, while
  backend-specific support checks live in the runtime layer

Core API surface covered here:

- `HotkeyManager` registers the global push-to-talk shortcut through KDE.
- `AudioRecorder` captures microphone audio while the shortcut is held.
- `RecordingNormalizer` converts captured audio to runtime-ready mono `float32`
  samples at `16 kHz`.
- `AudioChunker` splits normalized audio into deterministic stream chunks.
- `TranscriptAssembler` builds final transcript text from streaming events.
- `ModelCatalog` resolves package directories, `model.json`, and legacy raw
  artifacts into validated product-owned model metadata.
- `RawWhisperImporter` converts raw whisper.cpp-compatible `ggml` `.bin` files
  into native Mutterkey packages.
- `RuntimeSelector` decides which runtime implementation should handle a given
  configured model path and records the reason in diagnostics.
- `TranscriptionEngine` and `TranscriptionSession` define the app-owned runtime
  seam.
- `CpuReferenceTranscriber` provides the current product-owned native CPU
  reference runtime scaffold.
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
- a product-owned native CPU reference runtime now exists for ownership,
  packaging, and conformance work
- `whisper.cpp` is still the only real end-user speech decoder today
- the vendored runtime is now optional at build time through
  `MUTTERKEY_ENABLE_LEGACY_WHISPER=OFF`

For build, runtime, release, and service setup use the repository `README.md`
and `RELEASE_CHECKLIST.md`.
