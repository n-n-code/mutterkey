# Feature: Translation And Language Expansion

## Motivation

Translation and multilingual support were deferred intentionally so the first
native CPU slice could focus on ASR correctness and ownership. That work still
needs an explicit contract plan instead of only a future-phase note.

## Proposed Behavior

After the real CPU decoder path is stable, expand native support beyond the
initial English-only floor by:

- widening supported languages for the native runtime
- defining translation-mode behavior in the native runtime
- adding multilingual and translation conformance coverage
- making capability reporting truthful for language and translation support

## Lifecycle

- State: planned
- Supersedes: feature_records/superseded/fresh-asr-runtime-roadmap.md
- Superseded by: none

## Contract

- Must remain true: Translation is not widened accidentally before native ASR correctness is defensible.
- Must become true: The native backend reports truthful language and translation capabilities and supports multilingual or translation flows for explicitly supported models.
- Success signals: Translation requests no longer fail with `InvalidConfig` on supported models, and multilingual/translation behavior is covered by deterministic tests.

## Uncertainty And Cost

- Product uncertainty: medium
- Technical uncertainty: medium
- Implementation cost: medium
- Validation cost: medium
- Notes: The native decoder and package/runtime contracts will widen materially, but only after Phase 5B is stable.

## Responsibilities

- Implementer: future-native-runtime-owner
- Verifier: future-runtime-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=waived | rationale=This is a planned follow-on phase; deterministic multilingual and translation coverage belongs to the future implementation rather than the planning artifact itself. | verifier_note=No such coverage exists yet.
- Docs | impact=yes | status=waived | rationale=This is a planned phase; docs should update only when capabilities become real rather than during planning. | verifier_note=User-facing docs should change only when support is truthful.
- Analyzers | impact=yes | status=waived | rationale=This is a planned phase; analyzer evidence will be collected with future code changes rather than in the planning artifact. | verifier_note=Analyzer evidence will be required once implementation begins.
- Install validation | impact=no | status=not_applicable | rationale=This phase is expected to be behavior and capability work rather than install-layout work. | verifier_note=Install validation matters only if shipped assets change.
- Release hygiene | impact=yes | status=waived | rationale=This is a planned phase; release-hygiene evidence belongs to the future implementation changes. | verifier_note=This plan is intentionally still at the planning stage.

## Implementation Notes

- Owner: future-native-runtime-owner
- Status: planned
- Notes: This work stays deferred until the native CPU decoder is real, stable, and measured.

## Verification Notes

- Owner: future-runtime-verifier
- Status: pending
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: This is a planning contract only; implementation evidence is still pending.
- Contract mismatches: All completion evidence remains pending by design.

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `src/asr/nativecpu/*` — language and translation behavior
- `src/asr/runtime/*` — truthful capability reporting and config/runtime mapping
- `tests/*` — multilingual and translation conformance coverage
- `README.md` and diagnostics-facing docs when support becomes real

## Testing Strategy

Completion should require deterministic multilingual and translation fixtures,
truthful capability diagnostics checks, and normal repo validation for touched
runtime, docs, and analyzer surfaces.

## Open Questions

- Which model families and languages are worth first-class support for Mutterkey's actual workflow?
- Should translation support remain model-family-specific rather than runtime-wide?
