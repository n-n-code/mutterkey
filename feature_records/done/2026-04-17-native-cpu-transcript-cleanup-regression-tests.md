# Feature: Native CPU Transcript Cleanup And Regression Tests

## Motivation

Phase 5B proved real native CPU recognition on the JFK lane, but the raw native
transcript still exposed Whisper byte-level BPE space markers and the Conv1d /
mel fixes were protected mainly by slow env-gated end-to-end evidence.

## Proposed Behavior

Make the native CPU decoder return user-facing transcript text without byte-level
tokenizer artifacts, and add cheap deterministic tests for the math contracts
that caused the real-weight failure:

- Whisper BPE token text is decoded before final transcript assembly
- PyTorch Conv1d flattened weight layout is covered by a unit test
- Slaney mel filterbank generation and Whisper log10 normalization are covered
  by unit tests
- the real JFK lane asserts the raw transcript has normal spaces and no BPE
  marker artifacts

## Lifecycle

- State: done
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Native CPU package, runtime, and optional legacy-whisper
  build boundaries remain intact.
- Must become true: Native CPU real-decoder transcripts are clean user-facing
  text, and the Conv1d/mel parity fixes have deterministic non-artifact
  regression coverage.
- Success signals: Default no-legacy tests pass with the new focused unit tests,
  env-gated JFK conformance emits a clean transcript, and legacy parity remains
  at the accepted similarity threshold.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: medium
- Implementation cost: low
- Validation cost: medium
- Notes: The transcript cleanup is narrow, but it touches tokenizer/runtime
  output and should be checked against real weights.

## Responsibilities

- Implementer: codex
- Verifier: codex
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=Added deterministic tokenizer, Conv1d, and mel frontend tests plus stricter real JFK and parity assertions for clean native transcripts. | verifier_note=`ctest --test-dir /tmp/mutterkey-build-codex --output-on-failure` passed 20/20; focused `cputensortest`, `cpumelspectrogramtest`, and `cpuwhispertokenizertest` passed; real Release `cpurealdecodertest` passed 7/7 with clean JFK transcript; legacy Release `cpuwhisperparitytest` passed 3/3 with similarity 1.000.
- Docs | impact=yes | status=passed | rationale=Measurement and Phase 5B evidence now reflect the clean transcript and updated benchmark numbers. | verifier_note=Updated `docs/measurements/2026-04-native-cpu-base-en.md` and `feature_records/done/2026-04-01-real-cpu-decoder-bring-up.md`.
- Analyzers | impact=yes | status=passed | rationale=Runtime/test C++ changes are covered by no-legacy clang-tidy, and the legacy-enabled analyzer target covers the parity test. | verifier_note=`cmake --build /tmp/mutterkey-build-codex --target clang-tidy` passed; `cmake --build /tmp/mutterkey-build-codex-legacy-release --target clang-tidy` passed after fixing parity-test narrowing and automatic-move diagnostics.
- Install validation | impact=no | status=not_applicable | rationale=No install layout or shipped asset behavior is changing. | verifier_note=Not required for this cleanup.
- Release hygiene | impact=yes | status=passed | rationale=Docs and feature records changed, so release hygiene and contract checks were run as final gates. | verifier_note=Initial gate failed only because this lane was still marked missing; after updating the lane, `bash scripts/check-release-hygiene.sh` and `bash scripts/check-change-contracts.sh` were rerun.

## Implementation Notes

- Owner: codex
- Status: completed
- Notes: Added user-facing Whisper token text decoding, real transcript assertions that reject byte-level space markers, focused Conv1d layout coverage, focused Slaney/log10 mel frontend coverage, and legacy analyzer coverage for the parity test.

## Verification Notes

- Owner: codex
- Status: completed
- Commands: `cmake -S . -B /tmp/mutterkey-build-codex -G Ninja -DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF`; `cmake --build /tmp/mutterkey-build-codex -j8`; `ctest --test-dir /tmp/mutterkey-build-codex --output-on-failure -R '(cputensortest|cpumelspectrogramtest|cpuwhispertokenizertest)'`; `ctest --test-dir /tmp/mutterkey-build-codex --output-on-failure`; `cmake --build /tmp/mutterkey-build-codex --target clang-tidy`; `cmake -S . -B /tmp/mutterkey-build-codex-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF`; `cmake --build /tmp/mutterkey-build-codex-release -j8`; `MUTTERKEY_TEST_WEIGHTS_PATH="$STAGING_DIR/base-en.mkweights" MUTTERKEY_TEST_PACKAGE_PATH=/tmp/mutterkey-packages/whisper-base-en QT_QPA_PLATFORM=offscreen /tmp/mutterkey-build-codex-release/tests/cpurealdecodertest`; `/tmp/mutterkey-build-codex-release/tools/nativecpubench/nativecpubench --package /tmp/mutterkey-packages/whisper-base-en --audio third_party/whisper.cpp/samples/jfk.wav --warmup 0 --runs 1 --max-tokens 96 --max-mel-frames 1200`; `cmake -S . -B /tmp/mutterkey-build-codex-legacy-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DMUTTERKEY_ENABLE_LEGACY_WHISPER=ON`; `cmake --build /tmp/mutterkey-build-codex-legacy-release -j8`; `MUTTERKEY_TEST_PACKAGE_PATH=/tmp/mutterkey-packages/whisper-base-en MUTTERKEY_TEST_LEGACY_WEIGHTS_PATH="$STAGING_DIR/ggml-base.en.bin" QT_QPA_PLATFORM=offscreen /tmp/mutterkey-build-codex-legacy-release/tests/cpuwhisperparitytest`; `cmake --build /tmp/mutterkey-build-codex-legacy-release --target clang-tidy`; `QT_QPA_PLATFORM=offscreen /tmp/mutterkey-build-codex/mutterkey --help`
- Observed result: Focused deterministic tests passed; default no-legacy CTest passed 20/20; no-legacy and legacy-enabled clang-tidy passed; real JFK conformance emitted `And so my fellow Americans ask not what your country can do for you, ask what you can do for your country.` with no byte-level markers; legacy parity emitted the same native text and scored 1.000 similarity; benchmark recorded cold load 1419 ms, warm decode 11672 ms, peak RSS 350284 KiB.
- Contract mismatches: none

## Waivers

- Self-validation rationale: User explicitly asked Codex to implement the
  retrospective improvements in the same session; verifier notes will record
  direct command evidence.

## Files to Add/Modify

- `src/asr/nativecpu/cpuwhispertokenizer.*` — user-facing token text decoding
- `src/asr/nativecpu/cpudecoderruntime.cpp` — assemble real-decoder transcripts from decoded token text
- `tests/*` — focused Conv1d, mel, tokenizer, and real JFK assertions
- `tests/CMakeLists.txt` — register new focused tests
- `docs/measurements/*` and `feature_records/*` — refresh evidence

## Testing Strategy

Run default no-legacy build/tests, no-legacy clang-tidy, legacy build/tests for
the parity lane, legacy clang-tidy or a concrete waiver if the legacy analyzer
target is not clean, real-weight JFK conformance, benchmark, release hygiene,
and contract checks.

## Open Questions

- none
