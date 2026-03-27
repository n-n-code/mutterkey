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

bool_env() {
    local value="${1:-}"
    case "${value,,}" in
        1|yes|true|on)
            return 0
            ;;
        0|no|false|off)
            return 1
            ;;
        *)
            return 1
            ;;
    esac
}

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
    --show-error-list=yes
    --error-exitcode=101
)

if [[ -z "${MUTTERKEY_VALGRIND_READ_VAR_INFO:-}" ]] || bool_env "${MUTTERKEY_VALGRIND_READ_VAR_INFO}"; then
    valgrind_args+=(--read-var-info=yes)
fi

if bool_env "${MUTTERKEY_VALGRIND_KEEP_DEBUGINFO:-}"; then
    valgrind_args+=(--keep-debuginfo=yes)
fi

if [[ -n "${MUTTERKEY_VALGRIND_GEN_SUPPRESSIONS:-}" ]]; then
    valgrind_args+=(--gen-suppressions="${MUTTERKEY_VALGRIND_GEN_SUPPRESSIONS}")
fi

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

app_binary="${build_dir}/mutterkey"
require_binary "$app_binary"

test_binaries=(
    "${build_dir}/tests/configtest"
    "${build_dir}/tests/commanddispatchtest"
    "${build_dir}/tests/recordingnormalizertest"
    "${build_dir}/tests/streamingtranscriptiontest"
    "${build_dir}/tests/daemoncontrolprotocoltest"
    "${build_dir}/tests/daemoncontroltypestest"
    "${build_dir}/tests/transcriptionworkertest"
    "${build_dir}/tests/platformlogicstest"
)

# Keep the default Memcheck lane focused on binaries that are stable when launched
# directly under constrained environments. The daemoncontrolclientservertest
# socket round-trip path is still covered by normal ctest, but direct AF_UNIX
# bind/listen can be blocked by some sandboxes with EPERM before Memcheck ever
# gets a chance to inspect the code under test.

for test_binary in "${test_binaries[@]}"; do
    require_binary "$test_binary"
done

for test_binary in "${test_binaries[@]}"; do
    run_memcheck "$(basename "$test_binary")" env QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "$test_binary"
done

run_memcheck "mutterkey --help" env QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "$app_binary" --help
