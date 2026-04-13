#!/usr/bin/env bash

# Repo-local change-contract policy consumed by the checker and overlay skill.

FRAME_CONTRACT_PLAN_DIR="feature_records"
FRAME_CONTRACT_TEMPLATE_BASENAME="TEMPLATE.md"

FRAME_CONTRACT_SUBSTANTIVE_PATH_PATTERNS=(
    '.agents/skills/*'
    '.github/ISSUE_TEMPLATE/*'
    '.github/workflows/*'
    'config/*'
    'feature_records/*'
    'src/*'
    'tests/*'
    'scripts/*'
    'cmake/*'
    'docs/*'
    'contrib/*'
    'tools/*'
    'third_party/whisper.cpp.UPSTREAM.md'
)

FRAME_CONTRACT_SUBSTANTIVE_TOP_LEVEL_FILES=(
    '.clang-tidy'
    '.gitattributes'
    '.gitignore'
    'CMakeLists.txt'
    'CMakePresets.json'
    'README.md'
    'AGENTS.md'
    'LICENSE'
    'RELEASE_CHECKLIST.md'
    'THIRD_PARTY_NOTICES.md'
    'config.example.json'
    'skills-lock.json'
)

FRAME_CONTRACT_REQUIRED_SECTIONS=(
    '## Motivation'
    '## Proposed Behavior'
    '## Lifecycle'
    '## Contract'
    '## Uncertainty And Cost'
    '## Responsibilities'
    '## Evidence Matrix'
    '## Implementation Notes'
    '## Verification Notes'
    '## Files to Add/Modify'
    '## Testing Strategy'
    '## Waivers'
)

FRAME_CONTRACT_LIFECYCLE_VALUES=(
    'planned'
    'active'
    'superseded'
    'done'
)

FRAME_CONTRACT_UNCERTAINTY_VALUES=(
    'low'
    'medium'
    'high'
)

FRAME_CONTRACT_EVIDENCE_STATUS_VALUES=(
    'passed'
    'waived'
    'not_applicable'
    'missing'
)

FRAME_CONTRACT_YES_NO_VALUES=(
    'yes'
    'no'
)

FRAME_CONTRACT_IMPLEMENTATION_STATUS_VALUES=(
    'planned'
    'in_progress'
    'completed'
)

FRAME_CONTRACT_VERIFICATION_STATUS_VALUES=(
    'pending'
    'in_progress'
    'completed'
)

FRAME_CONTRACT_EVIDENCE_LANES=(
    'Tests'
    'Docs'
    'Analyzers'
    'Install validation'
    'Release hygiene'
)

FRAME_CONTRACT_CHECKER_COMMAND='bash scripts/check-change-contracts.sh'

FRAME_CONTRACT_VALIDATION_PROFILE_DOCS=(
    'bash scripts/check-change-contracts.sh'
    'bash scripts/check-release-hygiene.sh'
)

FRAME_CONTRACT_VALIDATION_PROFILE_CODE=(
    'bash scripts/check-change-contracts.sh'
    'BUILD_DIR="$(mktemp -d /tmp/mutterkey-build-XXXXXX)"'
    'cmake -S . -B "$BUILD_DIR" -G Ninja'
    'cmake --build "$BUILD_DIR" -j"$(nproc)"'
    'ctest --test-dir "$BUILD_DIR" --output-on-failure'
    'QT_QPA_PLATFORM=offscreen "$BUILD_DIR/mutterkey" --help'
    'cmake --build "$BUILD_DIR" --target lint'
)

FRAME_CONTRACT_VALIDATION_PROFILE_RELEASE=(
    'bash scripts/check-change-contracts.sh'
    'bash scripts/check-release-hygiene.sh'
    'BUILD_DIR="$(mktemp -d /tmp/mutterkey-build-XXXXXX)"'
    'cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGGML_CCACHE=OFF'
    'cmake --build "$BUILD_DIR" -j"$(nproc)"'
    'ctest --test-dir "$BUILD_DIR" --output-on-failure'
    'cmake --build "$BUILD_DIR" --target docs'
    'bash scripts/run-valgrind.sh "$BUILD_DIR"'
)
