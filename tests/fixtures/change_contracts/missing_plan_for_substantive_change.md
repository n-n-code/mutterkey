# Feature: Missing Plan For Substantive Change

## Motivation

Exercise the substantive-change gate.

## Proposed Behavior

The checker should reject substantive repo-owned changes that do not update a non-template plan file.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Substantive changes require explicit planning.
- Must become true: A repo-owned change without a plan update fails closed.
- Success signals: The checker is invoked with only a substantive changed file and fails.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: This fixture exists only to verify the change-presence gate.

## Responsibilities

- Implementer: agent-implementer
- Verifier: agent-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The checker behavior is under test. | verifier_note=The plan-presence gate should fail before normal validation matters.
- Docs | impact=no | status=not_applicable | rationale=Docs are unaffected. | verifier_note=No docs lane applies.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are unaffected. | verifier_note=No analyzer lane applies.
- Install validation | impact=no | status=not_applicable | rationale=Install is unaffected. | verifier_note=No install lane applies.
- Release hygiene | impact=yes | status=passed | rationale=The checker participates in hygiene enforcement. | verifier_note=The plan-presence gate should fail before normal validation matters.

## Implementation Notes

- Owner: agent-implementer
- Status: planned
- Notes: This plan should not count because the simulated change list omits any plan path.

## Verification Notes

- Owner: agent-verifier
- Status: pending
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: The checker should fail before this file can satisfy the plan-update requirement.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `feature_records/...` — fixture only

## Testing Strategy

Run the checker with a changed-files list that includes only a substantive
repo-owned path such as `src/asr/runtime/transcriptionengine.cpp`.

## Open Questions

None.
