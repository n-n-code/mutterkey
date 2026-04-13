# Feature: Missing Verification Commands

## Motivation

Exercise explicit verifier evidence requirements.

## Proposed Behavior

The checker should reject plans that omit verifier commands.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Verification stays a first-class artifact rather than an implied step.
- Must become true: Plans without concrete verifier commands fail closed.
- Success signals: The fixture is rejected by the checker.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: This fixture omits verifier commands on purpose.

## Responsibilities

- Implementer: agent-implementer
- Verifier: agent-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The checker behavior is under test. | verifier_note=The checker should reject the verification-notes section.
- Docs | impact=no | status=not_applicable | rationale=Docs are unaffected. | verifier_note=No docs lane applies.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are unaffected. | verifier_note=No analyzer lane applies.
- Install validation | impact=no | status=not_applicable | rationale=Install is unaffected. | verifier_note=No install lane applies.
- Release hygiene | impact=yes | status=passed | rationale=The checker participates in hygiene enforcement. | verifier_note=The fixture should fail during contract validation.

## Implementation Notes

- Owner: agent-implementer
- Status: planned
- Notes: The implementation side is present but verifier commands are omitted.

## Verification Notes

- Owner: agent-verifier
- Status: pending
- Observed result: The checker should reject the missing verifier commands.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `feature_records/...` — fixture only

## Testing Strategy

Run the checker and expect failure.

## Open Questions

None.
