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

- State: active
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

- Tests | impact=yes | status=passed | rationale=The current implementation slice adds default synthetic MKCPUR3 load coverage, a real dimension-mismatch rejection case that no longer depends on external weights, and metadata-driven real-decoder no-speech token coverage. | verifier_note=Validated with `/tmp/mutterkey-build-codex/tests/cpurealdecodertest`, `/tmp/mutterkey-build-codex/tests/cpudecoderruntimetest`, and `ctest --test-dir /tmp/mutterkey-build-codex --output-on-failure`; real-weight pipeline coverage still requires `MUTTERKEY_TEST_WEIGHTS_PATH` and parity/measurement lanes remain open for phase completion.
- Docs | impact=yes | status=waived | rationale=README and AGENTS should be updated only after native CPU user-facing claims materially change. | verifier_note=No user-facing runtime claim changed in this slice; this feature record was updated with implementation and verification evidence.
- Analyzers | impact=yes | status=passed | rationale=The touched C++ runtime and test sources are covered by the configured no-legacy clang-tidy target; Clazy is not material for this non-Widget/non-QObject runtime slice. | verifier_note=Validated with `cmake --build /tmp/mutterkey-build-codex --target clang-tidy`; legacy whisper files were skipped because they are not part of the no-legacy compile database.
- Install validation | impact=no | status=not_applicable | rationale=This phase is primarily runtime correctness and measurement work. | verifier_note=Install validation becomes relevant only if shipped assets or install layout change.
- Release hygiene | impact=yes | status=passed | rationale=This slice changed repo-owned tests and the active feature record, so release hygiene and contract checks were rerun even though shipped runtime assets did not change. | verifier_note=Validated with `bash scripts/check-release-hygiene.sh` and `bash scripts/check-change-contracts.sh`.

## Implementation Notes

- Owner: native-runtime-owner
- Status: in_progress
- Notes: RealDecoderV3, MKCPUR3, mel/frontend, encoder, decoder, KV cache, and greedy search are already in place. 2026-04-13 slice: real decoder search now consumes package execution metadata for no-speech and timestamp token IDs, clamps generated tokens to the packaged text context, and has default synthetic V3 load/dimension coverage that is not hidden behind external real-weight staging. The remaining gates are real-model validation, weight conversion, broader safety checks, parity, and performance work.

## Verification Notes

- Owner: runtime-verifier
- Status: in_progress
- Commands: `cmake -S . -B /tmp/mutterkey-build-codex -G Ninja -DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF`; `cmake --build /tmp/mutterkey-build-codex -j8`; `/tmp/mutterkey-build-codex/tests/cpurealdecodertest`; `/tmp/mutterkey-build-codex/tests/cpudecoderruntimetest`; `ctest --test-dir /tmp/mutterkey-build-codex --output-on-failure`; `cmake --build /tmp/mutterkey-build-codex --target clang-tidy`; `QT_QPA_PLATFORM=offscreen /tmp/mutterkey-build-codex/mutterkey --help`; `bash scripts/check-release-hygiene.sh`; `bash scripts/check-change-contracts.sh`
- Observed result: Native-only configure/build passed; touched decoder tests passed; full CTest passed 18/18; `cpurealdecodertest` now runs the synthetic V3 load and dimension-mismatch cases by default and skips only the external real-weight pipeline when `MUTTERKEY_TEST_WEIGHTS_PATH` is unset; clang-tidy passed on the no-legacy compile database and skipped legacy-only whisper sources as expected; headless CLI help, release hygiene, and contract checks passed.
- Contract mismatches: Real external-weight decoding, whisper.cpp parity, and latency/memory measurement evidence remain open for overall Phase 5B completion.

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `src/asr/nativecpu/*` — decoder execution, model loading, tokenizer, timestamps, and performance work
- `src/asr/model/*` — native model validation and converter-facing metadata rules
- `tests/*` — end-to-end native CPU integration, conformance, and parity coverage
- `tools/*` or `scripts/*` — MKCPUR3 conversion/export tooling if the repo owns it
- `README.md` and release-facing docs when native user-facing claims change

## Remaining Work

The current slice improves native decoder ownership and default synthetic
coverage, but this record should stay active until the following gates are
closed:

- Produce or document the MKCPUR3 conversion/export path for the chosen baseline
  model family, including tokenizer vocabulary, tokenizer merge assets, manifest
  native-execution metadata, and package validation steps.
- Validate RealDecoderV3 end-to-end with real baseline-family weights rather
  than only synthetic tensors or fixture/template scaffolds.
- Add a short-utterance conformance lane with representative audio samples,
  expected transcript behavior, and explicit handling for any artifact that is
  too large or licensed too restrictively to commit.
- Run and record a parity lane against the legacy `whisper.cpp` runtime while
  the legacy adapter still exists, including acceptable transcript/timestamp
  deltas and any known divergences.
- Measure and record cold model load, warm decode latency, and memory use for
  the baseline model on a representative machine.
- Continue moving decoder prompt/search assumptions into app-owned package
  metadata or typed runtime config, especially remaining hardcoded
  Whisper-family prompt tokens beyond the no-speech and timestamp IDs covered
  by the latest slice.
- Resolve the real-weight pipeline skip in `cpurealdecodertest` by either
  providing a documented local artifact lane or adding a deterministic
  repository-safe test artifact strategy.
- Update README, release checklist, and API docs only when the native CPU path
  is validated enough to change user-facing runtime claims.

## Testing Strategy

Completion should require:

- end-to-end native V3 integration coverage with a real or synthetic model
- tensor-dimension validation tests for malformed files
- short-utterance conformance corpus coverage
- parity comparison lane against `whisper.cpp` where parity remains part of migration
- measured cold start, warm start, and short-utterance latency
- normal repo validation for touched files and docs

## Open Questions

- Should the MKCPUR3 converter live in-repo under `tools/` or remain an external staging/export step?
- What is the smallest distributable or synthetic V3 model artifact that gives meaningful end-to-end coverage without bloating the repo?
