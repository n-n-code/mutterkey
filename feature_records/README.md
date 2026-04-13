# Feature Records

This directory stores tracked change contracts for substantive repo-owned work.

Lifecycle folders:

- `planned/` contains approved or drafted work that has not started yet.
- `active/` contains work currently in flight.
- `done/` contains completed records that remain part of the repo history.
- `superseded/` contains historical records replaced by a newer record.

Rules:

- A record's folder must match its `## Lifecycle` `State`.
- `TEMPLATE.md` stays at the root and is used for all new records.
- `superseded/` records must still point at their replacement via `Superseded by`.
- Substantive repo changes must update a non-template record under one of the lifecycle folders.
- Use `bash scripts/set-feature-record-lifecycle.sh <record> <state>` to move a
  record between lifecycle folders and update its `Lifecycle` state field.
- When moving to `superseded`, pass `--superseded-by feature_records/<state>/<replacement>.md`.

Mutterkey keeps roadmap and implementation records here as Markdown only. The
checker policy lives in `config/change-contract-policy.sh`, and
`scripts/check-change-contracts.sh` enforces the template, lifecycle placement,
ownership, evidence, and substantive-change coverage rules.

Legacy note:

- `next_feature/` was the old planning directory. Its records were migrated into
  this lifecycle tree; the old directory is ignored as a local migration
  breadcrumb and should not receive new contract-bearing work.
