#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

upstream_remote="${WHISPER_UPSTREAM_REMOTE:-whisper-upstream}"
upstream_url="${WHISPER_UPSTREAM_URL:-https://github.com/ggml-org/whisper.cpp.git}"
upstream_ref="${1:-}"

if [[ -z "$upstream_ref" ]]; then
    printf 'Usage: bash scripts/update-whisper.sh <upstream-tag-or-commit>\n' >&2
    printf 'Example: bash scripts/update-whisper.sh v1.8.4\n' >&2
    exit 1
fi

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    printf 'This update workflow must be run from inside a Git work tree.\n' >&2
    exit 1
fi

require_clean_worktree() {
    if [[ -n "$(git status --short --untracked-files=all)" ]]; then
        printf 'Refusing to update whisper.cpp: Git work tree is not clean.\n' >&2
        printf 'Commit, stash, or discard local changes before running this script.\n' >&2
        printf '\nCurrent work tree state:\n' >&2
        git status --short --untracked-files=all >&2
        exit 1
    fi
}

require_clean_worktree

subtree_prefix="third_party/whisper.cpp"

has_existing_subtree() {
    [[ -n "$(git log --grep="^git-subtree-dir: ${subtree_prefix}\$" -n 1 --format=%H)" ]]
}

require_subtree_bootstrap() {
    printf "Refusing to update whisper.cpp with git subtree.\n" >&2
    printf "'%s' exists, but this repository history does not show a prior 'git subtree add' for that prefix.\n" "$subtree_prefix" >&2
    printf "The current vendored copy was imported as a plain directory, so 'git subtree pull' cannot attach to it.\n" >&2
    printf "\nBootstrap options:\n" >&2
    printf "1. remove the existing vendored directory in a dedicated commit\n" >&2
    printf "2. rerun: bash scripts/update-whisper.sh %s\n" "$upstream_ref" >&2
    printf "\nExample:\n" >&2
    printf "  git rm -r %s\n" "$subtree_prefix" >&2
    printf "  git commit -m 'vendor: remove pre-subtree whisper.cpp snapshot'\n" >&2
    printf "  bash scripts/update-whisper.sh %s\n" "$upstream_ref" >&2
    exit 1
}

if ! git remote get-url "$upstream_remote" >/dev/null 2>&1; then
    printf 'Adding remote %s -> %s\n' "$upstream_remote" "$upstream_url"
    git remote add "$upstream_remote" "$upstream_url"
fi

printf 'Fetching %s from %s\n' "$upstream_ref" "$upstream_remote"
git fetch "$upstream_remote" "$upstream_ref"

if [[ -d "$subtree_prefix" ]]; then
    if ! has_existing_subtree; then
        require_subtree_bootstrap
    fi
    printf 'Updating vendored whisper.cpp subtree at %s\n' "$upstream_ref"
    git subtree pull --prefix="$subtree_prefix" "$upstream_remote" "$upstream_ref" --squash
else
    printf 'Adding vendored whisper.cpp subtree at %s\n' "$upstream_ref"
    git subtree add --prefix="$subtree_prefix" "$upstream_remote" "$upstream_ref" --squash
fi

cat <<'EOF'
whisper.cpp subtree update completed.

Next steps:
- update third_party/whisper.cpp.UPSTREAM.md with the exact imported ref
- review THIRD_PARTY_NOTICES.md for notice changes
- configure, build, and run tests again
- inspect any local patches before committing
EOF
