# Feature: Native CPU Byte-Level Tokenizer And Benchmark Evidence

## Motivation

The prior transcript-cleanup pass shipped a narrow fix that decoded only two
byte-level BPE markers (`Ġ` / `Ċ`) before tokens reached users. That is
correct for ASCII English on the JFK lane, but punctuation and non-ASCII
tokens in the Whisper vocabulary are stored as further GPT-2 byte-level
codepoints (e.g. `Ã` + `©` for UTF-8 `é`), so they still pass through
unchanged. Future multilingual paths and punctuation-heavy utterances have no
unit-level protection.

The same pass also left committed benchmark evidence with single-run
min/median/p95 numbers (`--runs 1`) and machine-specific absolute paths that
had to be manually sanitized before `scripts/check-release-hygiene.sh`
accepted the doc.

## Proposed Behavior

- `decodeCpuWhisperTokenText` implements the full inverse of GPT-2's
  `bytes_to_unicode` bijection: every Whisper byte-level codepoint round-trips
  to the correct raw byte, and the resulting byte sequence is decoded as UTF-8
  so punctuation and non-ASCII characters reach users correctly.
- Unit tests lock the whitespace, ASCII punctuation, Latin-1, UTF-8 multi-byte,
  and mixed-marker round trips against the mathematical `bytes_to_unicode`
  specification.
- The legacy parity test drops its manual `Ġ` / `Ċ` normalization because the
  decoder now produces clean text; the `<|endoftext|>` sentinel handling
  remains.
- `nativecpubench` accepts a `--repo-relative-paths` flag that emits the
  `--package` / `--audio` arguments into its JSON verbatim, so committed
  benchmark evidence no longer needs manual path scrubbing.
- `docs/measurements/2026-04-native-cpu-base-en.md` is regenerated with
  `--warmup 1 --runs 5 --repo-relative-paths`, so the committed min/median/p95
  distribution reflects actual variance rather than a single run.

## Lifecycle

- State: done
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Legacy whisper.cpp parity similarity stays at the accepted
  threshold on the JFK lane; the no-legacy and legacy-enabled build flavors
  continue to build and pass their existing test suites; the public
  `decodeCpuWhisperTokenText(QStringView) -> QString` signature is unchanged.
- Must become true: Non-ASCII and punctuation byte-level tokens round-trip to
  correct UTF-8 user text; the committed benchmark doc carries real
  distribution values and repo-relative paths without manual sanitization.
- Success signals: New tokenizer tests pass deterministically; real JFK
  conformance still emits clean English text; parity similarity remains 1.000;
  `check-release-hygiene.sh` passes on the regenerated benchmark without
  manual edits.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: medium
- Notes: The inverse `bytes_to_unicode` mapping is mathematically defined, so
  there is no fixture generation risk; validation still needs a real Release
  build and legacy parity lane.

## Responsibilities

- Implementer: claude
- Verifier: claude
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=Added deterministic coverage for the full inverse `bytes_to_unicode` mapping and verified end-to-end real-weight conformance plus legacy parity. | verifier_note=Focused ctest run targeting cpuwhispertokenizertest, cpumelspectrogramtest, and cputensortest passed 3/3; full no-legacy `ctest` passed 20/20; Release `cpurealdecodertest` passed 7/7 with clean JFK transcript; legacy-enabled Release `cpuwhisperparitytest` passed 3/3 with similarity 1.000 after dropping the dead marker replacement.
- Docs | impact=yes | status=passed | rationale=Benchmark measurement doc regenerated with real 5-run distribution and repo-relative paths. | verifier_note=`docs/measurements/2026-04-native-cpu-base-en.md` refreshed with cold load 1457 ms, warm decode min/median/p95 = 11659/11715/11772 ms, peak RSS 352092 KiB, and verbatim `third_party/whisper.cpp/samples/jfk.wav` audio path.
- Analyzers | impact=yes | status=passed | rationale=C++ changes in `src/asr/nativecpu/cpuwhispertokenizer.cpp`, `tools/nativecpubench/main.cpp`, and two test translation units are covered by clang-tidy in both build flavors. | verifier_note=`cmake --build <no-legacy-build> --target clang-tidy` passed after replacing a hand-rolled `table.fill(-1)` loop flagged by `modernize-loop-convert`; `cmake --build <legacy-build> --target clang-tidy` passed.
- Install validation | impact=no | status=not_applicable | rationale=No install layout, shipped asset, or service behavior changes. | verifier_note=Not required for this change.
- Release hygiene | impact=yes | status=passed | rationale=Regenerated measurement doc and updated feature record are free of user-home absolute path literals after rewording. | verifier_note=`bash scripts/check-release-hygiene.sh` passed for repo-owned files; the one remaining match lives in untracked, gitignored `.claude/settings.local.json` cached Claude Code permission rules and is outside repo-owned scope. `bash scripts/check-change-contracts.sh` passed once evidence lanes were filled.

## Implementation Notes

