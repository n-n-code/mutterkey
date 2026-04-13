# Feature: Fresh ASR Runtime Roadmap Migration

## Motivation

`feature_records/` now has an enforced change-contract workflow. The old ASR
runtime roadmap carried valuable phase history and future direction, but it was
not shaped as a collection of contract-bearing plans.

## Proposed Behavior

The old monolithic roadmap is replaced by a set of phase-oriented contract
plans:

- `2026-04-01-real-cpu-decoder-bring-up.md`
- `2026-04-01-translation-and-language-expansion.md`
- `2026-04-01-selective-multiplatform-backends.md`
- `2026-04-01-productize-config-telemetry-distribution.md`

This file remains only as the superseded migration pointer.

## Lifecycle

- State: superseded
- Supersedes: none
- Superseded by: feature_records/active/2026-04-01-real-cpu-decoder-bring-up.md, feature_records/planned/2026-04-01-translation-and-language-expansion.md, feature_records/planned/2026-04-01-selective-multiplatform-backends.md, feature_records/planned/2026-04-01-productize-config-telemetry-distribution.md

## Contract

- Must remain true: The repo keeps the ASR runtime history, current status, and next steps that were recorded in the original roadmap.
- Must become true: ASR planning now lives as a collection of contract plans that the repo checker can validate.
- Success signals: The historical and future phases are represented by non-template `# Feature:` plans and this file only points to them.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: This is a documentation and planning-structure migration rather than a runtime behavior change.

## Responsibilities

- Implementer: codex
- Verifier: codex-review-pass
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=no | status=not_applicable | rationale=No runtime or test behavior changed. | verifier_note=No C++ test lane was required for the planning migration itself.
- Docs | impact=yes | status=passed | rationale=The roadmap moved into contract plans under `feature_records/`. | verifier_note=Reviewed that the new files preserve phase intent and current status.
- Analyzers | impact=no | status=not_applicable | rationale=No analyzer-sensitive source changed. | verifier_note=No analyzer lane was required.
- Install validation | impact=no | status=not_applicable | rationale=Install layout did not change. | verifier_note=Install validation stayed out of scope.
- Release hygiene | impact=yes | status=passed | rationale=The repo's planning contract changed structurally. | verifier_note=Validated with `bash scripts/check-change-contracts.sh` after the migration.

## Implementation Notes

- Owner: codex
- Status: completed
- Notes: Replaced the monolithic roadmap with a superseded pointer and new phase-specific contract plans.

## Verification Notes

- Owner: codex-review-pass
- Status: completed
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: The migrated planning set passed the contract checker.
- Contract mismatches: none

## Waivers

- Self-validation rationale: Separate implementation and verification notes were recorded in one session because this documentation migration was executed by one agent.

## Files to Add/Modify

- `feature_records/superseded/fresh-asr-runtime-roadmap.md` — convert the old roadmap into a superseded pointer
- `feature_records/<state>/*.md` — add phase-specific contract plans

## Testing Strategy

Run the change-contract checker after creating the new phase plans.

## Open Questions

Whether later ASR sub-plans should split Phase 5B further once the weight
converter, conformance corpus, and performance work start moving
independently.
