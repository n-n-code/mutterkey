#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF_USAGE'
Usage: bash scripts/set-feature-record-lifecycle.sh [--repo-root DIR] [--superseded-by PATH] <record-path> <target-state>

Moves a feature record into the lifecycle directory that matches <target-state>
and updates the record's `## Lifecycle` state fields accordingly.

Examples:
  bash scripts/set-feature-record-lifecycle.sh feature_records/planned/example.md active
  bash scripts/set-feature-record-lifecycle.sh \
    --superseded-by feature_records/done/replacement.md \
    feature_records/active/old-plan.md superseded
EOF_USAGE
}

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
superseded_by=""

while [[ $# -gt 0 ]]; do
    case "$1" in
    --repo-root)
        [[ $# -ge 2 ]] || die "--repo-root requires a value"
        repo_root="$2"
        shift 2
        ;;
    --superseded-by)
        [[ $# -ge 2 ]] || die "--superseded-by requires a value"
        superseded_by="$2"
        shift 2
        ;;
    --help)
        usage
        exit 0
        ;;
    --*)
        die "Unknown option: $1"
        ;;
    *)
        break
        ;;
    esac
done

[[ $# -eq 2 ]] || die "Expected <record-path> <target-state>"

record_arg="$1"
target_state="$2"
policy_file="$repo_root/config/change-contract-policy.sh"

[[ -f "$policy_file" ]] || die "Missing contract policy file: $policy_file"
source "$policy_file"

plan_dir="${FRAME_CONTRACT_PLAN_DIR:-feature_records}"
plan_root="$repo_root/$plan_dir"

case "$target_state" in
planned|active|done|superseded)
    ;;
*)
    die "Target state must be one of: planned, active, done, superseded"
    ;;
esac

case "$record_arg" in
/*)
    record_path="$record_arg"
    ;;
*)
    record_path="$repo_root/$record_arg"
    ;;
esac

[[ -f "$record_path" ]] || die "Feature record does not exist: $record_path"

case "$record_path" in
"$plan_root"/*)
    ;;
*)
    die "Feature record must live under $plan_root"
    ;;
esac

record_basename="$(basename "$record_path")"
[[ "$record_basename" != "$FRAME_CONTRACT_TEMPLATE_BASENAME" ]] || die "Template cannot be transitioned"
[[ "$record_basename" != "README.md" ]] || die "feature_records/README.md cannot be transitioned"

target_dir="$plan_root/$target_state"
mkdir -p "$target_dir"
target_path="$target_dir/$record_basename"

if [[ "$target_state" == "superseded" ]]; then
    [[ -n "$superseded_by" ]] || die "--superseded-by is required when moving to superseded"
    case "$superseded_by" in
    /*)
        superseded_by_path="$superseded_by"
        ;;
    *)
        superseded_by_path="$repo_root/$superseded_by"
        ;;
    esac

    [[ -f "$superseded_by_path" ]] || die "Replacement record does not exist: $superseded_by_path"
    case "$superseded_by_path" in
    "$plan_root"/*)
        ;;
    *)
        die "Replacement record must live under $plan_root"
        ;;
    esac
    superseded_by_value="${superseded_by_path#"$repo_root"/}"
else
    superseded_by_value="none"
fi

temp_file="$(mktemp)"
trap 'rm -f "$temp_file"' EXIT

awk \
    -v target_state="$target_state" \
    -v superseded_by_value="$superseded_by_value" \
    '
    BEGIN {
        in_lifecycle = 0
        saw_state = 0
        saw_superseded_by = 0
    }

    /^## / {
        if (in_lifecycle && !saw_state) {
            print "- State: " target_state
        }
        if (in_lifecycle && !saw_superseded_by) {
            print "- Superseded by: " superseded_by_value
        }
        in_lifecycle = ($0 == "## Lifecycle")
        print
        next
    }

    {
        if (in_lifecycle && $0 ~ /^- State: /) {
            print "- State: " target_state
            saw_state = 1
            next
        }
        if (in_lifecycle && $0 ~ /^- Superseded by: /) {
            print "- Superseded by: " superseded_by_value
            saw_superseded_by = 1
            next
        }
        print
    }

    END {
        if (in_lifecycle && !saw_state) {
            print "- State: " target_state
        }
        if (in_lifecycle && !saw_superseded_by) {
            print "- Superseded by: " superseded_by_value
        }
    }
    ' "$record_path" >"$temp_file"

mv "$temp_file" "$record_path"
trap - EXIT

if [[ "$record_path" != "$target_path" ]]; then
    mv "$record_path" "$target_path"
fi

printf 'Updated feature record lifecycle: %s -> %s\n' "${record_arg}" "${target_path#"$repo_root"/}"