- Owner: claude
- Status: completed
- Notes: Replaced the two-character `decodeCpuWhisperTokenText` body with a
  constexpr inverse GPT-2 `bytes_to_unicode` lookup plus UTF-8 reassembly;
  added ASCII-punctuation, UTF-8 multi-byte, mixed-marker, and out-of-range
  coverage to `cpuwhispertokenizertest.cpp`; removed the dead U+0120 / U+010A
  pre-normalization from `cpuwhisperparitytest.cpp`; added
  `--repo-relative-paths` to `tools/nativecpubench/main.cpp` so JSON path
  emission uses the raw argv string when requested.

## Verification Notes

- Owner: claude
- Status: completed
- Commands: `cmake -S . -B /tmp/mutterkey-build-2cpcDv -G Ninja -DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF`; `cmake --build /tmp/mutterkey-build-2cpcDv -j"$(nproc)"`; `ctest --test-dir /tmp/mutterkey-build-2cpcDv -R 'cpuwhispertokenizertest|cpumelspectrogramtest|cputensortest' --output-on-failure`; `ctest --test-dir /tmp/mutterkey-build-2cpcDv --output-on-failure`; `cmake --build /tmp/mutterkey-build-codex-release -j"$(nproc)"`; `MUTTERKEY_TEST_WEIGHTS_PATH=$STAGING_DIR/base-en.mkweights MUTTERKEY_TEST_PACKAGE_PATH=/tmp/mutterkey-packages/whisper-base-en QT_QPA_PLATFORM=offscreen /tmp/mutterkey-build-codex-release/tests/cpurealdecodertest`; `cmake --build /tmp/mutterkey-build-codex-legacy-release -j"$(nproc)"`; `MUTTERKEY_TEST_PACKAGE_PATH=/tmp/mutterkey-packages/whisper-base-en MUTTERKEY_TEST_LEGACY_WEIGHTS_PATH=$STAGING_DIR/ggml-base.en.bin QT_QPA_PLATFORM=offscreen /tmp/mutterkey-build-codex-legacy-release/tests/cpuwhisperparitytest`; `cmake --build /tmp/mutterkey-build-2cpcDv --target clang-tidy`; `cmake --build /tmp/mutterkey-build-codex-legacy-release --target clang-tidy`; `/tmp/mutterkey-build-codex-release/tools/nativecpubench/nativecpubench --package /tmp/mutterkey-packages/whisper-base-en --audio third_party/whisper.cpp/samples/jfk.wav --warmup 1 --runs 5 --max-tokens 96 --max-mel-frames 1200 --repo-relative-paths`; `bash scripts/check-release-hygiene.sh`; `bash scripts/check-change-contracts.sh`.
- Observed result: Focused tests passed 3/3; full no-legacy CTest passed 20/20; Release real JFK conformance emitted `And so my fellow Americans ask not what your country can do for you, ask what you can do for your country.` with no byte-level markers; legacy parity printed the same native text and scored similarity 1.000; no-legacy clang-tidy passed after switching the byte-table initializer loop to `std::array::fill`; legacy-enabled clang-tidy passed unchanged; benchmark recorded cold load 1457 ms, warm decode min/median/p95 = 11659/11715/11772 ms, peak RSS 352092 KiB with repo-relative audio path; `check-change-contracts.sh` passed after evidence lanes were filled; `check-release-hygiene.sh` passes for repo-owned files (the remaining user-home absolute path match lives only in untracked, gitignored `.claude/settings.local.json`).
- Contract mismatches: none

## Waivers

- Self-validation rationale: User explicitly approved a self-implemented
  follow-up pass continuing from the 2026-04-17 transcript-cleanup record;
  verifier notes will record direct command evidence.

## Files to Add/Modify

- `src/asr/nativecpu/cpuwhispertokenizer.cpp` — replace the two-character
  replacement in `decodeCpuWhisperTokenText` with the full GPT-2
  `bytes_to_unicode` reverse table; return UTF-8-decoded text.
- `tests/cpuwhispertokenizertest.cpp` — add deterministic coverage for ASCII
  punctuation, Latin-1 pass-through, UTF-8 multi-byte (`é`, `ñ`), mixed marker
  + multi-byte, and out-of-range codepoints.
- `tests/cpuwhisperparitytest.cpp` — drop manual U+0120 / U+010A replacement;
  keep `<|endoftext|>` sentinel handling.
- `tools/nativecpubench/main.cpp` — add `--repo-relative-paths` flag; when
  set, emit `--package` and `--audio` argv strings verbatim into JSON instead
  of `QFileInfo::absoluteFilePath()`.
- `docs/measurements/2026-04-native-cpu-base-en.md` — regenerate with
  `--warmup 1 --runs 5 --repo-relative-paths`; reflect real distribution
  values.

## Testing Strategy

Run the no-legacy code validation profile plus release hygiene:

- No-legacy Debug build + full `ctest`, with a focused pre-pass on the new
  `cpuwhispertokenizertest` cases.
- No-legacy clang-tidy.
- Release no-legacy build + env-gated `cpurealdecodertest` to confirm raw
  transcript stays clean.
- Legacy-enabled Release build + env-gated `cpuwhisperparitytest`; expect
  similarity 1.000 and no byte-level markers in native text.
- Legacy-enabled clang-tidy covering the parity test translation unit.
- Regenerated benchmark run feeding the refreshed measurement doc.
- `bash scripts/check-release-hygiene.sh` and
  `bash scripts/check-change-contracts.sh` as final gates.

## Open Questions

- none
