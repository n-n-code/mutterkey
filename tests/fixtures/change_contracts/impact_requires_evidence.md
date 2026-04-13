# Feature: Impact Requires Evidence

## Motivation

Exercise impact-driven evidence validation.

## Proposed Behavior

The checker should reject `not_applicable` evidence when the corresponding impact is declared as `yes`.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Impact declarations must affect required proof.
- Must become true: Declared impact makes the corresponding evidence lane mandatory.
- Success signals: The fixture is rejected by the checker.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: This fixture only checks the impact-to-evidence rule.

## Responsibilities

- Implementer: agent-implementer
- Verifier: agent-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The checker behavior is under test. | verifier_note=The tests lane is not the failure point here.
- Docs | impact=yes | status=not_applicable | rationale=This intentionally violates the impact rule. | verifier_note=The checker should reject the docs lane.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are unaffected. | verifier_note=No analyzer lane applies.
- Install validation | impact=no | status=not_applicable | rationale=Install is unaffected. | verifier_note=No install lane applies.
- Release hygiene | impact=yes | status=passed | rationale=The checker participates in hygiene enforcement. | verifier_note=The fixture should fail during contract validation.

## Implementation Notes

- Owner: agent-implementer
- Status: planned
- Notes: The docs lane is intentionally inconsistent.

## Verification Notes

- Owner: agent-verifier
- Status: pending
- Commands: `bash scripts/check-change-contracts.sh`
- Observed result: The checker should reject the docs evidence state.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `feature_records/...` — fixture only

## Testing Strategy

Run the checker and expect failure.

## Open Questions

None.
