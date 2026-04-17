# Feature: Real CPU Decoder Bring-Up

## Motivation

The native CPU runtime now has a real tensor-backed decoder path compiled and
wired into the existing engine/model/session seam, but it is not yet validated
end-to-end well enough to replace `whisper.cpp` for end users.

## Proposed Behavior

Complete Phase 5B by turning the current native tensor path into a defensible,
measured, end-user-capable baseline for the chosen model family. The work in
this plan covers:

- MKCPUR3 weight conversion or documented export tooling
- end-to-end integration testing with a real or synthetic V3 weight file
- tensor-dimension validation during weight loading
- real decoder-emitted timestamp behavior
- short-utterance conformance and parity evidence against `whisper.cpp`
- latency and memory measurement on the chosen baseline model family
- elimination of known decoder/encoder performance bottlenecks

## Lifecycle

- State: done
- Supersedes: feature_records/superseded/fresh-asr-runtime-roadmap.md
- Superseded by: none

## Contract

- Must remain true: The current engine/model/session boundary, native package contract, and optional legacy-whisper build path stay intact while native internals improve.
- Must become true: The native CPU backend performs real end-user speech recognition for the chosen baseline model family with measurable quality, latency, and deterministic lifecycle behavior.
- Success signals: Native CPU speech recognition works on real model weights, conformance and parity lanes exist, tensor dimensions are validated before execution, and latency/resource measurements are available.

## Uncertainty And Cost

- Product uncertainty: medium
- Technical uncertainty: high
- Implementation cost: high
- Validation cost: high
- Notes: This phase combines model conversion, runtime correctness, security validation, conformance, and performance work across several app-owned native CPU seams.

## Responsibilities

- Implementer: native-runtime-owner
- Verifier: runtime-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The implementation adds package-staging, prompt/suppression metadata round-trip coverage, default synthetic MKCPUR3 coverage, env-gated real JFK conformance tests, and a legacy parity lane. | verifier_note=Default no-artifact validation passed with `ctest --test-dir /tmp/mutterkey-build-codex --output-on-failure` (20/20). External Release evidence passed: `cpurealdecodertest` decoded the canonical JFK phrase, and `cpuwhisperparitytest` scored 1.000 similarity against whisper.cpp on the same audio.
- Docs | impact=yes | status=passed | rationale=README, native CPU staging docs, and measurement notes describe the staging path, env-gated validation lanes, local artifact requirements, and the accepted real-decoder baseline. | verifier_note=Updated `README.md`, `docs/native-cpu-model-staging.md`, and `docs/measurements/2026-04-native-cpu-base-en.md`.
- Analyzers | impact=yes | status=passed | rationale=The touched C++ runtime and test sources are covered by the configured no-legacy clang-tidy target; Clazy is not material for this non-Widget/non-QObject runtime slice. | verifier_note=Validated with `cmake --build /tmp/mutterkey-build-codex --target clang-tidy`; legacy whisper files were skipped because they are not part of the no-legacy compile database.
- Install validation | impact=no | status=not_applicable | rationale=This phase is primarily runtime correctness and measurement work. | verifier_note=Install validation becomes relevant only if shipped assets or install layout change.
- Release hygiene | impact=yes | status=passed | rationale=This slice changed repo-owned tests and the active feature record, so release hygiene and contract checks were rerun even though shipped runtime assets did not change. | verifier_note=Validated with `bash scripts/check-release-hygiene.sh` and `bash scripts/check-change-contracts.sh`.

## Implementation Notes

- Owner: native-runtime-owner
- Status: completed
- Notes: RealDecoderV3, MKCPUR3, mel/frontend, encoder, decoder, KV cache, and greedy search are in place. 2026-04-13 slice: real decoder search consumes package execution metadata for no-speech and timestamp token IDs, clamps generated tokens to the packaged text context, and has default synthetic V3 load/dimension coverage. 2026-04-16 slice: package staging emits native execution prompt and suppression metadata from HuggingFace config, real JFK conformance/parity tests exist, and `nativecpubench` records load/decode/RSS. 2026-04-17 correction: Conv1d now indexes flattened PyTorch weights as `[out, in, kernel]`, and the mel frontend now matches Whisper's Slaney/librosa filterbank, log10, and normalization behavior; base.en JFK conformance and whisper.cpp parity now pass with clean user-facing text.

