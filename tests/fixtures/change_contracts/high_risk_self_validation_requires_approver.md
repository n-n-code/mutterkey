# Feature: High Risk Self Validation Requires Approver

## Motivation

Exercise the bounded self-validation rule.

## Proposed Behavior

The checker should reject higher-risk self-validation when no approver is recorded.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Self-validation stays constrained rather than becoming the default.
- Must become true: Higher-risk self-validation requires stronger governance.
- Success signals: The fixture is rejected by the checker.

## Uncertainty And Cost

- Product uncertainty: medium
- Technical uncertainty: high
- Implementation cost: medium
- Validation cost: medium
- Notes: The risk bands intentionally exceed the low-risk self-validation path.

## Responsibilities

- Implementer: same-agent
- Verifier: same-agent
- Approver: 

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The checker behavior is under test. | verifier_note=The fixture should fail because higher-risk self-validation lacks an approver.
- Docs | impact=no | status=not_applicable | rationale=Docs are unaffected. | verifier_note=No docs lane applies.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are unaffected. | verifier_note=No analyzer lane applies.
- Install validation | impact=no | status=not_applicable | rationale=Install is unaffected. | verifier_note=No install lane applies.
- Release hygiene | impact=yes | status=passed | rationale=The checker participates in hygiene enforcement. | verifier_note=The fixture should fail during contract validation.

## Implementation Notes

- Owner: same-agent
- Status: completed
- Notes: The shared ownership is intentional for this failure fixture.

## Verification Notes

- Owner: same-agent
- Status: completed
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: The checker should reject the missing approver for higher-risk self-validation.
- Contract mismatches: none

## Waivers

- Self-validation rationale: The actor is intentionally the same to exercise the higher-risk path.

## Files to Add/Modify

- `feature_records/...` — fixture only

## Testing Strategy

Run the checker and expect failure.

## Open Questions

None.
