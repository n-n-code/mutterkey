# Feature: Productize Config, Telemetry, And Distribution

## Motivation

Even with a good runtime core, Mutterkey still needs product-shaped config,
better diagnostics, and predictable distribution rules so the runtime is easy
to operate and ship.

## Proposed Behavior

Make the runtime easier to operate, diagnose, and package by:

- replacing backend-flavored runtime config with product-shaped config
- mapping product settings to backend-specific tuning internally
- adding structured diagnostics for backend choice, selection reason, load
  time, memory usage, warmup state, and transcription latency
- keeping runtime artifact layout small and predictable for packaging
- ensuring model and backend loading paths are safe and deterministic

## Lifecycle

- State: planned
- Supersedes: feature_records/superseded/fresh-asr-runtime-roadmap.md
- Superseded by: none

## Contract

- Must remain true: User-facing config should not widen backend-specific assumptions prematurely.
- Must become true: User-facing config is product-shaped, diagnostics are sufficient for backend/model troubleshooting, and release packaging includes only intended runtime libraries and assets.
- Success signals: User-facing config no longer exposes backend-specific assumptions by default, release packaging remains intentional, and diagnostics are sufficient to debug backend selection and model-load failures.

## Uncertainty And Cost

- Product uncertainty: medium
- Technical uncertainty: medium
- Implementation cost: medium
- Validation cost: medium
- Notes: This phase spans config semantics, diagnostics surfaces, and release/distribution alignment rather than only one subsystem.

## Responsibilities

- Implementer: future-productization-owner
- Verifier: future-productization-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=waived | rationale=This is a planned phase; deterministic coverage belongs to future implementation work rather than the planning artifact. | verifier_note=No completion evidence exists yet.
- Docs | impact=yes | status=waived | rationale=This is a planned phase; coordinated docs updates belong to the future implementation rather than the planning artifact. | verifier_note=Docs updates are part of the eventual completion contract.
- Analyzers | impact=yes | status=waived | rationale=This is a planned phase; analyzer evidence will be gathered with future code changes. | verifier_note=Analyzer evidence remains pending.
- Install validation | impact=yes | status=waived | rationale=This is a planned phase; install-tree evidence belongs to the future distribution work rather than the planning artifact. | verifier_note=Install-tree checks will be required for completion.
- Release hygiene | impact=yes | status=waived | rationale=This is a planned phase; release-hygiene evidence will be gathered once implementation begins. | verifier_note=Release hygiene and release-checklist alignment remain pending.

## Implementation Notes

- Owner: future-productization-owner
- Status: planned
- Notes: This phase should start only after the native runtime is good enough that changing config semantics and diagnostics is worthwhile.

## Verification Notes

- Owner: future-productization-verifier
- Status: pending
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: This is a planning contract only; no implementation evidence exists yet.
- Contract mismatches: All completion evidence remains pending by design.

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `src/config.*` and runtime diagnostics surfaces
- release and install documentation
- install rules, workflows, and packaging helpers if distribution shape changes
- deterministic tests for config, diagnostics, and packaging expectations

## Testing Strategy

Completion should require deterministic config and diagnostics coverage,
install-tree validation, and aligned release-facing docs/workflows.

## Open Questions

- Which diagnostics belong in the always-on user-facing path versus only in `diagnose` or internal runtime surfaces?
- How far should product-shaped config go before the backend set is more stable?
