#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fixtures_dir="$repo_root/tests/fixtures/change_contracts"
checker="$repo_root/scripts/check-change-contracts.sh"
policy_source="$repo_root/config/change-contract-policy.sh"

source "$policy_source"

run_case() {
    local fixture_name="$1"
    local expected_result="$2"
    local changed_files="${3:-$FRAME_CONTRACT_PLAN_DIR/active/$fixture_name.md}"
    local feature_dir="${4:-active}"
    local temp_root
    local actual_result

    temp_root="$(mktemp -d /tmp/frame-change-contract-XXXXXX)"
    mkdir -p \
        "$temp_root/config" \
        "$temp_root/$FRAME_CONTRACT_PLAN_DIR/active" \
        "$temp_root/$FRAME_CONTRACT_PLAN_DIR/planned" \
        "$temp_root/$FRAME_CONTRACT_PLAN_DIR/done" \
        "$temp_root/$FRAME_CONTRACT_PLAN_DIR/superseded"
    cp "$policy_source" "$temp_root/config/change-contract-policy.sh"
    cp "$repo_root/$FRAME_CONTRACT_PLAN_DIR/$FRAME_CONTRACT_TEMPLATE_BASENAME" \
        "$temp_root/$FRAME_CONTRACT_PLAN_DIR/$FRAME_CONTRACT_TEMPLATE_BASENAME"
    cp "$repo_root/$FRAME_CONTRACT_PLAN_DIR/README.md" \
        "$temp_root/$FRAME_CONTRACT_PLAN_DIR/README.md"
    cp "$fixtures_dir/$fixture_name.md" \
        "$temp_root/$FRAME_CONTRACT_PLAN_DIR/$feature_dir/$fixture_name.md"

    if FRAME_CHANGE_CONTRACT_CHANGED_FILES="$changed_files" bash "$checker" "$temp_root" >/dev/null 2>&1; then
        actual_result="pass"
    else
        actual_result="fail"
    fi

    rm -rf "$temp_root"

    if [[ "$actual_result" != "$expected_result" ]]; then
        printf 'Fixture %s with changed files %q expected %s but got %s\n' \
            "$fixture_name" "$changed_files" "$expected_result" "$actual_result" >&2
        exit 1
    fi
}

run_substantive_path_case() {
    local changed_file="$1"
    run_case missing_plan_for_substantive_change fail "$changed_file"
}

run_case_with_policy_override() {
    local fixture_name="$1"
    local expected_result="$2"
    local changed_files="$3"
    local temp_root
    local actual_result

    temp_root="$(mktemp -d /tmp/frame-change-contract-XXXXXX)"
    mkdir -p "$temp_root/config" "$temp_root/docs/plans/active"
    cp "$fixtures_dir/$fixture_name.md" "$temp_root/docs/plans/active/$fixture_name.md"
    apply_override_policy "$temp_root/config/change-contract-policy.sh"
    cp "$repo_root/$FRAME_CONTRACT_PLAN_DIR/$FRAME_CONTRACT_TEMPLATE_BASENAME" \
        "$temp_root/docs/plans/TEMPLATE.md"
    cp "$repo_root/$FRAME_CONTRACT_PLAN_DIR/README.md" \
        "$temp_root/docs/plans/README.md"

    if FRAME_CHANGE_CONTRACT_CHANGED_FILES="$changed_files" bash "$checker" "$temp_root" >/dev/null 2>&1; then
        actual_result="pass"
    else
        actual_result="fail"
    fi

    rm -rf "$temp_root"

    if [[ "$actual_result" != "$expected_result" ]]; then
        printf 'Policy override fixture %s with changed files %q expected %s but got %s\n' \
            "$fixture_name" "$changed_files" "$expected_result" "$actual_result" >&2
        exit 1
    fi
}

apply_override_policy() {
    local policy_target="$1"

    cp "$policy_source" "$policy_target"
    perl -0pi -e "s/FRAME_CONTRACT_PLAN_DIR=\"feature_records\"/FRAME_CONTRACT_PLAN_DIR=\"docs\\/plans\"/" "$policy_target"
    perl -0pi -e "s/'src\\/\\*'/'lib\\/*'/" "$policy_target"
    perl -0pi -e "s/'tests\\/\\*'/'guide\\/*'/" "$policy_target"
    perl -0pi -e "s/'CMakeLists.txt'/'BUILDING.md'/" "$policy_target"
}

run_case valid_contract pass
run_case missing_contract_section fail
run_case invalid_uncertainty fail
run_case missing_verifier fail
run_case self_validation_without_waiver fail
run_case self_validation_with_waiver pass
run_case high_risk_self_validation_requires_approver fail
run_case waived_without_rationale fail
run_case missing_evidence_state fail
run_case impact_requires_evidence fail
run_case missing_verification_commands fail
run_case superseded_requires_pointer fail
run_substantive_path_case $'src/asr/runtime/transcriptionengine.cpp'
run_substantive_path_case $'.agents/skills/mutterkey-development-contract/SKILL.md'
run_substantive_path_case $'.github/ISSUE_TEMPLATE/bug_report.md'
run_substantive_path_case $'config/change-contract-policy.sh'
run_substantive_path_case $'feature_records/README.md'
run_substantive_path_case $'skills-lock.json'
run_substantive_path_case $'LICENSE'
run_substantive_path_case $'third_party/whisper.cpp.UPSTREAM.md'
run_case mismatched_directory_state fail $'feature_records/planned/mismatched_directory_state.md' planned
run_case_with_policy_override valid_contract pass $'docs/plans/active/valid_contract.md'
run_case_with_policy_override valid_contract fail $'lib/runtime.cpp'

printf 'Change contract fixtures passed.\n'
