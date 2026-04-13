# Feature: Selective Multiplatform Backends

## Motivation

Mutterkey should not recreate an unbounded backend matrix, but the roadmap
still calls for a small, product-valuable set of non-CPU backend options once
the app-owned runtime contract is stable.

## Proposed Behavior

Add only the most valuable additional backends, with explicit parity and
selection rules:

- Apple platforms: Metal
- Windows: DirectML or ONNX Runtime DirectML
- Linux: CPU first, then Vulkan or NVIDIA only if profiling justifies it
- Android: CPU first, Vulkan later only if justified

Every backend must satisfy the same app-owned engine/model/session/event
contract and remain opt-in at build and package time.

## Lifecycle

- State: planned
- Supersedes: feature_records/superseded/fresh-asr-runtime-roadmap.md
- Superseded by: none

## Contract

- Must remain true: Backend breadth is driven by product value, not by matching upstream backend breadth.
- Must become true: Additional backends can be selected explicitly, observed in diagnostics, and fall back cleanly to CPU when unsupported.
- Success signals: Backend selection is explicit and observable, unsupported devices fall back cleanly, and official builds ship a controlled documented backend set.

## Uncertainty And Cost

- Product uncertainty: medium
- Technical uncertainty: high
- Implementation cost: high
- Validation cost: high
- Notes: This phase widens platform, packaging, parity, and diagnostic surfaces across multiple optional backends.

## Responsibilities

- Implementer: future-backend-owner
- Verifier: future-backend-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=waived | rationale=This is a planned future phase; parity and fallback tests belong to the implementation work rather than the planning artifact. | verifier_note=No backend parity lane exists yet for this future work.
- Docs | impact=yes | status=waived | rationale=This is a planned phase; docs should change only when backend support becomes real. | verifier_note=Docs should update only when backend support is real and packaged.
- Analyzers | impact=yes | status=waived | rationale=This is a planned phase; analyzer evidence will be gathered with future code changes. | verifier_note=Analyzer evidence remains pending.
- Install validation | impact=yes | status=waived | rationale=This is a planned phase; install validation belongs to future backend packaging work rather than the planning artifact. | verifier_note=Install validation will be required for any shipped backend expansion.
- Release hygiene | impact=yes | status=waived | rationale=This is a planned phase; release-hygiene evidence will be gathered once backend work lands. | verifier_note=This remains a future planning contract only.

## Implementation Notes

- Owner: future-backend-owner
- Status: planned
- Notes: This phase should start only after the native CPU path is good enough to serve as the product-owned parity reference.

## Verification Notes

- Owner: future-backend-verifier
- Status: pending
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: This is a future planning contract; implementation and parity evidence are still pending.
- Contract mismatches: All completion evidence remains pending by design.

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `src/asr/runtime/*` — backend selection and diagnostics
- backend-specific runtime integration surfaces as needed
- build, install, and release docs/workflows for supported backend packaging
- tests and parity/fallback coverage

## Testing Strategy

Completion should require backend parity coverage against CPU, deterministic
fallback tests, install-tree validation for shipped backend libraries, and
release-facing documentation checks.

## Open Questions

- Which non-CPU backends are actually worth the packaging and support burden for Mutterkey?
- Should backend expansion be release-channel-specific rather than part of every official build?
