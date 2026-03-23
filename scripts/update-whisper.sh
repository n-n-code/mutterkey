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

if ! git remote get-url "$upstream_remote" >/dev/null 2>&1; then
    printf 'Adding remote %s -> %s\n' "$upstream_remote" "$upstream_url"
    git remote add "$upstream_remote" "$upstream_url"
fi

printf 'Fetching %s from %s\n' "$upstream_ref" "$upstream_remote"
git fetch "$upstream_remote" "$upstream_ref"

if [[ -d third_party/whisper.cpp ]]; then
    printf 'Updating vendored whisper.cpp subtree at %s\n' "$upstream_ref"
    git subtree pull --prefix=third_party/whisper.cpp "$upstream_remote" "$upstream_ref" --squash
else
    printf 'Adding vendored whisper.cpp subtree at %s\n' "$upstream_ref"
    git subtree add --prefix=third_party/whisper.cpp "$upstream_remote" "$upstream_ref" --squash
fi

cat <<'EOF'
whisper.cpp subtree update completed.

Next steps:
- update third_party/whisper.cpp.UPSTREAM.md with the exact imported ref
- review THIRD_PARTY_NOTICES.md for notice changes
- configure, build, and run tests again
- inspect any local patches before committing
EOF
