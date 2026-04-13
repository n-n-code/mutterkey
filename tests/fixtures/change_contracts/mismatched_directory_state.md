# Feature: Mismatched Directory State

## Motivation

Fixtures should prove that a record's location matches its declared lifecycle.

## Proposed Behavior

The checker should reject a record placed in the wrong lifecycle directory.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Lifecycle state should be explicit and machine-checkable.
- Must become true: The checker should fail when a file path and lifecycle state disagree.
- Success signals: The fixture is rejected.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: This fixture isolates the directory-versus-state rule.

## Responsibilities

- Implementer: fixture-author
- Verifier: fixture-reviewer
- Approver: none

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=Fixture validation is the test surface. | verifier_note=Fixture only.
- Docs | impact=no | status=not_applicable | rationale=Docs are out of scope for this fixture. | verifier_note=Fixture only.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are out of scope for this fixture. | verifier_note=Fixture only.
- Install validation | impact=no | status=not_applicable | rationale=Install validation is out of scope for this fixture. | verifier_note=Fixture only.
- Release hygiene | impact=no | status=not_applicable | rationale=Release hygiene is out of scope for this fixture. | verifier_note=Fixture only.

## Implementation Notes

- Owner: fixture-author
- Status: completed
- Notes: Fixture only.

## Verification Notes

- Owner: fixture-reviewer
- Status: completed
- Commands: fixture only
- Observed result: Should fail because the record is intended to be placed under `planned/` while declaring `active`.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `feature_records/...` — fixture only

## Testing Strategy

Run the checker fixture suite.

## Open Questions

None.
