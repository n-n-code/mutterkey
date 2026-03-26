#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: bash scripts/run-release-checklist.sh [--build-dir DIR] [--skip-diagnose] [-- <cmake-configure-args...>]

Runs the automated portion of RELEASE_CHECKLIST.md up to, but not including,
install validation.

Options:
  --build-dir DIR    Use an existing or chosen build directory instead of a new tmpdir.
  --skip-diagnose    Skip the headless `mutterkey diagnose 1` step.
  --help             Show this help text.

Examples:
  bash scripts/run-release-checklist.sh
  bash scripts/run-release-checklist.sh --build-dir /tmp/mutterkey-build-abc123
  bash scripts/run-release-checklist.sh -- -DMUTTERKEY_ENABLE_WHISPER_VULKAN=ON
EOF
}

die() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

note() {
    printf '==> %s\n' "$*"
}

run_cmd() {
    note "$*"
    "$@"
}

run_build_target() {
    shift

    local output
    if output="$("$@" 2>&1)"; then
        printf '%s\n' "$output"
        return 0
    fi

    printf '%s\n' "$output" >&2
    if grep -Fq 'ccache: error: Read-only file system' <<<"$output"; then
        note "Retrying with CCACHE_DISABLE=1 because ccache is read-only in this environment"
        CCACHE_DISABLE=1 "$@"
        return 0
    fi

    die "Build command failed"
}

assert_file_exists() {
    local path="$1"
    [[ -e "$path" ]] || die "Required file is missing: $path"
}

contains_vendored_rule() {
    grep -Fqx 'third_party/whisper.cpp/** linguist-vendored' .gitattributes
}

scan_for_model_binaries() {
    if command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git ls-files | grep -E '(^|/)(ggml-.*\.bin|.*\.(bin|gguf))$' | grep -Ev '^third_party/whisper\.cpp/' || true
        return
    fi

    find . \
        -path './.git' -prune -o \
        -path './third_party/whisper.cpp' -prune -o \
        -path './build' -prune -o \
        -path './build-*' -prune -o \
        -path './cmake-build-*' -prune -o \
        \( -name '*.bin' -o -name '*.gguf' \) -print
}

build_dir=""
skip_diagnose=0
declare -a extra_cmake_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
    --build-dir)
        [[ $# -ge 2 ]] || die "--build-dir requires a value"
        build_dir="$2"
        shift 2
        ;;
    --skip-diagnose)
        skip_diagnose=1
        shift
        ;;
    --help)
        usage
        exit 0
        ;;
    --)
        shift
        extra_cmake_args=("$@")
        break
        ;;
    *)
        die "Unknown argument: $1"
        ;;
    esac
done

if [[ -z "$build_dir" ]]; then
    build_dir="$(mktemp -d /tmp/mutterkey-build-XXXXXX)"
fi

declare -a generator_args=()
if command -v ninja >/dev/null 2>&1 || command -v ninja-build >/dev/null 2>&1; then
    generator_args=(-G Ninja)
fi

note "Release checklist build directory: $build_dir"

assert_file_exists LICENSE
assert_file_exists THIRD_PARTY_NOTICES.md
assert_file_exists third_party/whisper.cpp.UPSTREAM.md
assert_file_exists .gitattributes

contains_vendored_rule || die "Missing vendored linguist rule for third_party/whisper.cpp in .gitattributes"

tracked_binaries="$(scan_for_model_binaries)"
if [[ -n "$tracked_binaries" ]]; then
    die "Unexpected model/binary artifacts found:\n$tracked_binaries"
fi

run_cmd bash scripts/check-release-hygiene.sh

run_cmd cmake -S . -B "$build_dir" "${generator_args[@]}" -DCMAKE_BUILD_TYPE=Debug -DGGML_CCACHE=OFF "${extra_cmake_args[@]}"
run_build_target "$build_dir" cmake --build "$build_dir" -j"$(nproc)"
run_cmd ctest --test-dir "$build_dir" --output-on-failure
run_cmd bash scripts/run-valgrind.sh "$build_dir"
if command -v clang-tidy >/dev/null 2>&1; then
    run_build_target "$build_dir" cmake --build "$build_dir" --target clang-tidy
else
    note "Skipping clang-tidy because clang-tidy is not installed"
fi

if command -v clazy-standalone >/dev/null 2>&1; then
    run_build_target "$build_dir" cmake --build "$build_dir" --target clazy
else
    note "Skipping clazy because clazy-standalone is not installed"
fi

if command -v doxygen >/dev/null 2>&1; then
    run_build_target "$build_dir" cmake --build "$build_dir" --target docs
else
    note "Skipping docs because doxygen is not installed"
fi
run_cmd env QT_QPA_PLATFORM=offscreen "$build_dir/mutterkey" --help

note "Running tray-shell smoke check"
set +e
timeout 2s env QT_QPA_PLATFORM=offscreen "$build_dir/mutterkey-tray"
tray_status=$?
set -e
if [[ $tray_status -ne 0 && $tray_status -ne 124 ]]; then
    die "Headless tray-shell smoke check failed with exit code $tray_status"
fi

if [[ $skip_diagnose -eq 0 ]]; then
    run_cmd env QT_QPA_PLATFORM=offscreen "$build_dir/mutterkey" diagnose 1
else
    note "Skipping headless diagnose step by request"
fi

cat <<EOF

Automated pre-install release checks passed.

Build directory:
  $build_dir

Manual review items still remaining from RELEASE_CHECKLIST.md:
  - Review LICENSE, THIRD_PARTY_NOTICES.md, and third_party/whisper.cpp.UPSTREAM.md for release accuracy.
  - Review README.md, docs/mainpage.md, contrib/mutterkey.service, and contrib/org.mutterkey.mutterkey.desktop for release consistency.
  - Confirm runtime backend behavior on representative target hardware when shipping accelerated inference.
  - Perform the Install Validation section next.
EOF
