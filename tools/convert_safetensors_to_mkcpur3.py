#!/usr/bin/env python3
"""Convert a HuggingFace Whisper safetensors checkpoint to MKCPUR3 format.

Usage:
    python3 convert_safetensors_to_mkcpur3.py <model_dir> <output_path>

The model directory must contain model.safetensors and config.json.
No pip dependencies are required.
"""

import json
import struct
import sys
from pathlib import Path


# MKCPUR3 format constants.
MAGIC = b"MKCPUR3\x00"
FORMAT_VERSION = 3
DTYPE_F32 = 0


def read_safetensors(path: Path) -> tuple[dict, memoryview]:
    """Read safetensors file, return (header_dict, data_memoryview)."""
    with open(path, "rb") as f:
        header_len = struct.unpack("<Q", f.read(8))[0]
        header = json.loads(f.read(header_len))
        data = f.read()
    return header, memoryview(data)


def extract_tensor(header: dict, data: memoryview, name: str) -> tuple[list[int], memoryview]:
    """Extract a single tensor's shape and raw f32 bytes from safetensors."""
    entry = header[name]
    assert entry["dtype"] == "F32", f"Expected F32 for {name}, got {entry['dtype']}"
    start, end = entry["data_offsets"]
    return entry["shape"], data[start:end]


def build_name_mapping(num_encoder_layers: int, num_decoder_layers: int) -> dict[str, str]:
    """Build safetensors->MKCPUR3 name mapping for all tensors."""
    mapping = {
        # Encoder non-layer tensors.
        "model.encoder.conv1.weight": "encoder.conv1.weight",
        "model.encoder.conv1.bias": "encoder.conv1.bias",
        "model.encoder.conv2.weight": "encoder.conv2.weight",
        "model.encoder.conv2.bias": "encoder.conv2.bias",
        "model.encoder.embed_positions.weight": "encoder.positional_embedding",
        "model.encoder.layer_norm.weight": "encoder.ln_post.weight",
        "model.encoder.layer_norm.bias": "encoder.ln_post.bias",
        # Decoder non-layer tensors.
        "model.decoder.embed_tokens.weight": "decoder.token_embedding.weight",
        "model.decoder.embed_positions.weight": "decoder.positional_embedding",
        "model.decoder.layer_norm.weight": "decoder.ln.weight",
        "model.decoder.layer_norm.bias": "decoder.ln.bias",
    }

    # Encoder layer tensors.
    for i in range(num_encoder_layers):
        p = f"model.encoder.layers.{i}"
        o = f"encoder.blocks.{i}"
        mapping.update({
            f"{p}.self_attn.q_proj.weight": f"{o}.attn.query.weight",
            f"{p}.self_attn.q_proj.bias": f"{o}.attn.query.bias",
            f"{p}.self_attn.k_proj.weight": f"{o}.attn.key.weight",
            # Key has no bias in Whisper.
            f"{p}.self_attn.v_proj.weight": f"{o}.attn.value.weight",
            f"{p}.self_attn.v_proj.bias": f"{o}.attn.value.bias",
            f"{p}.self_attn.out_proj.weight": f"{o}.attn.out.weight",
            f"{p}.self_attn.out_proj.bias": f"{o}.attn.out.bias",
            f"{p}.self_attn_layer_norm.weight": f"{o}.attn_ln.weight",
            f"{p}.self_attn_layer_norm.bias": f"{o}.attn_ln.bias",
            f"{p}.final_layer_norm.weight": f"{o}.mlp_ln.weight",
            f"{p}.final_layer_norm.bias": f"{o}.mlp_ln.bias",
            f"{p}.fc1.weight": f"{o}.mlp.0.weight",
            f"{p}.fc1.bias": f"{o}.mlp.0.bias",
            f"{p}.fc2.weight": f"{o}.mlp.2.weight",
            f"{p}.fc2.bias": f"{o}.mlp.2.bias",
        })

    # Decoder layer tensors.
    for i in range(num_decoder_layers):
        p = f"model.decoder.layers.{i}"
        o = f"decoder.blocks.{i}"
        mapping.update({
            # Self-attention.
            f"{p}.self_attn.q_proj.weight": f"{o}.attn.query.weight",
            f"{p}.self_attn.q_proj.bias": f"{o}.attn.query.bias",
            f"{p}.self_attn.k_proj.weight": f"{o}.attn.key.weight",
            # Key has no bias.
            f"{p}.self_attn.v_proj.weight": f"{o}.attn.value.weight",
            f"{p}.self_attn.v_proj.bias": f"{o}.attn.value.bias",
            f"{p}.self_attn.out_proj.weight": f"{o}.attn.out.weight",
            f"{p}.self_attn.out_proj.bias": f"{o}.attn.out.bias",
            f"{p}.self_attn_layer_norm.weight": f"{o}.attn_ln.weight",
            f"{p}.self_attn_layer_norm.bias": f"{o}.attn_ln.bias",
            # Cross-attention.
            f"{p}.encoder_attn.q_proj.weight": f"{o}.cross_attn.query.weight",
            f"{p}.encoder_attn.q_proj.bias": f"{o}.cross_attn.query.bias",
            f"{p}.encoder_attn.k_proj.weight": f"{o}.cross_attn.key.weight",
            # Key has no bias.
            f"{p}.encoder_attn.v_proj.weight": f"{o}.cross_attn.value.weight",
            f"{p}.encoder_attn.v_proj.bias": f"{o}.cross_attn.value.bias",
            f"{p}.encoder_attn.out_proj.weight": f"{o}.cross_attn.out.weight",
            f"{p}.encoder_attn.out_proj.bias": f"{o}.cross_attn.out.bias",
            f"{p}.encoder_attn_layer_norm.weight": f"{o}.cross_attn_ln.weight",
            f"{p}.encoder_attn_layer_norm.bias": f"{o}.cross_attn_ln.bias",
            # FFN.
            f"{p}.final_layer_norm.weight": f"{o}.mlp_ln.weight",
            f"{p}.final_layer_norm.bias": f"{o}.mlp_ln.bias",
            f"{p}.fc1.weight": f"{o}.mlp.0.weight",
            f"{p}.fc1.bias": f"{o}.mlp.0.bias",
            f"{p}.fc2.weight": f"{o}.mlp.2.weight",
            f"{p}.fc2.bias": f"{o}.mlp.2.bias",
        })

    return mapping


