#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    printf 'Usage: %s <build-dir>\n' "${0##*/}" >&2
    exit 2
fi

build_dir="$1"

if [[ ! -d "$build_dir" ]]; then
    printf 'Build directory not found: %s\n' "$build_dir" >&2
    exit 2
fi

if ! command -v valgrind >/dev/null 2>&1; then
    printf 'valgrind was not found in PATH.\n' >&2
    exit 2
fi

suppression_args=()
if [[ -n "${MUTTERKEY_VALGRIND_SUPPRESSIONS:-}" ]]; then
    if [[ ! -f "${MUTTERKEY_VALGRIND_SUPPRESSIONS}" ]]; then
        printf 'Suppression file not found: %s\n' "${MUTTERKEY_VALGRIND_SUPPRESSIONS}" >&2
        exit 2
    fi
    suppression_args+=(--suppressions="${MUTTERKEY_VALGRIND_SUPPRESSIONS}")
fi

valgrind_args=(
    --tool=memcheck
    --leak-check=full
    --show-leak-kinds=definite,possible
    --errors-for-leak-kinds=definite,possible
    --track-origins=yes
    --num-callers=30
    --error-exitcode=101
)
valgrind_args+=("${suppression_args[@]}")

run_memcheck() {
    local label="$1"
    shift

    printf 'Running Valgrind Memcheck: %s\n' "$label"
    valgrind "${valgrind_args[@]}" "$@"
}

require_binary() {
    local path="$1"

    if [[ ! -x "$path" ]]; then
        printf 'Expected executable not found: %s\n' "$path" >&2
        exit 2
    fi
}

config_test="${build_dir}/tests/configtest"
normalizer_test="${build_dir}/tests/recordingnormalizertest"
app_binary="${build_dir}/mutterkey"

require_binary "$config_test"
require_binary "$normalizer_test"
require_binary "$app_binary"

run_memcheck "configtest" env QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "$config_test"
run_memcheck "recordingnormalizertest" env QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "$normalizer_test"
run_memcheck "mutterkey --help" env QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "$app_binary" --help
