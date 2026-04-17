# Native CPU base.en measurement

Date: 2026-04-17

Machine: AMD Ryzen 5 5600X 6-Core Processor, Linux, local Release build.

Build:

```bash
cmake -S . -B /tmp/mutterkey-build-codex-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF
cmake --build /tmp/mutterkey-build-codex-release -j8
```

Package staging:

```bash
STAGING_DIR=/path/to/whisper-base-en-staging

python3 tools/stage_native_package.py \
  --weights "$STAGING_DIR/base-en.mkweights" \
  --vocab-json "$STAGING_DIR/vocab.json" \
  --merges "$STAGING_DIR/merges.txt" \
  --baseline whisper-base-en \
  --output /tmp/mutterkey-packages/whisper-base-en \
  --force
```

Benchmark command:

```bash
/tmp/mutterkey-build-codex-release/tools/nativecpubench/nativecpubench \
  --package /tmp/mutterkey-packages/whisper-base-en \
  --audio third_party/whisper.cpp/samples/jfk.wav \
  --warmup 0 --runs 1 --max-tokens 96 --max-mel-frames 1200
```

Result:

| Metric | Value |
| --- | ---: |
| Cold package/model load | 1419 ms |
| Warm decode min | 11672 ms |
| Warm decode median | 11672 ms |
| Warm decode p95 | 11672 ms |
| Peak RSS | 350284 KiB |
| Max decoder tokens | 96 |
| Max mel frames | 1200 |

Transcript:

> And so my fellow Americans ask not what your country can do for you, ask what you can do for your country.

Validation status: passed. The native CPU decoder produced the JFK canonical
phrase, so this run is accepted Phase 5B quality and measurement evidence for
the base.en short-utterance lane.

```json
{
    "audio": "third_party/whisper.cpp/samples/jfk.wav",
    "cold_load_ms": 1419,
    "max_decoder_tokens": 96,
    "max_mel_frames": 1200,
    "measured_runs": 1,
    "package": "/tmp/mutterkey-packages/whisper-base-en",
    "peak_rss_kib": 350284,
    "runs_ms": [
        11672
    ],
    "transcript": "And so my fellow Americans ask not what your country can do for you, ask what you can do for your country.",
    "warm_decode_ms_median": 11672,
    "warm_decode_ms_min": 11672,
    "warm_decode_ms_p95": 11672,
    "warmup_runs": 0
}
```
