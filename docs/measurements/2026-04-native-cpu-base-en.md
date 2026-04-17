# Native CPU base.en measurement

Date: 2026-04-18

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
  --warmup 1 --runs 5 --max-tokens 96 --max-mel-frames 1200 \
  --repo-relative-paths
```

Result:

| Metric | Value |
| --- | ---: |
| Cold package/model load | 1457 ms |
| Warm decode min | 11659 ms |
| Warm decode median | 11715 ms |
| Warm decode p95 | 11772 ms |
| Peak RSS | 352092 KiB |
| Max decoder tokens | 96 |
| Max mel frames | 1200 |

Transcript:

> And so my fellow Americans ask not what your country can do for you, ask what you can do for your country.

Validation status: passed. The native CPU decoder produced the JFK canonical
phrase across five measured runs with one warm-up run, so this distribution is
accepted Phase 5B quality and measurement evidence for the base.en
short-utterance lane.

```json
{
    "audio": "third_party/whisper.cpp/samples/jfk.wav",
    "cold_load_ms": 1457,
    "max_decoder_tokens": 96,
    "max_mel_frames": 1200,
    "measured_runs": 5,
    "package": "/tmp/mutterkey-packages/whisper-base-en",
    "peak_rss_kib": 352092,
    "runs_ms": [
        11730,
        11772,
        11715,
        11696,
        11659
    ],
    "transcript": "And so my fellow Americans ask not what your country can do for you, ask what you can do for your country.",
    "warm_decode_ms_median": 11715,
    "warm_decode_ms_min": 11659,
    "warm_decode_ms_p95": 11772,
    "warmup_runs": 1
}
```
