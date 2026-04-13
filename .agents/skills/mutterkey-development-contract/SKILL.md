---
name: mutterkey-development-contract
description: Repo-local development contract for Mutterkey. Use when starting substantive development work, changing repo-owned code/tests/scripts/docs/workflows, updating a `feature_records/<state>/...` contract plan, or needing the repo's policy file, lifecycle helper, or validation profiles.
---

# Mutterkey Development Contract

Read `AGENTS.md`, then read `config/change-contract-policy.sh`, then apply the shared development-contract workflow from `development-contract-core`.

## Use this skill when

- work may touch a substantive path or top-level file listed in `config/change-contract-policy.sh`
- creating, updating, or verifying a non-template plan under `feature_records/<state>/`
- deciding which validation profile this repo expects for a change

Do not use this skill for unrelated generic coding advice; pair it with a more specific implementation skill when needed.

## Core workflow

1. Read `AGENTS.md` and the touched files before editing.
2. Read `config/change-contract-policy.sh` for the plan directory, substantive path rules, required lanes, and validation profiles.
3. Apply `development-contract-core` for the generic workflow and decision rules.
4. If the change is substantive under repo policy, update a non-template plan in the lifecycle subdirectory under `feature_records/` that matches the record's `State`.
   Prefer `bash scripts/set-feature-record-lifecycle.sh` when changing an existing record's lifecycle.
5. Keep verifier notes concrete: record commands, observed results, and any contract mismatches explicitly.
6. Run the smallest repo policy profile that proves the change, then extend when the surface justifies it.
7. Before closing work, run the checker command declared in `config/change-contract-policy.sh`.

## Decision rules

- Treat `config/change-contract-policy.sh` as the source of truth for what is substantive in this repo.
- Do not leave a substantive change without a non-template contract plan update.
- Use `bash scripts/set-feature-record-lifecycle.sh --superseded-by ...` when superseding a record so the replacement link is updated with the move.
- Keep the repo overlay thin: change policy data first, then only add prose when Mutterkey needs extra human guidance that policy cannot express.
- If repo policy and docs disagree, fix the policy or the docs so the checker, template, and guidance converge again.

## Repo validation profiles

Use the policy file's named profiles as the default command sets:

- `FRAME_CONTRACT_VALIDATION_PROFILE_DOCS`
- `FRAME_CONTRACT_VALIDATION_PROFILE_CODE`
- `FRAME_CONTRACT_VALIDATION_PROFILE_RELEASE`

## Output expectations

When this skill applies, the final work should leave behind:

- code or docs aligned with `AGENTS.md`
- an updated non-template `feature_records/<state>/...` contract plan when the change is substantive
- explicit verifier evidence in the plan
- repo policy, docs, and checker behavior that still agree
- a concise report of what was validated and what could not be validated

## Examples

- `Implement this runtime-selection change in src/ and update tests`:
  read `AGENTS.md`, read `config/change-contract-policy.sh`, update the relevant `feature_records/active/*.md` plan, implement narrowly, validate with the smallest proving set, and record verifier commands/results.
- `Update the CI workflow to enforce a new repo rule`:
  treat it as substantive workflow work under policy, update the contract plan, run `check-change-contracts`, and verify workflow/docs alignment.
- `Revise README and release checklist for the new process`:
  treat it as substantive top-level documentation work, update the contract plan, and keep the evidence matrix and verifier notes current.
- `Finish a feature and accept it`:
  update verification evidence, then run `bash scripts/set-feature-record-lifecycle.sh feature_records/active/<record>.md done`.
