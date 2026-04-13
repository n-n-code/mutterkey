# Feature: Valid Contract

## Motivation

Keep feature planning explicit and enforceable.

## Proposed Behavior

Planned changes must declare ownership, risk, lifecycle, and evidence lanes before implementation.

## Lifecycle

- State: active
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: The repo stays portable and validates with local scripts.
- Must become true: Feature plans use explicit contract and evidence fields.
- Success signals: The checker passes and required states are visible in review.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: medium
- Implementation cost: medium
- Validation cost: low
- Notes: The workflow is new, but the enforcement is limited to template structure.

## Responsibilities

- Implementer: agent-implementer
- Verifier: agent-verifier
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=Tests are affected by the workflow change. | verifier_note=Validated through the checker fixture suite.
- Docs | impact=yes | status=passed | rationale=Docs changed with the workflow. | verifier_note=README and AGENTS guidance were reviewed.
- Analyzers | impact=no | status=not_applicable | rationale=No analyzer-sensitive source change was made. | verifier_note=Analyzer lanes stayed out of scope.
- Install validation | impact=no | status=not_applicable | rationale=Install layout did not change. | verifier_note=Install validation was intentionally skipped.
- Release hygiene | impact=yes | status=passed | rationale=Release-facing workflow changed. | verifier_note=Release hygiene and contract checks were executed.

## Implementation Notes

- Owner: agent-implementer
- Status: completed
- Notes: The contract schema and checker were updated together.

## Verification Notes

- Owner: agent-verifier
- Status: completed
- Commands: `bash scripts/test-change-contracts.sh`; `bash scripts/check-change-contracts.sh`
- Observed result: The fixture suite and checker both passed.
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `scripts/...` — change-contract checker and fixture runner
- `feature_records/...` — stricter template contract
- `README.md` — describe the workflow

## Testing Strategy

Run the checker on valid and invalid fixtures and include it in release hygiene.

## Open Questions

Whether future slices should require evidence truth-checking instead of declaration-only enforcement.
