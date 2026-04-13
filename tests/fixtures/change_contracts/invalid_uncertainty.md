# Feature: Invalid Uncertainty

## Motivation

Exercise enum validation for uncertainty fields.

## Proposed Behavior

The checker should reject unsupported uncertainty values.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Validation stays deterministic.
- Must become true: Unsupported enum values fail closed.
- Success signals: The fixture is rejected by the checker.

## Uncertainty And Cost

- Product uncertainty: unknown
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: Only the product uncertainty value is intentionally invalid.

## Responsibilities

- Implementer: agent-implementer
- Verifier: agent-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The fixture exercises the checker. | verifier_note=The fixture should fail on uncertainty parsing.
- Docs | impact=no | status=not_applicable | rationale=Docs are unaffected. | verifier_note=No docs lane applies.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are unaffected. | verifier_note=No analyzer lane applies.
- Install validation | impact=no | status=not_applicable | rationale=Install is unaffected. | verifier_note=No install lane applies.
- Release hygiene | impact=yes | status=passed | rationale=The checker participates in hygiene enforcement. | verifier_note=The fixture should fail during contract validation.

## Implementation Notes

- Owner: agent-implementer
- Status: planned
- Notes: The invalid enum value is intentional.

## Verification Notes

- Owner: agent-verifier
- Status: pending
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: The checker should reject the unsupported uncertainty value.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `feature_records/...` — fixture only

## Testing Strategy

Run the checker and expect failure.

## Open Questions

None.
