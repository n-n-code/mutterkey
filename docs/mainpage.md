# Mutterkey API Documentation

`Mutterkey` is a native `C++ + Qt 6` push-to-talk transcription tool for
`KDE Plasma`.

This documentation is generated from the repo-owned C++ headers under `src/`.
It focuses on the application's core interfaces, ownership boundaries, and the
main daemon-mode workflow:

- `HotkeyManager` registers the global push-to-talk shortcut through KDE.
- `AudioRecorder` captures microphone audio while the shortcut is held.
- `RecordingNormalizer` converts captured audio to Whisper-ready mono `float32`
  samples at `16 kHz`.
- `WhisperCppTranscriber` performs in-process transcription through vendored
  `whisper.cpp`.
- `ClipboardWriter` copies the resulting text to the clipboard.
- `MutterkeyService` coordinates those pieces on the main thread plus a
  dedicated transcription worker thread.

For build, runtime, and service setup details, use the repository `README.md`.
