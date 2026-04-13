# Feature: Waived Without Rationale

## Motivation

Exercise evidence waiver validation.

## Proposed Behavior

The checker should reject waived evidence without rationale.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Waivers require explicit justification.
- Must become true: A waived evidence state fails when rationale is missing.
- Success signals: The fixture is rejected by the checker.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: The docs evidence is intentionally waived without a rationale.

## Responsibilities

- Implementer: agent-implementer
- Verifier: agent-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The fixture exercises the checker. | verifier_note=The tests lane is not the failure point here.
- Docs | impact=yes | status=waived | rationale=none | verifier_note=The checker should reject the waived docs evidence.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are unaffected. | verifier_note=No analyzer lane applies.
- Install validation | impact=no | status=not_applicable | rationale=Install is unaffected. | verifier_note=No install lane applies.
- Release hygiene | impact=yes | status=passed | rationale=The checker participates in hygiene enforcement. | verifier_note=The fixture should fail during contract validation.

## Implementation Notes

- Owner: agent-implementer
- Status: planned
- Notes: The docs evidence is intentionally waived without rationale.

## Verification Notes

- Owner: agent-verifier
- Status: pending
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: The checker should reject the waived docs evidence.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `feature_records/...` — fixture only

## Testing Strategy

Run the checker and expect failure.

## Open Questions

None.
