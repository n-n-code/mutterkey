#!/usr/bin/env python3
"""Stage a HuggingFace Whisper conversion into a Mutterkey native model package.

Input:  an MKCPUR3 `.mkweights` file (produced by
        tools/convert_safetensors_to_mkcpur3.py), plus the HuggingFace
        tokenizer files (`vocab.json`, `merges.txt`).
Output: a directory containing `model.json`, the copied weights, a
        line-ordered `vocab.txt`, and `merges.txt`, ready to be loaded by
        `ModelValidator::validatePackagePath`.

Only the Python 3 standard library is required.

Usage:
    python3 stage_native_package.py \
        --weights /path/to/base-en.mkweights \
        --vocab-json /path/to/vocab.json \
        --merges /path/to/merges.txt \
        --config-json /path/to/config.json \
        --baseline whisper-base-en \
        --output /path/to/output-package-dir
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


MKCPUR3_MAGIC = b"MKCPUR3\x00"
MKCPUR3_VERSION = 3

# Stable marker strings expected by Mutterkey's native CPU runtime (mkasr-v2).
MANIFEST_FORMAT = "mutterkey.model-package"
MANIFEST_SCHEMA_VERSION = 1
ENGINE_NAME = "mutterkey.cpu-reference"
MODEL_FORMAT = "mkasr-v2"
RUNTIME_FAMILY = "asr"
EXECUTION_VERSION = 2
DECODER_KIND = "real-decoder-v3"
TOKENIZER_KIND = "whisper-bpe"
FRONTEND_KIND = "log-mel-v1"
SEARCH_POLICY = "greedy-real-v1"
TIMESTAMP_MODE = "timestamp-token-v1"
TOKENIZER_ASSET_ROLE = "tokenizer_vocab"
TOKENIZER_MERGES_ASSET_ROLE = "tokenizer_merges"
WEIGHTS_ASSET_ROLE = "weights"

# Whisper-family special tokens (base.en vocabulary layout, 51864 tokens).
# These are placed in the manifest as-is; the real decoder reads them from
# package execution metadata instead of hardcoding them.
WHISPER_BOS_TOKEN_ID = 50257
WHISPER_EOS_TOKEN_ID = 50256
WHISPER_NO_SPEECH_TOKEN_ID = 50361
WHISPER_TIMESTAMP_START_ID = 50363
# Highest timestamp token id for a 51864-token Whisper vocabulary. Must be
# strictly less than the packaged vocabulary size; see `hasSaneNative
# DecoderMetadata` in src/asr/model/modelvalidator.cpp.
WHISPER_TIMESTAMP_END_ID = 51863

# Default Whisper-en prompt tokens fed after the SOT token. The runtime
# falls back to these when `initial_prompt_token_ids` is empty; emitting
# them explicitly lets the manifest drive prompt selection.
WHISPER_LANGUAGE_EN_TOKEN_ID = 50258
WHISPER_TASK_TRANSCRIBE_TOKEN_ID = 50358
WHISPER_NO_TIMESTAMPS_TOKEN_ID = 50362

WHISPER_LANGUAGE_TOKEN_IDS = {
    "en": WHISPER_LANGUAGE_EN_TOKEN_ID,
}
WHISPER_TASK_TOKEN_IDS = {
    "transcribe": WHISPER_TASK_TRANSCRIBE_TOKEN_ID,
}


def whisper_initial_prompt(baseline_family: str, language: str, task: str, include_timestamps: bool) -> list[int]:
    """Build a Whisper-family initial prompt sequence (without the leading SOT)."""
    baseline_key = baseline_family.lower()
    if baseline_key.endswith("-en") or baseline_key.endswith(".en"):
        return [] if include_timestamps else [WHISPER_NO_TIMESTAMPS_TOKEN_ID]

    language_key = language.lower()
    if language_key not in WHISPER_LANGUAGE_TOKEN_IDS:
        raise SystemExit(
            f"Unsupported --language '{language}'. Supported: "
            f"{sorted(WHISPER_LANGUAGE_TOKEN_IDS)}"
        )
    task_key = task.lower()
    if task_key not in WHISPER_TASK_TOKEN_IDS:
        raise SystemExit(
            f"Unsupported --task '{task}'. Supported: "
            f"{sorted(WHISPER_TASK_TOKEN_IDS)}"
        )
    prompt = [WHISPER_LANGUAGE_TOKEN_IDS[language_key], WHISPER_TASK_TOKEN_IDS[task_key]]
    if not include_timestamps:
        prompt.append(WHISPER_NO_TIMESTAMPS_TOKEN_ID)
    return prompt


@dataclass(frozen=True)
class MkcpuR3Header:
    tensor_count: int
    metadata: dict


def read_mkcpur3_header(path: Path) -> MkcpuR3Header:
    """Parse the MKCPUR3 header and return its tensor count and JSON metadata."""
    with path.open("rb") as handle:
        magic = handle.read(8)
        if magic != MKCPUR3_MAGIC:
            raise SystemExit(f"Not an MKCPUR3 file: {path}")
        version, tensor_count, metadata_bytes = struct.unpack("<III", handle.read(12))
        if version != MKCPUR3_VERSION:
            raise SystemExit(f"Unsupported MKCPUR3 version {version} in {path}")
        raw_metadata = handle.read(metadata_bytes)
        if len(raw_metadata) != metadata_bytes:
            raise SystemExit(f"Truncated metadata section in {path}")
    try:
        metadata = json.loads(raw_metadata.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise SystemExit(f"Failed to parse MKCPUR3 metadata: {exc}") from exc
    return MkcpuR3Header(tensor_count=tensor_count, metadata=metadata)


def read_json_object(path: Path, label: str) -> dict:
    try:
        with path.open("r", encoding="utf-8") as handle:
            value = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"Failed to parse {label} JSON at {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise SystemExit(f"{label} JSON is not an object: {path}")
    return value


def forced_decoder_prompt(config: dict, include_timestamps: bool) -> list[int]:
    """Read HuggingFace forced decoder ids as prompt tokens after SOT."""
    forced_ids = config.get("forced_decoder_ids")
    if not isinstance(forced_ids, list):
        return []

    pairs: list[tuple[int, int]] = []
    for item in forced_ids:
        if (
            isinstance(item, list)
            and len(item) == 2
            and isinstance(item[0], int)
            and isinstance(item[1], int)
        ):
            pairs.append((item[0], item[1]))
    prompt = [token_id for _, token_id in sorted(pairs)]
    if include_timestamps:
        prompt = [token_id for token_id in prompt if token_id != WHISPER_NO_TIMESTAMPS_TOKEN_ID]
    return prompt


def suppressed_tokens(config: dict) -> list[int]:
    """Read stable generation suppression ids from a HuggingFace config."""
    raw_tokens = config.get("suppress_tokens")
    if not isinstance(raw_tokens, list):
        return []
    return sorted({token_id for token_id in raw_tokens if isinstance(token_id, int) and token_id >= 0})


def sha256_of(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def write_line_ordered_vocabulary(
    vocab_json_path: Path, output_path: Path, declared_vocab_size: int
) -> int:
    """Convert HuggingFace `vocab.json` into a gap-filled line-ordered file.

    The runtime's `loadTokenizerVocabulary` expects line N to contain the
    string for token id N, and compares line count against the manifest's
    `vocabulary_size`. Missing ids are filled with a stable `<|unused_N|>`
    placeholder so the line count stays in lock-step with the vocabulary
    size declared in metadata.
    """
    with vocab_json_path.open("r", encoding="utf-8") as handle:
        vocab_obj = json.load(handle)
    if not isinstance(vocab_obj, dict):
        raise SystemExit(f"vocab.json is not a JSON object: {vocab_json_path}")

    id_to_token: dict[int, str] = {}
    for token_string, raw_id in vocab_obj.items():
        if not isinstance(raw_id, int):
            continue
        id_to_token[raw_id] = token_string

    max_id = max(id_to_token) if id_to_token else -1
    required_length = max(declared_vocab_size, max_id + 1)

    gaps = 0
    lines = []
    for index in range(required_length):
        token = id_to_token.get(index)
        if token is None:
            lines.append(f"<|unused_{index}|>")
            gaps += 1
            continue
        # Disallow embedded newlines; replace with a visible marker so the
        # line-ordered format stays one-line-per-id.
        if "\n" in token or "\r" in token:
            token = token.replace("\n", "\\n").replace("\r", "\\r")
        # Skip empty strings by substituting a placeholder so the loader,
        # which drops empty lines, stays aligned with the manifest.
        if token == "":
            token = f"<|empty_{index}|>"
            gaps += 1
        lines.append(token)

    with output_path.open("w", encoding="utf-8") as handle:
        for line in lines:
            handle.write(line)
            handle.write("\n")

    if declared_vocab_size and declared_vocab_size != len(lines):
        raise SystemExit(
            f"vocab.json yields {len(lines)} lines but MKCPUR3 metadata "
            f"declares n_vocab={declared_vocab_size}"
        )
    return gaps


def build_manifest(
    weights_asset: dict,
    vocab_asset: dict,
    merges_asset: dict,
    metadata: dict,
    baseline_family: str,
    package_id: str,
    display_name: str,
    language_profile: str,
    source_artifact: str,
    initial_prompt_token_ids: list[int],
    suppressed_token_ids: list[int],
) -> dict:
    """Assemble the `model.json` manifest body.

    Mirrors `modelPackageManifestToJson` in `src/asr/model/modelpackage.cpp`.
    """
    vocabulary_size = int(metadata.get("n_vocab", 0))
    if vocabulary_size <= WHISPER_TIMESTAMP_END_ID:
        raise SystemExit(
            "Package metadata inconsistency: vocabulary_size "
            f"({vocabulary_size}) must exceed timestamp_token_end_id "
            f"({WHISPER_TIMESTAMP_END_ID}); staging cannot proceed."
        )

    return {
        "format": MANIFEST_FORMAT,
        "schema_version": MANIFEST_SCHEMA_VERSION,
        "metadata": {
            "package_id": package_id,
            "display_name": display_name,
            "package_version": "1.0.0",
            "runtime_family": RUNTIME_FAMILY,
            "source_format": "huggingface-whisper-safetensors",
            "model_format": MODEL_FORMAT,
            "architecture": baseline_family,
            "language_profile": language_profile,
            "quantization": "f32",
            "tokenizer": TOKENIZER_KIND,
            "legacy_compatibility": False,
            "vocabulary_size": vocabulary_size,
            "audio_context": int(metadata.get("n_audio_ctx", 0)),
            "audio_state": int(metadata.get("n_audio_state", 0)),
            "audio_head_count": int(metadata.get("n_audio_head", 0)),
            "audio_layer_count": int(metadata.get("n_audio_layer", 0)),
            "text_context": int(metadata.get("n_text_ctx", 0)),
            "text_state": int(metadata.get("n_text_state", 0)),
            "text_head_count": int(metadata.get("n_text_head", 0)),
            "text_layer_count": int(metadata.get("n_text_layer", 0)),
            "mel_count": int(metadata.get("n_mels", 0)),
            "format_type": 0,
        },
        "native_execution": {
            "execution_version": EXECUTION_VERSION,
            "baseline_family": baseline_family,
            "decoder": DECODER_KIND,
            "tokenizer": TOKENIZER_KIND,
            "tokenizer_asset_role": TOKENIZER_ASSET_ROLE,
            "tokenizer_merges_asset_role": TOKENIZER_MERGES_ASSET_ROLE,
            "frontend": FRONTEND_KIND,
            "search_policy": SEARCH_POLICY,
            "timestamp_mode": TIMESTAMP_MODE,
            "feature_bin_count": 0,
            "template_count": 0,
            "max_distance": 0.0,
            "bos_token_id": WHISPER_BOS_TOKEN_ID,
            "eos_token_id": WHISPER_EOS_TOKEN_ID,
            "no_speech_token_id": WHISPER_NO_SPEECH_TOKEN_ID,
            "timestamp_token_start_id": WHISPER_TIMESTAMP_START_ID,
            "timestamp_token_end_id": WHISPER_TIMESTAMP_END_ID,
            "initial_prompt_token_ids": list(initial_prompt_token_ids),
            "suppressed_token_ids": list(suppressed_token_ids),
        },
        "compatible_engines": [
            {"engine": ENGINE_NAME, "model_format": MODEL_FORMAT},
        ],
        "assets": [weights_asset, vocab_asset, merges_asset],
        "source_artifact": source_artifact,
    }


def copy_to_output(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Stage a Whisper MKCPUR3 file into a Mutterkey native model package."
    )
    parser.add_argument("--weights", required=True, type=Path, help="Path to the .mkweights file.")
    parser.add_argument("--vocab-json", required=True, type=Path, help="Path to HuggingFace vocab.json.")
    parser.add_argument("--merges", required=True, type=Path, help="Path to HuggingFace merges.txt.")
    parser.add_argument(
        "--config-json",
        type=Path,
        help="Optional HuggingFace config.json. Defaults to config.json beside --weights when present.",
    )
    parser.add_argument(
        "--baseline",
        required=True,
        help="Baseline family marker recorded in the manifest (e.g. 'whisper-base-en').",
    )
    parser.add_argument("--output", required=True, type=Path, help="Output package directory.")
    parser.add_argument(
        "--package-id",
        help="Optional package id override. Defaults to --baseline when omitted.",
    )
    parser.add_argument(
        "--display-name",
        help="Optional human-readable package name. Defaults to --baseline.",
    )
    parser.add_argument(
        "--language-profile",
        default="en",
        help="Language profile marker recorded in manifest metadata (default: en).",
    )
    parser.add_argument(
        "--language",
        default="en",
        help="Language token for the packaged prompt sequence (default: en).",
    )
    parser.add_argument(
        "--task",
        default="transcribe",
        help="Task token for the packaged prompt sequence (default: transcribe).",
    )
    parser.add_argument(
        "--with-timestamps",
        action="store_true",
        help=(
            "Emit a prompt that requests timestamp tokens. Defaults to a "
            "no-timestamps prompt matching the Whisper-en convention."
        ),
    )
    parser.add_argument(
        "--source-artifact",
        default="",
        help="Optional free-form diagnostic describing the upstream conversion.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing files in the output directory.",
    )
    return parser.parse_args(argv)


def stage_package(args: argparse.Namespace) -> int:
    weights_src: Path = args.weights.resolve()
    vocab_src: Path = args.vocab_json.resolve()
    merges_src: Path = args.merges.resolve()
    output_dir: Path = args.output.resolve()

    for path, label in ((weights_src, "weights"), (vocab_src, "vocab-json"), (merges_src, "merges")):
        if not path.is_file():
            raise SystemExit(f"--{label} is not a file: {path}")

    output_dir.mkdir(parents=True, exist_ok=True)
    weights_dst = output_dir / weights_src.name
    vocab_dst = output_dir / "vocab.txt"
    merges_dst = output_dir / merges_src.name
    manifest_dst = output_dir / "model.json"

    for destination in (weights_dst, vocab_dst, merges_dst, manifest_dst):
        if destination.exists() and not args.force:
            raise SystemExit(
                f"Refusing to overwrite existing file without --force: {destination}"
            )

    header = read_mkcpur3_header(weights_src)
    declared_vocab = int(header.metadata.get("n_vocab", 0))
    config_src = args.config_json.resolve() if args.config_json is not None else weights_src.with_name("config.json")
    hf_config = read_json_object(config_src, "config") if config_src.is_file() else {}

    print(f"[1/4] Copying weights -> {weights_dst}")
    copy_to_output(weights_src, weights_dst)

    print(f"[2/4] Converting vocab.json -> {vocab_dst}")
    gaps = write_line_ordered_vocabulary(vocab_src, vocab_dst, declared_vocab)
    if gaps > 0:
        print(f"       filled {gaps} unused slot(s) with <|unused_N|> placeholders")

    print(f"[3/4] Copying merges.txt -> {merges_dst}")
    copy_to_output(merges_src, merges_dst)

    weights_asset = {
        "role": WEIGHTS_ASSET_ROLE,
        "path": weights_dst.name,
        "sha256": sha256_of(weights_dst),
        "size_bytes": weights_dst.stat().st_size,
    }
    vocab_asset = {
        "role": TOKENIZER_ASSET_ROLE,
        "path": vocab_dst.name,
        "sha256": sha256_of(vocab_dst),
        "size_bytes": vocab_dst.stat().st_size,
    }
    merges_asset = {
        "role": TOKENIZER_MERGES_ASSET_ROLE,
        "path": merges_dst.name,
        "sha256": sha256_of(merges_dst),
        "size_bytes": merges_dst.stat().st_size,
    }

    package_id = args.package_id or args.baseline
    display_name = args.display_name or args.baseline

    initial_prompt_token_ids = forced_decoder_prompt(hf_config, args.with_timestamps) or whisper_initial_prompt(
        baseline_family=args.baseline,
        language=args.language,
        task=args.task,
        include_timestamps=args.with_timestamps,
    )
    suppressed_token_ids = suppressed_tokens(hf_config)

    manifest = build_manifest(
        weights_asset=weights_asset,
        vocab_asset=vocab_asset,
        merges_asset=merges_asset,
        metadata=header.metadata,
        baseline_family=args.baseline,
        package_id=package_id,
        display_name=display_name,
        language_profile=args.language_profile,
        source_artifact=args.source_artifact,
        initial_prompt_token_ids=initial_prompt_token_ids,
        suppressed_token_ids=suppressed_token_ids,
    )

    print(f"[4/4] Writing manifest -> {manifest_dst}")
    with manifest_dst.open("w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2, sort_keys=False)
        handle.write("\n")

    print()
    print(f"Staged native package: {output_dir}")
    print(f"    weights: {weights_asset['size_bytes']} bytes, sha256={weights_asset['sha256'][:16]}…")
    print(f"    vocab:   {vocab_asset['size_bytes']} bytes, sha256={vocab_asset['sha256'][:16]}…")
    print(f"    merges:  {merges_asset['size_bytes']} bytes, sha256={merges_asset['sha256'][:16]}…")
    if suppressed_token_ids:
        print(f"    suppressed generation tokens: {len(suppressed_token_ids)}")
    return 0


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    return stage_package(args)


if __name__ == "__main__":
    sys.exit(main())
