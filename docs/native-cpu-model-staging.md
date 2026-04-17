# Staging a Whisper model for the native CPU backend

This page documents how to turn an upstream HuggingFace Whisper checkpoint
into a Mutterkey native model package (the `mkasr-v2` contract) that the
product-owned native CPU runtime can load. It covers three steps:

1. download the HuggingFace checkpoint
2. convert the safetensors weights into `.mkweights` (the MKCPUR3 tensor file)
3. stage the weights, tokenizer, and manifest into a package directory

The native CPU runtime consumes packages validated by
`ModelValidator::validatePackagePath`, so every artifact produced here must
match the manifest schema defined in `src/asr/model/modelpackage.cpp`.

## 1. Download a HuggingFace Whisper model

Pick a baseline family that the native CPU runtime is shape-compatible with
(`base.en`, `small.en`, and similarly sized English-only or multilingual
Whisper checkpoints). Clone it into a staging directory outside the repo so
no model binaries get committed accidentally:

```bash
STAGE_DIR="$HOME/whisper-base-en-staging"
mkdir -p "$STAGE_DIR"
hf_hub_download() {
    # Either `huggingface-cli download` or a manual download works; the goal
    # is to land these four files in the staging directory.
    :
}
```

Required files in `$STAGE_DIR`:

- `config.json`
- `model.safetensors`
- `vocab.json`
- `merges.txt`

The `merges.txt` lines must follow the HuggingFace BPE format (one `left
right` pair per line). The `vocab.json` is a JSON object mapping token
strings to integer ids.

## 2. Convert safetensors to MKCPUR3

Run the in-repo converter. It is a self-contained Python 3 script with no
third-party dependencies:

```bash
python3 tools/convert_safetensors_to_mkcpur3.py \
    "$STAGE_DIR" \
    "$STAGE_DIR/base-en.mkweights"
```

The script:

- reads `model.safetensors` and `config.json`
- maps HuggingFace tensor names to the names the MKCPUR3 loader expects
- reshapes the 3D encoder conv weights into the 2D layout the runtime uses
- writes a header-plus-tensor-directory MKCPUR3 file

Validate that the header looks right:

```bash
xxd -l 16 "$STAGE_DIR/base-en.mkweights"
# 00000000: 4d4b 4350 5552 3300 0300 0000 ...  MKCPURR3.........
```

If the magic or version changes, the runtime will reject the file at load
time.

## 3. Stage the package

A Mutterkey model package is a directory containing `model.json` plus the
packaged assets. The staging helper produces this layout from the outputs
of the previous step and the HuggingFace tokenizer files:

```bash
python3 tools/stage_native_package.py \
    --weights    "$STAGE_DIR/base-en.mkweights" \
    --vocab-json "$STAGE_DIR/vocab.json" \
    --merges     "$STAGE_DIR/merges.txt" \
    --config-json "$STAGE_DIR/config.json" \
    --baseline   whisper-base-en \
    --output     "$HOME/.local/share/mutterkey/models/whisper-base-en"
```

The helper:

- copies the MKCPUR3 weights into the output directory
- converts `vocab.json` (a JSON dict) into a line-ordered `vocab.txt` that
  matches what the runtime's `loadTokenizerVocabulary` expects; unused
  Whisper token ids are filled with stable `<|unused_N|>` placeholders so
  line count stays in lock-step with the declared vocabulary size
- copies `merges.txt` verbatim
- computes the SHA-256 and byte size of each staged asset
- writes a `model.json` manifest with the schema read by
  `modelPackageManifestFromJson`, including:
  - `format: "mutterkey.model-package"`, `schema_version: 1`
  - `metadata` populated from the MKCPUR3 header (layer counts, context
    sizes, vocabulary size, mel count)
  - `native_execution` with the real-decoder markers and the Whisper-family
    special-token ids the greedy search needs
  - `initial_prompt_token_ids` and `suppressed_token_ids` derived from
    HuggingFace `config.json` when it is available beside the weights or
    passed through `--config-json`
  - a single `compatible_engines` entry marking the package compatible
    with `mutterkey.cpu-reference` / `mkasr-v2`

For English-only Whisper checkpoints such as `base.en`, HuggingFace
`forced_decoder_ids` normally stages only the no-timestamps prompt token after
SOT. Multilingual checkpoints can use `--language` and `--task` when no
`forced_decoder_ids` are present.

## Installation and use

Install the produced directory under the default Mutterkey package location
(`~/.local/share/mutterkey/models/<package-id>`) or point `transcriber.
model_path` at it directly:

```bash
~/.local/bin/mutterkey config set transcriber.model_path \
    "$HOME/.local/share/mutterkey/models/whisper-base-en"
```

After that, a no-legacy Mutterkey build
(`-DMUTTERKEY_ENABLE_LEGACY_WHISPER=OFF`) can run entirely through the
native CPU runtime without the vendored `whisper.cpp` path.

## Troubleshooting

- **"Model package manifest not found"**: the validator expects a directory
  containing `model.json` or a direct path to that file. Re-run the staging
  helper with `--output` pointing at a directory, not a file.
- **"Native CPU decoder package is missing execution metadata"**: the
  staging helper writes `native_execution` automatically; this error means
  the manifest was edited afterward in a way that dropped required fields.
- **"Model asset hash does not match manifest"**: the staged assets were
  modified after staging. Re-run the helper with `--force` to rebuild the
  manifest and refresh the hashes.
- **"Native CPU decoder package is missing tokenizer merge assets"**: a
  non-BPE tokenizer was staged. The native CPU runtime currently requires
  Whisper-family BPE; other tokenizers are not supported yet.

## Repository hygiene

- Never commit `.mkweights`, `.safetensors`, or staged package directories
  to the Mutterkey repository. Keep them in user-local staging paths.
- The staging helper is idempotent given the same inputs, so re-running it
  regenerates the same manifest (minus new hash timestamps in unrelated
  fields).
- When the native execution contract changes, update this page along with
  `src/asr/model/modelpackage.cpp` and
  `src/asr/model/modelvalidator.cpp` so staged packages stay loadable.
