# Feature: Development Contract System Adoption

## Motivation

Mutterkey had an older `next_feature/` contract shape while the development
frame now provides a repo-owned policy file, lifecycle directories, a portable
checker, and a deterministic lifecycle helper. Mutterkey needs the current
iteration so substantive work is tracked consistently and the checker can use a
single policy source of truth.

## Proposed Behavior

Adopt the current development-contract system from the frame while keeping
Mutterkey-specific validation commands and roadmap records. Feature records live
under `feature_records/<state>/`, the checker loads
`config/change-contract-policy.sh`, and release hygiene plus CI invoke the
policy-backed checker.

## Lifecycle

- State: done
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Mutterkey remains KDE-first, CMake-only, local-only, and aligned with its existing README and AGENTS guidance.
- Must become true: Contract-bearing plans live under lifecycle-matching `feature_records/` directories, and scripts/CI use the policy-backed checker and lifecycle helper.
- Success signals: The migrated records pass the checker, the helper tests pass, release hygiene passes, and docs/skills refer to `feature_records/` instead of `next_feature/`.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: medium
- Implementation cost: medium
- Validation cost: medium
- Notes: The frame implementation is proven locally, but the port touches repo process, CI, docs, scripts, fixtures, local skills, and policy coverage for release/provenance surfaces.

## Responsibilities

- Implementer: codex
- Verifier: codex
- Approver: repo-maintainer

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The checker and lifecycle helper are shell-tested workflow behavior and are now wired into CTest with workflow labels and Mutterkey-specific policy-surface fixture cases. | verifier_note=Validated with `bash scripts/test-change-contracts.sh`, `bash scripts/test-feature-record-lifecycle.sh`, and `ctest --test-dir /tmp/mutterkey-contract-pass-build --output-on-failure`.
- Docs | impact=yes | status=passed | rationale=README, AGENTS, release checklist, feature-record docs, and the local overlay skill changed with the workflow. | verifier_note=Reviewed repo guidance for `feature_records/`, lifecycle helper usage, and policy-file alignment.
- Analyzers | impact=no | status=not_applicable | rationale=This change is shell, Markdown, workflow, and local skill content rather than analyzer-sensitive C++ code. | verifier_note=C++ analyzer lanes stayed out of scope for this process port.
- Install validation | impact=no | status=not_applicable | rationale=Install layout and shipped runtime assets are unchanged. | verifier_note=Install validation stayed out of scope.
- Release hygiene | impact=yes | status=passed | rationale=The workflow is release-facing and must remain aligned with hygiene checks. | verifier_note=Validated with `bash scripts/check-change-contracts.sh` and `bash scripts/check-release-hygiene.sh`.

## Implementation Notes

- Owner: codex
- Status: completed
- Notes: Ported the policy-backed checker/helper, migrated real roadmap records into lifecycle folders, broadened the policy surface to cover repo-owned process/release artifacts, added policy-surface fixture coverage, and updated repo guidance to treat the policy file as the contract source of truth.

## Verification Notes

- Owner: codex
- Status: completed
- Commands: `bash scripts/test-change-contracts.sh`; `bash scripts/test-feature-record-lifecycle.sh`; `bash scripts/check-change-contracts.sh`; `bash scripts/check-release-hygiene.sh`; `cmake -S . -B /tmp/mutterkey-contract-pass-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGGML_CCACHE=OFF`; `cmake --build /tmp/mutterkey-contract-pass-build -j4`; `ctest --test-dir /tmp/mutterkey-contract-pass-build --output-on-failure`; `ctest --test-dir /tmp/mutterkey-contract-pass-build -L workflow --output-on-failure`; `QT_QPA_PLATFORM=offscreen /tmp/mutterkey-contract-pass-build/mutterkey --help`
- Observed result: The checker fixture suite, lifecycle helper tests, direct checker run, release hygiene run, full configure/build, 19 CTest tests, workflow-labeled CTest subset, and headless CLI help check passed locally.
- Contract mismatches: none

## Waivers

- Self-validation rationale: The process port was implemented and verified in one session, with explicit shell-test and hygiene evidence recorded here.

## Files to Add/Modify

- `config/change-contract-policy.sh` — Mutterkey-specific contract policy.
- `feature_records/...` — lifecycle tree, migrated roadmap records, and adoption record.
- `scripts/check-change-contracts.sh` — policy-backed contract checker.
- `scripts/set-feature-record-lifecycle.sh` — lifecycle transition helper.
- `scripts/test-change-contracts.sh`, `scripts/test-feature-record-lifecycle.sh`, `tests/fixtures/change_contracts/*` — checker/helper coverage.
- `README.md`, `AGENTS.md`, `RELEASE_CHECKLIST.md`, `.gitignore` — maintainer-facing workflow guidance and migration breadcrumbs.
- `.agents/skills/...` — repo-local contract overlay and portable core skill.

## Testing Strategy

Run the checker fixture suite, lifecycle helper tests, direct checker, and
release hygiene after the port.

## Open Questions

None. The obsolete untracked `next_feature/` directory is intentionally ignored
as a local migration breadcrumb; new contract-bearing work belongs under
`feature_records/<state>/`.
