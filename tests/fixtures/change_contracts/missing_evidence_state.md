# Feature: Missing Evidence State

## Motivation

Exercise the fail-closed evidence state rule.

## Proposed Behavior

The checker should reject `missing` evidence states.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Evidence requirements stay explicit.
- Must become true: Missing evidence states fail instead of being ignored.
- Success signals: The fixture is rejected by the checker.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: The tests evidence is intentionally marked missing.

## Responsibilities

- Implementer: agent-implementer
- Verifier: agent-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=missing | rationale=This intentionally fails the checker. | verifier_note=The checker should reject the missing status.
- Docs | impact=no | status=not_applicable | rationale=Docs are unaffected. | verifier_note=No docs lane applies.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are unaffected. | verifier_note=No analyzer lane applies.
- Install validation | impact=no | status=not_applicable | rationale=Install is unaffected. | verifier_note=No install lane applies.
- Release hygiene | impact=yes | status=passed | rationale=The checker participates in hygiene enforcement. | verifier_note=The fixture should fail during contract validation.

## Implementation Notes

- Owner: agent-implementer
- Status: planned
- Notes: The tests status is intentionally invalid.

## Verification Notes

- Owner: agent-verifier
- Status: pending
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: The checker should reject missing evidence.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `feature_records/...` — fixture only

## Testing Strategy

Run the checker and expect failure.

## Open Questions

None.
