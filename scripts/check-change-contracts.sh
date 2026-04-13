#!/usr/bin/env bash

set -euo pipefail

policy_path="${FRAME_CONTRACT_POLICY_FILE:-config/change-contract-policy.sh}"
repo_root="${1:-$(pwd)}"
policy_file="$repo_root/$policy_path"

if [[ ! -f "$policy_file" ]]; then
    printf 'ERROR: missing contract policy file: %s\n' "$policy_file" >&2
    exit 1
fi

source "$policy_file"

plan_dir="${FRAME_CONTRACT_PLAN_DIR:-feature_records}"
template_basename="${FRAME_CONTRACT_TEMPLATE_BASENAME:-TEMPLATE.md}"
features_dir="$repo_root/$plan_dir"

if [[ ! -d "$features_dir" ]]; then
    printf 'ERROR: missing contract plan directory: %s\n' "$features_dir" >&2
    exit 1
fi

if [[ ${#FRAME_CONTRACT_REQUIRED_SECTIONS[@]} -eq 0 ]]; then
    printf 'ERROR: contract policy must define FRAME_CONTRACT_REQUIRED_SECTIONS\n' >&2
    exit 1
fi

if [[ ${#FRAME_CONTRACT_EVIDENCE_LANES[@]} -eq 0 ]]; then
    printf 'ERROR: contract policy must define FRAME_CONTRACT_EVIDENCE_LANES\n' >&2
    exit 1
fi

status=0
readonly array_sep=$'\034'

join_by_sep() {
    local sep="$1"
    shift
    local joined=""
    local item

    for item in "$@"; do
        if [[ -n "$joined" ]]; then
            joined+="$sep"
        fi
        joined+="$item"
    done

    printf '%s' "$joined"
}

is_substantive_path() {
    local path="$1"
    local pattern

    for pattern in "${FRAME_CONTRACT_SUBSTANTIVE_PATH_PATTERNS[@]:-}"; do
        case "$path" in
        $pattern)
            return 0
            ;;
        esac
    done

    for pattern in "${FRAME_CONTRACT_SUBSTANTIVE_TOP_LEVEL_FILES[@]:-}"; do
        if [[ "$path" == "$pattern" ]]; then
            return 0
        fi
    done

    return 1
}

collect_changed_files() {
    if [[ -n "${FRAME_CHANGE_CONTRACT_CHANGED_FILES:-}" ]]; then
        printf '%s\n' "${FRAME_CHANGE_CONTRACT_CHANGED_FILES}"
        return
    fi

    if ! command -v git >/dev/null 2>&1 || ! git -C "$repo_root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        return
    fi

    if [[ -n "${FRAME_CHANGE_CONTRACT_RANGE:-}" ]]; then
        git -C "$repo_root" diff --name-only --diff-filter=ACMR "${FRAME_CHANGE_CONTRACT_RANGE}" || true
        return
    fi

    if git -C "$repo_root" rev-parse --verify HEAD >/dev/null 2>&1; then
        git -C "$repo_root" diff --name-only --diff-filter=ACMR HEAD -- || true
    fi
    git -C "$repo_root" ls-files --others --exclude-standard || true
}

validate_feature_file() {
    local feature_file="$1"
    local feature_state_dir
    local required_sections_raw lifecycle_values_raw uncertainty_values_raw
    local evidence_status_values_raw yes_no_values_raw implementation_status_values_raw
    local verification_status_values_raw evidence_lanes_raw

    feature_state_dir="$(basename "$(dirname "$feature_file")")"

    required_sections_raw="$(join_by_sep "$array_sep" "${FRAME_CONTRACT_REQUIRED_SECTIONS[@]}")"
    lifecycle_values_raw="$(join_by_sep "$array_sep" "${FRAME_CONTRACT_LIFECYCLE_VALUES[@]}")"
    uncertainty_values_raw="$(join_by_sep "$array_sep" "${FRAME_CONTRACT_UNCERTAINTY_VALUES[@]}")"
    evidence_status_values_raw="$(join_by_sep "$array_sep" "${FRAME_CONTRACT_EVIDENCE_STATUS_VALUES[@]}")"
    yes_no_values_raw="$(join_by_sep "$array_sep" "${FRAME_CONTRACT_YES_NO_VALUES[@]}")"
    implementation_status_values_raw="$(join_by_sep "$array_sep" "${FRAME_CONTRACT_IMPLEMENTATION_STATUS_VALUES[@]}")"
    verification_status_values_raw="$(join_by_sep "$array_sep" "${FRAME_CONTRACT_VERIFICATION_STATUS_VALUES[@]}")"
    evidence_lanes_raw="$(join_by_sep "$array_sep" "${FRAME_CONTRACT_EVIDENCE_LANES[@]}")"

    awk \
        -v sep="$array_sep" \
        -v required_sections_raw="$required_sections_raw" \
        -v lifecycle_values_raw="$lifecycle_values_raw" \
        -v uncertainty_values_raw="$uncertainty_values_raw" \
        -v evidence_status_values_raw="$evidence_status_values_raw" \
        -v yes_no_values_raw="$yes_no_values_raw" \
        -v implementation_status_values_raw="$implementation_status_values_raw" \
        -v verification_status_values_raw="$verification_status_values_raw" \
        -v evidence_lanes_raw="$evidence_lanes_raw" \
        -v feature_state_dir="$feature_state_dir" \
        '
        BEGIN {
            status = 0
            split(required_sections_raw, required_sections_list, sep)
            split(lifecycle_values_raw, lifecycle_values_list, sep)
            split(uncertainty_values_raw, uncertainty_values_list, sep)
            split(evidence_status_values_raw, evidence_status_values_list, sep)
            split(yes_no_values_raw, yes_no_values_list, sep)
            split(implementation_status_values_raw, implementation_status_values_list, sep)
            split(verification_status_values_raw, verification_status_values_list, sep)
            split(evidence_lanes_raw, evidence_lanes_list, sep)

            for (i in required_sections_list) {
                if (required_sections_list[i] != "") {
                    required_sections[required_sections_list[i]] = 1
                }
            }
            for (i in lifecycle_values_list) {
                if (lifecycle_values_list[i] != "") {
                    allowed_lifecycle[lifecycle_values_list[i]] = 1
                }
            }
            for (i in uncertainty_values_list) {
                if (uncertainty_values_list[i] != "") {
                    allowed_band[uncertainty_values_list[i]] = 1
                }
            }
            for (i in evidence_status_values_list) {
                if (evidence_status_values_list[i] != "") {
                    allowed_status[evidence_status_values_list[i]] = 1
                }
            }
            for (i in yes_no_values_list) {
                if (yes_no_values_list[i] != "") {
                    allowed_yes_no[yes_no_values_list[i]] = 1
                }
            }
            for (i in implementation_status_values_list) {
                if (implementation_status_values_list[i] != "") {
                    allowed_impl_status[implementation_status_values_list[i]] = 1
                }
            }
            for (i in verification_status_values_list) {
                if (verification_status_values_list[i] != "") {
                    allowed_verification_status[verification_status_values_list[i]] = 1
                }
            }
            for (i in evidence_lanes_list) {
                if (evidence_lanes_list[i] != "") {
                    expected_lane[evidence_lanes_list[i]] = 1
                }
            }
        }

        function trim(value) {
            sub(/^[[:space:]]+/, "", value)
            sub(/[[:space:]]+$/, "", value)
            return value
        }

        function parse_bullet(prefix, target) {
            value = $0
            sub("^- " prefix ": ", "", value)
            target = trim(value)
            return target
        }

        function fail(message) {
            printf("%s: %s\n", FILENAME, message)
            status = 1
        }

        function parse_matrix_line(raw,    clean, parts, lane_name, i, field, pair, eq_pos, key, val) {
            clean = raw
            sub(/^- /, "", clean)
            split(clean, parts, /\|/)
            lane_name = trim(parts[1])

            if (!(lane_name in expected_lane)) {
                fail("Evidence Matrix has unknown lane \"" lane_name "\"")
                return
            }
            seen_lane[lane_name] = 1

            for (i = 2; i <= length(parts); ++i) {
                field = trim(parts[i])
                eq_pos = index(field, "=")
                if (eq_pos == 0) {
                    fail("Evidence Matrix entry for \"" lane_name "\" must use key=value fields")
                    continue
                }
                key = trim(substr(field, 1, eq_pos - 1))
                val = trim(substr(field, eq_pos + 1))
                evidence[lane_name, key] = val
            }
        }

        /^## / {
            current_section = $0
            seen_section[current_section] = 1
            next
        }

        current_section == "## Lifecycle" {
            if ($0 ~ /^- State: /) {
                lifecycle_state = parse_bullet("State", lifecycle_state)
            } else if ($0 ~ /^- Supersedes: /) {
                lifecycle_supersedes = parse_bullet("Supersedes", lifecycle_supersedes)
            } else if ($0 ~ /^- Superseded by: /) {
                lifecycle_superseded_by = parse_bullet("Superseded by", lifecycle_superseded_by)
            }
            next
        }

        current_section == "## Contract" {
            if ($0 ~ /^- Must remain true: /) {
                contract_must_remain = parse_bullet("Must remain true", contract_must_remain)
            } else if ($0 ~ /^- Must become true: /) {
                contract_must_become = parse_bullet("Must become true", contract_must_become)
            } else if ($0 ~ /^- Success signals: /) {
                contract_success_signals = parse_bullet("Success signals", contract_success_signals)
            }
            next
        }

        current_section == "## Uncertainty And Cost" {
            if ($0 ~ /^- Product uncertainty: /) {
                product_uncertainty = parse_bullet("Product uncertainty", product_uncertainty)
            } else if ($0 ~ /^- Technical uncertainty: /) {
                technical_uncertainty = parse_bullet("Technical uncertainty", technical_uncertainty)
            } else if ($0 ~ /^- Implementation cost: /) {
                implementation_cost = parse_bullet("Implementation cost", implementation_cost)
            } else if ($0 ~ /^- Validation cost: /) {
                validation_cost = parse_bullet("Validation cost", validation_cost)
            } else if ($0 ~ /^- Notes: /) {
                uncertainty_notes = parse_bullet("Notes", uncertainty_notes)
            }
            next
        }

        current_section == "## Responsibilities" {
            if ($0 ~ /^- Implementer: /) {
                implementer = parse_bullet("Implementer", implementer)
            } else if ($0 ~ /^- Verifier: /) {
                verifier = parse_bullet("Verifier", verifier)
            } else if ($0 ~ /^- Approver: /) {
                approver = parse_bullet("Approver", approver)
            }
            next
        }

        current_section == "## Evidence Matrix" {
            if ($0 ~ /^- /) {
                parse_matrix_line($0)
            }
            next
        }

        current_section == "## Implementation Notes" {
            if ($0 ~ /^- Owner: /) {
                implementation_owner = parse_bullet("Owner", implementation_owner)
            } else if ($0 ~ /^- Status: /) {
                implementation_status = parse_bullet("Status", implementation_status)
            } else if ($0 ~ /^- Notes: /) {
                implementation_notes = parse_bullet("Notes", implementation_notes)
            }
            next
        }

        current_section == "## Verification Notes" {
            if ($0 ~ /^- Owner: /) {
                verification_owner = parse_bullet("Owner", verification_owner)
            } else if ($0 ~ /^- Status: /) {
                verification_status = parse_bullet("Status", verification_status)
            } else if ($0 ~ /^- Commands: /) {
                verification_commands = parse_bullet("Commands", verification_commands)
            } else if ($0 ~ /^- Observed result: /) {
                verification_observed_result = parse_bullet("Observed result", verification_observed_result)
            } else if ($0 ~ /^- Contract mismatches: /) {
                verification_contract_mismatches = parse_bullet("Contract mismatches", verification_contract_mismatches)
            }
            next
        }

        current_section == "## Waivers" {
            if ($0 ~ /^- Self-validation rationale: /) {
                waiver_self_validation = parse_bullet("Self-validation rationale", waiver_self_validation)
            }
            next
        }

        END {
            for (section in required_sections) {
                if (!(section in seen_section)) {
                    fail("missing required section " section)
                }
            }

            if (!(lifecycle_state in allowed_lifecycle)) {
                fail("Lifecycle state must be one of: planned, active, superseded, done")
            }
            if (trim(lifecycle_supersedes) == "") {
                fail("Lifecycle must define Supersedes")
            }
            if (trim(lifecycle_superseded_by) == "") {
                fail("Lifecycle must define Superseded by")
            }
            if (lifecycle_state == "superseded" && lifecycle_superseded_by == "none") {
                fail("Superseded lifecycle state requires a \"Superseded by\" reference")
            }
            if (feature_state_dir != lifecycle_state) {
                fail("Feature record directory must match Lifecycle state")
            }

            if (trim(contract_must_remain) == "") {
                fail("Contract must define \"Must remain true\"")
            }
            if (trim(contract_must_become) == "") {
                fail("Contract must define \"Must become true\"")
            }
            if (trim(contract_success_signals) == "") {
                fail("Contract must define \"Success signals\"")
            }

            if (!(product_uncertainty in allowed_band)) {
                fail("Product uncertainty must be one of: low, medium, high")
            }
            if (!(technical_uncertainty in allowed_band)) {
                fail("Technical uncertainty must be one of: low, medium, high")
            }
            if (!(implementation_cost in allowed_band)) {
                fail("Implementation cost must be one of: low, medium, high")
            }
            if (!(validation_cost in allowed_band)) {
                fail("Validation cost must be one of: low, medium, high")
            }
            if (trim(uncertainty_notes) == "") {
                fail("Uncertainty And Cost must include Notes")
            }

            if (trim(implementer) == "") {
                fail("Responsibilities must define Implementer")
            }
            if (trim(verifier) == "") {
                fail("Responsibilities must define Verifier")
            }

            for (lane in expected_lane) {
                if (!(lane in seen_lane)) {
                    fail("Evidence Matrix must include lane \"" lane "\"")
                    continue
                }

                impact = evidence[lane, "impact"]
                evidence_status = evidence[lane, "status"]
                rationale = evidence[lane, "rationale"]
                verifier_note = evidence[lane, "verifier_note"]

                if (!(impact in allowed_yes_no)) {
                    fail(lane " evidence impact must be one of: yes, no")
                }
                if (!(evidence_status in allowed_status)) {
                    fail(lane " evidence status must be one of: passed, waived, not_applicable, missing")
                }
                if (evidence_status == "missing") {
                    fail(lane " evidence must not be marked missing")
                }
                if (impact == "yes" && evidence_status == "not_applicable") {
                    fail(lane " evidence cannot be not_applicable when impact is yes")
                }
                if (trim(rationale) == "") {
                    fail(lane " evidence rationale must be explicit")
                }
                if (evidence_status == "waived" && rationale == "none") {
                    fail(lane " evidence is waived but rationale is none")
                }
                if (trim(verifier_note) == "") {
                    fail(lane " evidence must include verifier_note")
                }
            }

            if (trim(implementation_owner) == "") {
                fail("Implementation Notes must define Owner")
            }
            if (implementation_owner != implementer) {
                fail("Implementation Notes owner must match Responsibilities implementer")
            }
            if (!(implementation_status in allowed_impl_status)) {
                fail("Implementation Notes status must be one of: planned, in_progress, completed")
            }
            if (trim(implementation_notes) == "") {
                fail("Implementation Notes must include Notes")
            }

            if (trim(verification_owner) == "") {
                fail("Verification Notes must define Owner")
            }
            if (verification_owner != verifier) {
                fail("Verification Notes owner must match Responsibilities verifier")
            }
            if (!(verification_status in allowed_verification_status)) {
                fail("Verification Notes status must be one of: pending, in_progress, completed")
            }
            if (trim(verification_commands) == "") {
                fail("Verification Notes must include Commands")
            }
            if (trim(verification_observed_result) == "") {
                fail("Verification Notes must include Observed result")
            }
            if (trim(verification_contract_mismatches) == "") {
                fail("Verification Notes must include Contract mismatches")
            }

            if (trim(waiver_self_validation) == "") {
                fail("Waivers must define Self-validation rationale")
            }

            if (implementer == verifier) {
                if (waiver_self_validation == "none") {
                    fail("Implementer and Verifier match without a self-validation rationale")
                }
                if (!(product_uncertainty == "low" && technical_uncertainty == "low" &&
                      implementation_cost != "high" && validation_cost != "high")) {
                    if (trim(approver) == "") {
                        fail("Higher-risk self-validation requires an Approver")
                    }
                    if (verification_status != "completed") {
                        fail("Higher-risk self-validation requires completed verification status")
                    }
                }
            }

            exit status
        }
    ' "$feature_file"
}

changed_files="$(collect_changed_files)"
substantive_change_count=0
changed_plan_count=0
substantive_paths=""

if [[ -n "$changed_files" ]]; then
    while IFS= read -r changed_file; do
        [[ -n "$changed_file" ]] || continue

        if is_substantive_path "$changed_file"; then
            substantive_change_count=$((substantive_change_count + 1))
            substantive_paths+="$changed_file"$'\n'
        fi

        case "$changed_file" in
        "$plan_dir"/*.md|"$plan_dir"/*/*.md)
            if [[ "$(basename "$changed_file")" != "$template_basename" &&
                  "$(basename "$changed_file")" != "README.md" ]]; then
                changed_plan_count=$((changed_plan_count + 1))
            fi
            ;;
        esac
    done <<<"$changed_files"
fi

if ((substantive_change_count > 0 && changed_plan_count == 0)); then
    printf 'ERROR: substantive repo-owned changes require a non-template feature_records plan update.\n' >&2
    printf 'Substantive changed paths:\n%s' "$substantive_paths" >&2
    status=1
fi

shopt -s nullglob
feature_files=("$features_dir"/*/*.md)
shopt -u nullglob

for feature_file in "${feature_files[@]}"; do
    if ! validate_feature_file "$feature_file"; then
        status=1
    fi
done

exit "$status"
