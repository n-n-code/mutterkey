#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

failures=0

collect_generated_artifacts() {
    find . \
        -path './.git' -prune -o \
        \( \
            -path './build' -o \
            \( -type d -name 'build-*' \) -o \
            \( -type d -name 'cmake-build-*' \) -o \
            -name 'CMakeCache.txt' -o \
            -name 'compile_commands.json' -o \
            -name 'CTestTestfile.cmake' -o \
            -name 'DartConfiguration.tcl' -o \
            -name 'install_manifest.txt' -o \
            -name 'Testing' -o \
            -name 'CMakeFiles' -o \
            -name '.qt' -o \
            -name '*_autogen' \
        \) -print
}

print_violation() {
    local title="$1"
    local matches="$2"

    if [[ -n "$matches" ]]; then
        printf 'FAIL: %s\n' "$title"
        printf '%s\n\n' "$matches"
        failures=1
    else
        printf 'PASS: %s\n' "$title"
    fi
}

collect_repo_files() {
    find . \
        -path './.git' -prune -o \
        -path './third_party' -prune -o \
        -path './build' -prune -o \
        -name 'build-*' -prune -o \
        -name 'cmake-build-*' -prune -o \
        -name 'CMakeFiles' -prune -o \
        -name '*_autogen' -prune -o \
        -path './scripts/check-release-hygiene.sh' -prune -o \
        -type f -print
}

collect_tracked_binary_artifacts() {
    if command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git ls-files \
            | grep -E '(^|/).+\.(bin|gguf)$' \
            | grep -Ev '^third_party/whisper\.cpp/' || true
        return
    fi

    find . \
        -path './.git' -prune -o \
        -path './third_party/whisper.cpp' -prune -o \
        -path './build' -prune -o \
        -name 'build-*' -prune -o \
        -name 'cmake-build-*' -prune -o \
        -name 'CMakeFiles' -prune -o \
        -name '*_autogen' -prune -o \
        \( -type f -name '*.bin' -o -type f -name '*.gguf' \) -print \
        | sed 's#^\./##' || true
}

mapfile -t repo_files < <(collect_repo_files)
home_path_matches=""
if ((${#repo_files[@]} > 0)); then
    home_path_matches="$(grep -nH --binary-files=without-match '/home/' "${repo_files[@]}" || true)"
fi
print_violation "repository-owned files must not contain /home/ paths" "$home_path_matches"

mapfile -t markdown_files < <(
    find . \
        -path './.git' -prune -o \
        -path './third_party' -prune -o \
        -path './build' -prune -o \
        -name 'build-*' -prune -o \
        -name 'cmake-build-*' -prune -o \
        -type f -name '*.md' -print
)
absolute_markdown_matches=""
if ((${#markdown_files[@]} > 0)); then
    absolute_markdown_matches="$(grep -nH -E '\]\((/|file://)[^)]+\)' "${markdown_files[@]}" || true)"
fi
print_violation "Markdown files must not contain absolute local links" "$absolute_markdown_matches"

generated_artifact_matches="$(
    collect_generated_artifacts \
        | sed 's#^\./##' \
        | awk -F/ '
            {
                root = $1
                if (!(root in seen)) {
                    seen[root] = 1
                    print "./" root
                }
            }
        ' \
        | sort || true
)"
print_violation "repository must not contain generated build artifacts" "$generated_artifact_matches"

tracked_binary_artifact_matches="$(collect_tracked_binary_artifacts)"
print_violation "repository must not track model or binary artifacts such as .bin or .gguf files" "$tracked_binary_artifact_matches"

commentary_check_output="$(bash "$repo_root/scripts/check-test-commentary.sh" "$repo_root" 2>&1 || true)"
print_violation "test sources must keep WHAT/HOW/WHY commentary blocks" "$commentary_check_output"

change_contract_output="$(bash "$repo_root/scripts/check-change-contracts.sh" "$repo_root" 2>&1 || true)"
print_violation "feature plans must keep explicit lifecycle, contract, ownership, evidence, and verifier fields" "$change_contract_output"

if ((failures != 0)); then
    exit 1
fi

printf 'Release hygiene checks passed.\n'
