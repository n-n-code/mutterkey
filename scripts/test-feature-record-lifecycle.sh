#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
helper="$repo_root/scripts/set-feature-record-lifecycle.sh"
checker="$repo_root/scripts/check-change-contracts.sh"

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

assert_exists() {
    local path="$1"
    [[ -f "$path" ]] || die "Expected file to exist: $path"
}

assert_missing() {
    local path="$1"
    [[ ! -e "$path" ]] || die "Expected path to be absent: $path"
}

assert_contains() {
    local path="$1"
    local pattern="$2"
    if ! grep -Fq -- "$pattern" "$path"; then
        die "Expected $path to contain: $pattern"
    fi
}

make_temp_repo() {
    local temp_root
    temp_root="$(mktemp -d /tmp/frame-feature-record-lifecycle-XXXXXX)"
    mkdir -p \
        "$temp_root/config" \
        "$temp_root/scripts" \
        "$temp_root/feature_records/planned" \
        "$temp_root/feature_records/active" \
        "$temp_root/feature_records/done" \
        "$temp_root/feature_records/superseded"
    cp "$repo_root/config/change-contract-policy.sh" "$temp_root/config/change-contract-policy.sh"
    cp "$repo_root/feature_records/TEMPLATE.md" "$temp_root/feature_records/TEMPLATE.md"
    cp "$repo_root/feature_records/README.md" "$temp_root/feature_records/README.md"
    printf '%s\n' "$temp_root"
}

write_record() {
    local path="$1"
    local state="$2"

    cat >"$path" <<EOF_RECORD
# Feature: Test Record

## Motivation

Test record motivation.

## Proposed Behavior

Test record behavior.

## Lifecycle

- State: $state
- Supersedes: none
- Superseded by: none

## Contract

- Must remain true: Lifecycle edits should remain explicit.
- Must become true: The helper should move this record and update its lifecycle fields.
- Success signals: The checker still passes after the helper runs.

## Uncertainty And Cost

- Product uncertainty: low
- Technical uncertainty: low
- Implementation cost: low
- Validation cost: low
- Notes: Helper test fixture.

## Responsibilities

- Implementer: test-author
- Verifier: test-reviewer
- Approver: none

## Evidence Matrix

- Tests | impact=yes | status=passed | rationale=The helper is validated by shell tests. | verifier_note=Helper test fixture.
- Docs | impact=no | status=not_applicable | rationale=Docs are out of scope for this fixture. | verifier_note=Helper test fixture.
- Analyzers | impact=no | status=not_applicable | rationale=Analyzers are out of scope for this fixture. | verifier_note=Helper test fixture.
- Install validation | impact=no | status=not_applicable | rationale=Install behavior is out of scope for this fixture. | verifier_note=Helper test fixture.
- Release hygiene | impact=no | status=not_applicable | rationale=Release hygiene is out of scope for this fixture. | verifier_note=Helper test fixture.

## Implementation Notes

- Owner: test-author
- Status: planned
- Notes: Helper test fixture.

## Verification Notes

- Owner: test-reviewer
- Status: pending
- Commands: helper test fixture
- Observed result: pending
- Contract mismatches: none

## Waivers

- Self-validation rationale: none

## Files to Add/Modify

- \`feature_records/...\` — helper test fixture

## Testing Strategy

Run the helper and the checker.

## Open Questions

None.
EOF_RECORD
}

test_move_to_active() {
    local temp_root record_path
    temp_root="$(make_temp_repo)"
    record_path="$temp_root/feature_records/planned/test-record.md"
    write_record "$record_path" planned

    bash "$helper" --repo-root "$temp_root" feature_records/planned/test-record.md active >/dev/null

    assert_missing "$record_path"
    assert_exists "$temp_root/feature_records/active/test-record.md"
    assert_contains "$temp_root/feature_records/active/test-record.md" "- State: active"
    bash "$checker" "$temp_root" >/dev/null
    rm -rf "$temp_root"
}

test_move_to_done() {
    local temp_root record_path
    temp_root="$(make_temp_repo)"
    record_path="$temp_root/feature_records/active/test-record.md"
    write_record "$record_path" active

    bash "$helper" --repo-root "$temp_root" feature_records/active/test-record.md done >/dev/null

    assert_missing "$record_path"
    assert_exists "$temp_root/feature_records/done/test-record.md"
    assert_contains "$temp_root/feature_records/done/test-record.md" "- State: done"
    bash "$checker" "$temp_root" >/dev/null
    rm -rf "$temp_root"
}

test_move_to_superseded() {
    local temp_root record_path replacement_path
    temp_root="$(make_temp_repo)"
    record_path="$temp_root/feature_records/active/test-record.md"
    replacement_path="$temp_root/feature_records/done/replacement.md"
    write_record "$record_path" active
    write_record "$replacement_path" done

    bash "$helper" \
        --repo-root "$temp_root" \
        --superseded-by feature_records/done/replacement.md \
        feature_records/active/test-record.md superseded >/dev/null

    assert_missing "$record_path"
    assert_exists "$temp_root/feature_records/superseded/test-record.md"
    assert_contains "$temp_root/feature_records/superseded/test-record.md" "- State: superseded"
    assert_contains "$temp_root/feature_records/superseded/test-record.md" "- Superseded by: feature_records/done/replacement.md"
    bash "$checker" "$temp_root" >/dev/null
    rm -rf "$temp_root"
}

test_superseded_requires_replacement() {
    local temp_root record_path
    temp_root="$(make_temp_repo)"
    record_path="$temp_root/feature_records/active/test-record.md"
    write_record "$record_path" active

    if bash "$helper" --repo-root "$temp_root" feature_records/active/test-record.md superseded >/dev/null 2>&1; then
        die "Expected superseded transition without replacement to fail"
    fi

    assert_exists "$record_path"
    rm -rf "$temp_root"
}

test_move_to_active
test_move_to_done
test_move_to_superseded
test_superseded_requires_replacement

printf 'Feature record lifecycle helper tests passed.\n'
