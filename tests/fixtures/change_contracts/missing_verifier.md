# Feature: Missing Verifier

## Motivation

Exercise responsibility validation.

## Proposed Behavior

The checker should reject plans that omit a verifier.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Ownership fields stay explicit.
- Must become true: Missing verifier fields fail closed.
- Success signals: The fixture is rejected by the checker.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: This fixture leaves the verifier field blank.

## Responsibilities

- Implementer: agent-implementer
- Verifier: 
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The fixture exercises the checker. | verifier_note=The checker should reject the missing verifier.
- Docs | impact=no | status=not_applicable | rationale=Docs are unaffected. | verifier_note=No docs lane applies.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are unaffected. | verifier_note=No analyzer lane applies.
- Install validation | impact=no | status=not_applicable | rationale=Install is unaffected. | verifier_note=No install lane applies.
- Release hygiene | impact=yes | status=passed | rationale=The checker participates in hygiene enforcement. | verifier_note=The fixture should fail during contract validation.

## Implementation Notes

- Owner: agent-implementer
- Status: planned
- Notes: The verifier field is intentionally left blank.

## Verification Notes

- Owner: 
- Status: pending
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: The checker should reject the missing verifier.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `feature_records/...` — fixture only

## Testing Strategy

Run the checker and expect failure.

## Open Questions

None.
