# Feature: Self Validation Without Waiver

## Motivation

Exercise the self-validation guardrail.

## Proposed Behavior

The checker should reject plans where the implementer and verifier are the same without rationale.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Self-validation stays visible rather than silent.
- Must become true: Matching implementer/verifier requires a waiver.
- Success signals: The fixture is rejected by the checker.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: The only invalid field is the missing self-validation rationale.

## Responsibilities

- Implementer: same-agent
- Verifier: same-agent
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The fixture exercises the checker. | verifier_note=The checker should reject the missing self-validation waiver.
- Docs | impact=no | status=not_applicable | rationale=Docs are unaffected. | verifier_note=No docs lane applies.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are unaffected. | verifier_note=No analyzer lane applies.
- Install validation | impact=no | status=not_applicable | rationale=Install is unaffected. | verifier_note=No install lane applies.
- Release hygiene | impact=yes | status=passed | rationale=The checker participates in hygiene enforcement. | verifier_note=The fixture should fail during contract validation.

## Implementation Notes

- Owner: same-agent
- Status: planned
- Notes: The owner match is intentional for this failure fixture.

## Verification Notes

- Owner: same-agent
- Status: pending
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: The checker should reject the missing waiver.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `feature_records/...` — fixture only

## Testing Strategy

Run the checker and expect failure.

## Open Questions

None.
