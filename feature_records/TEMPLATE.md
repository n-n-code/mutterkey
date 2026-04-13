# Feature: <name>

This template is enforced by `config/change-contract-policy.sh` and `scripts/check-change-contracts.sh`.

Store each completed record under the lifecycle directory that matches its
`## Lifecycle` `State`.
Prefer `bash scripts/set-feature-record-lifecycle.sh <record> <state>` when
changing lifecycle folders after a record already exists.

## Motivation

Why this feature is needed and what problem it solves.

## Proposed Behavior

What the feature does from the user's perspective. Include examples if helpful.

## Lifecycle

- State: planned|active|superseded|done
- Supersedes: none|<older plan path or identifier>
- Superseded by: none|<newer plan path or identifier>

## Contract

- Must remain true: <invariants, compatibility promises, or safety rails>
- Must become true: <new externally visible behavior or maintained contract>
- Success signals: <observable proof that the contract is satisfied>

## Uncertainty And Cost

- Product uncertainty: low|medium|high
- Technical uncertainty: low|medium|high
- Implementation cost: low|medium|high
- Validation cost: low|medium|high
- Notes: <short rationale for the declared bands>

## Responsibilities

- Implementer: <name, role, or agent identifier>
- Verifier: <name, role, or agent identifier>
- Approver: <optional name, role, or agent identifier>

## Evidence Matrix

- Tests | impact=yes|no | status=passed|waived|not_applicable|missing | rationale=<why this state is correct> | verifier_note=<lane-specific verification note>
- Docs | impact=yes|no | status=passed|waived|not_applicable|missing | rationale=<why this state is correct> | verifier_note=<lane-specific verification note>
- Analyzers | impact=yes|no | status=passed|waived|not_applicable|missing | rationale=<why this state is correct> | verifier_note=<lane-specific verification note>
- Install validation | impact=yes|no | status=passed|waived|not_applicable|missing | rationale=<why this state is correct> | verifier_note=<lane-specific verification note>
- Release hygiene | impact=yes|no | status=passed|waived|not_applicable|missing | rationale=<why this state is correct> | verifier_note=<lane-specific verification note>

## Implementation Notes

- Owner: <must match Implementer>
- Status: planned|in_progress|completed
- Notes: <implementation-side status or summary>

## Verification Notes

- Owner: <must match Verifier>
- Status: pending|in_progress|completed
- Commands: <commands or lanes run by the verifier>
- Observed result: <what the verifier observed>
- Contract mismatches: none|<mismatch summary>

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- `src/...` — description of changes
- `tests/...` — test coverage plan
- `CMakeLists.txt` — build integration (if applicable)

## Testing Strategy

How to verify the feature works correctly. Prefer deterministic, headless tests.

## Open Questions

Unresolved decisions or trade-offs that need input before implementation.