# Tensors that need 3D -> 2D reshaping (conv weights).
RESHAPE_3D = {"encoder.conv1.weight", "encoder.conv2.weight"}


def reshape_if_needed(mkcpur3_name: str, shape: list[int], raw: bytes) -> tuple[list[int], bytes]:
    """Reshape 3D conv weights [out, in, k] to 2D [out, in*k]."""
    if mkcpur3_name in RESHAPE_3D and len(shape) == 3:
        new_shape = [shape[0], shape[1] * shape[2]]
        return new_shape, raw
    return shape, raw


def build_mkcpur3_metadata(config: dict) -> bytes:
    """Build the JSON metadata section from the HuggingFace config."""
    metadata = {
        "n_mels": config.get("num_mel_bins", 80),
        "n_audio_ctx": config.get("max_source_positions", 1500),
        "n_audio_state": config.get("d_model", 512),
        "n_audio_head": config.get("encoder_attention_heads", 8),
        "n_audio_layer": config.get("encoder_layers", 6),
        "n_text_ctx": config.get("max_target_positions", 448),
        "n_text_state": config.get("d_model", 512),
        "n_text_head": config.get("decoder_attention_heads", 8),
        "n_text_layer": config.get("decoder_layers", 6),
        "n_vocab": config.get("vocab_size", 51864),
    }
    return json.dumps(metadata, separators=(",", ":")).encode("utf-8")


def write_tensor_directory_entry(name: str, shape: list[int], data_offset: int, data_size: int) -> bytes:
    """Encode one tensor directory entry."""
    name_bytes = name.encode("utf-8")
    buf = struct.pack("<I", len(name_bytes))
    buf += name_bytes
    n_dims = len(shape)
    buf += struct.pack("<I", n_dims)
    for d in shape:
        buf += struct.pack("<I", d)
    buf += struct.pack("<I", DTYPE_F32)  # dtype
    buf += struct.pack("<q", data_offset)  # data offset
    buf += struct.pack("<q", data_size)  # data size
    return buf


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <model_dir> <output_path>", file=sys.stderr)
        sys.exit(1)

    model_dir = Path(sys.argv[1])
    output_path = Path(sys.argv[2])

    safetensors_path = model_dir / "model.safetensors"
    config_path = model_dir / "config.json"

    if not safetensors_path.exists():
        print(f"Error: {safetensors_path} not found", file=sys.stderr)
        sys.exit(1)
    if not config_path.exists():
        print(f"Error: {config_path} not found", file=sys.stderr)
        sys.exit(1)

    with open(config_path) as f:
        config = json.load(f)

    print(f"Reading {safetensors_path} ...")
    header, data = read_safetensors(safetensors_path)

    num_encoder_layers = config.get("encoder_layers", 6)
    num_decoder_layers = config.get("decoder_layers", 6)
    name_mapping = build_name_mapping(num_encoder_layers, num_decoder_layers)

    # Check for unmapped tensors.
    st_names = {k for k in header if k != "__metadata__"}
    mapped_st_names = set(name_mapping.keys())
    unmapped = st_names - mapped_st_names
    if unmapped:
        print(f"Warning: {len(unmapped)} unmapped tensors in safetensors:")
        for name in sorted(unmapped):
            print(f"  {name}")

    missing = mapped_st_names - st_names
    if missing:
        print(f"Error: {len(missing)} expected tensors missing from safetensors:")
        for name in sorted(missing):
            print(f"  {name}")
        sys.exit(1)

    # Build the list of tensors to write.
    tensors = []  # (mkcpur3_name, shape, raw_bytes)
    for st_name, mk_name in sorted(name_mapping.items(), key=lambda x: x[1]):
        if st_name not in st_names:
            continue
        shape, raw = extract_tensor(header, data, st_name)
        shape, raw = reshape_if_needed(mk_name, shape, raw)
        tensors.append((mk_name, shape, raw))

    print(f"Converting {len(tensors)} tensors ...")

    # Build metadata.
    metadata_bytes = build_mkcpur3_metadata(config)

    # Build tensor directory and compute data offsets.
    data_offset = 0
    directory_buf = b""
    for mk_name, shape, raw in tensors:
        directory_buf += write_tensor_directory_entry(mk_name, shape, data_offset, len(raw))
        data_offset += len(raw)

    # Write output file.
    with open(output_path, "wb") as f:
        # Header: magic (8) + version (4) + tensor_count (4) + metadata_bytes_len (4).
        f.write(MAGIC)
        f.write(struct.pack("<I", FORMAT_VERSION))
        f.write(struct.pack("<I", len(tensors)))
        f.write(struct.pack("<I", len(metadata_bytes)))

        # JSON metadata.
        f.write(metadata_bytes)

        # Tensor directory.
        f.write(directory_buf)

        # Tensor data.
        for _, _, raw in tensors:
            f.write(raw)

    file_size_mb = output_path.stat().st_size / (1024 * 1024)
    print(f"Wrote {output_path} ({file_size_mb:.1f} MB, {len(tensors)} tensors)")


if __name__ == "__main__":
    main()
