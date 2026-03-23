# Vendored Upstream Metadata: whisper.cpp

- Dependency: `whisper.cpp`
- Upstream repository: <https://github.com/ggml-org/whisper.cpp>
- Vendored path: `third_party/whisper.cpp`
- Metadata last updated: `2026-03-22`

## Current Imported Snapshot

- Upstream project version visible in the vendored tree: `1.8.4`
  Source: `third_party/whisper.cpp/CMakeLists.txt`
- Exact upstream git commit: unavailable in the current vendored snapshot
  The imported tree does not include `.git` metadata, and upstream's own
  `cmake/build-info.cmake` falls back to `BUILD_COMMIT "unknown"` when the git
  history is unavailable.
- Import method for this snapshot: pre-existing vendored copy in `third_party/`

## Local Policy

- Keep `whisper.cpp` vendored under `third_party/`.
- Prefer changing Mutterkey integration code over patching vendored upstream
  files.
- If vendored files must be modified, record the reason below and mention it in
  the relevant commit message.

## Local Modifications Against Upstream

- None recorded as part of this metadata pass.

## Future Updates

Prefer updating through the repository helper:

```bash
bash scripts/update-whisper.sh <upstream-tag-or-commit>
```

After updating:

1. refresh this metadata file with the exact imported ref
2. review `THIRD_PARTY_NOTICES.md` for notice changes
3. rebuild and rerun tests