## Verification Notes

- Owner: runtime-verifier
- Status: completed
- Commands: `STAGING_DIR=/path/to/whisper-base-en-staging`; `cmake -S . -B /tmp/mutterkey-build-codex -G Ninja -DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF`; `cmake --build /tmp/mutterkey-build-codex -j8`; `ctest --test-dir /tmp/mutterkey-build-codex --output-on-failure`; `cmake --build /tmp/mutterkey-build-codex --target clang-tidy`; `QT_QPA_PLATFORM=offscreen /tmp/mutterkey-build-codex/mutterkey --help`; `python3 tools/stage_native_package.py --weights "$STAGING_DIR/base-en.mkweights" --vocab-json "$STAGING_DIR/vocab.json" --merges "$STAGING_DIR/merges.txt" --baseline whisper-base-en --output /tmp/mutterkey-packages/whisper-base-en --force`; `cmake -S . -B /tmp/mutterkey-build-codex-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF`; `cmake --build /tmp/mutterkey-build-codex-release -j8`; `MUTTERKEY_TEST_WEIGHTS_PATH="$STAGING_DIR/base-en.mkweights" MUTTERKEY_TEST_PACKAGE_PATH=/tmp/mutterkey-packages/whisper-base-en QT_QPA_PLATFORM=offscreen /tmp/mutterkey-build-codex-release/tests/cpurealdecodertest`; `/tmp/mutterkey-build-codex-release/tools/nativecpubench/nativecpubench --package /tmp/mutterkey-packages/whisper-base-en --audio third_party/whisper.cpp/samples/jfk.wav --warmup 0 --runs 1 --max-tokens 96 --max-mel-frames 1200`; `cmake -S . -B /tmp/mutterkey-build-codex-legacy-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DMUTTERKEY_ENABLE_LEGACY_WHISPER=ON`; `cmake --build /tmp/mutterkey-build-codex-legacy-release -j8`; `MUTTERKEY_TEST_PACKAGE_PATH=/tmp/mutterkey-packages/whisper-base-en MUTTERKEY_TEST_LEGACY_WEIGHTS_PATH="$STAGING_DIR/ggml-base.en.bin" QT_QPA_PLATFORM=offscreen /tmp/mutterkey-build-codex-legacy-release/tests/cpuwhisperparitytest`; `bash scripts/check-release-hygiene.sh`; `bash scripts/check-change-contracts.sh`
- Observed result: Native-only configure/build passed; default CTest passed 18/18 with real-artifact tests skipped when env vars are unset; staged package generation succeeded and emitted `[50362]` initial prompt plus 90 suppressed token ids from HuggingFace config; Release real JFK conformance passed with transcript `And so my fellow Americans ask not what your country can do for you, ask what you can do for your country.`; benchmark recorded cold load 1419 ms, warm decode 11672 ms, peak RSS 350284 KiB, and the same JFK transcript; legacy parity passed with native/legacy similarity 1.000.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `src/asr/nativecpu/*` — decoder execution, model loading, tokenizer, timestamps, and performance work
- `src/asr/model/*` — native model validation and converter-facing metadata rules
- `tests/*` — end-to-end native CPU integration, conformance, and parity coverage
- `tools/*` or `scripts/*` — MKCPUR3 conversion/export tooling if the repo owns it
- `README.md` and release-facing docs when native user-facing claims change

## Follow-Up Work

Phase 5B now has accepted real-weight correctness, parity, and measurement
evidence for the base.en JFK lane. Performance tuning remains intentionally
deferred: the current CPU tensor and attention loops are measured but still
naive, with no SIMD, blocking, or BLAS optimization in this phase.

## Testing Strategy

Completion should require:

- end-to-end native V3 integration coverage with a real or synthetic model
- tensor-dimension validation tests for malformed files
- short-utterance conformance corpus coverage
- parity comparison lane against `whisper.cpp` where parity remains part of migration
- measured cold start, warm start, and short-utterance latency
- normal repo validation for touched files and docs

## Open Questions

- Resolved: MKCPUR3 conversion and package staging live in-repo under `tools/`.
- Resolved for now: committed coverage remains synthetic/env-gated; real weights
  and staged packages stay local artifacts outside Git until a small
  repository-safe fixture strategy exists.
