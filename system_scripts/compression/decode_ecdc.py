#!/usr/bin/env python3
import argparse
import struct
import torch
import torchaudio
import numpy as np
from encodec import EncodecModel


def unpack_codes(data_bytes, B, N, T, nbits=10):
    """Unpack bytes into [B,N,T] integer tensor."""
    total_codes = B * N * T
    codes = np.zeros(total_codes, dtype=np.uint32)
    bitstream = 0
    bits_in_buffer = 0
    byte_index = 0
    code_index = 0
    while code_index < total_codes:
        while bits_in_buffer < nbits and byte_index < len(data_bytes):
            bitstream = (bitstream << 8) | data_bytes[byte_index]
            bits_in_buffer += 8
            byte_index += 1
        if bits_in_buffer >= nbits:
            bits_in_buffer -= nbits
            codes[code_index] = (bitstream >> bits_in_buffer) & ((1 << nbits) - 1)
            bitstream &= (1 << bits_in_buffer) - 1
            code_index += 1
        else:
            codes[code_index] = (bitstream << (nbits - bits_in_buffer)) & ((1 << nbits) - 1)
            code_index += 1
            bits_in_buffer = 0
    return torch.from_numpy(codes.reshape(B, N, T)).long()


def unpack_int_sequence(data_bytes, total_values, nbits):
    """Unpack bitstream bytes into a 1-D numpy array of integers."""
    values = np.zeros(total_values, dtype=np.uint32)
    bitstream = 0
    bits_in_buffer = 0
    byte_index = 0
    value_index = 0
    while value_index < total_values:
        while bits_in_buffer < nbits and byte_index < len(data_bytes):
            bitstream = (bitstream << 8) | data_bytes[byte_index]
            bits_in_buffer += 8
            byte_index += 1
        if bits_in_buffer >= nbits:
            bits_in_buffer -= nbits
            values[value_index] = (bitstream >> bits_in_buffer) & ((1 << nbits) - 1)
            bitstream &= (1 << bits_in_buffer) - 1
        else:
            values[value_index] = (bitstream << (nbits - bits_in_buffer)) & ((1 << nbits) - 1)
            bits_in_buffer = 0
        value_index += 1
    return values


def map_tensors(obj, mapper):
    if torch.is_tensor(obj):
        return mapper(obj)
    if isinstance(obj, list):
        return [map_tensors(item, mapper) for item in obj]
    if isinstance(obj, tuple):
        return tuple(map_tensors(item, mapper) for item in obj)
    if isinstance(obj, dict):
        return {key: map_tensors(val, mapper) for key, val in obj.items()}
    return obj


def try_load_snac_bitstream(path, fallback_model):
    """Return (model, sample_rate, codes) if file uses the compact SNAC format."""
    with open(path, "rb") as f:
        magic = f.read(4)
        if magic != b"SNAC":
            return None

        version_byte = f.read(1)
        if len(version_byte) != 1:
            raise SystemExit("Corrupted SNAC file: missing version byte.")
        _version = struct.unpack("<B", version_byte)[0]
        sample_rate_bytes = f.read(4)
        if len(sample_rate_bytes) != 4:
            raise SystemExit("Corrupted SNAC file: missing sample rate.")
        sample_rate = struct.unpack("<I", sample_rate_bytes)[0]

        name_len_bytes = f.read(2)
        if len(name_len_bytes) != 2:
            raise SystemExit("Corrupted SNAC file: missing model length.")
        name_len = struct.unpack("<H", name_len_bytes)[0]
        model_name = f.read(name_len).decode("utf-8") if name_len else fallback_model

        num_levels_bytes = f.read(4)
        if len(num_levels_bytes) != 4:
            raise SystemExit("Corrupted SNAC file: missing level count.")
        num_levels = struct.unpack("<I", num_levels_bytes)[0]

        level_meta = []
        for _ in range(num_levels):
            length_bytes = f.read(4)
            nbits_bytes = f.read(2)
            chunk_bytes = f.read(4)
            if len(length_bytes) != 4 or len(nbits_bytes) != 2 or len(chunk_bytes) != 4:
                raise SystemExit("Corrupted SNAC file: incomplete level metadata.")
            length = struct.unpack("<I", length_bytes)[0]
            nbits = struct.unpack("<H", nbits_bytes)[0]
            payload_size = struct.unpack("<I", chunk_bytes)[0]
            level_meta.append((length, nbits, payload_size))

        codes = []
        for length, nbits, payload_size in level_meta:
            payload = f.read(payload_size)
            if len(payload) != payload_size:
                raise SystemExit("Corrupted SNAC file: unexpected EOF inside payload.")
            values = unpack_int_sequence(payload, length, nbits)
            tensor = torch.from_numpy(values.astype(np.int64)).unsqueeze(0)
            codes.append(tensor)

    return model_name or fallback_model, sample_rate, codes


def require_snac():
    try:
        from snac import SNAC  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            "SNAC support requires the 'snac' package. Install via: "
            "pip install git+https://github.com/hubertsiuzdak/snac"
        ) from exc
    return SNAC


def decode_with_encodec(args):
    frames = []
    with open(args.input, "rb") as f:
        header = f.read(12)
        if len(header) != 12:
            raise SystemExit("Invalid EnCodec container header")
        sample_rate, channels, num_frames = np.frombuffer(header, dtype=np.int32)
        for _ in range(int(num_frames)):
            shape_bytes = f.read(12)
            if len(shape_bytes) != 12:
                raise SystemExit("Corrupted frame header in EnCodec file")
            B, N, T = np.frombuffer(shape_bytes, dtype=np.int32)
            nbits = 10
            num_bytes = (int(B) * int(N) * int(T) * nbits + 7) // 8
            data_bytes = f.read(num_bytes)
            if len(data_bytes) != num_bytes:
                raise SystemExit("Unexpected end of file while reading EnCodec codes")
            codes = unpack_codes(data_bytes, int(B), int(N), int(T), nbits)
            frames.append((codes, None))

    model = EncodecModel.encodec_model_24khz()
    model.eval()

    with torch.no_grad():
        decoded = model.decode(frames)

    decoded = decoded.squeeze(0)
    torchaudio.save(args.output, decoded, int(sample_rate))
    print(f"Decoded {args.input} -> {args.output}")


def decode_with_snac(args):
    SNAC = require_snac()
    device = torch.device("cpu")

    parsed = try_load_snac_bitstream(args.input, args.snac_model)
    if parsed is None:
        payload = torch.load(args.input, map_location=device)
        model_name = payload.get("model", args.snac_model)
        sample_rate = payload.get("sample_rate", args.snac_sample_rate)
        codes = payload.get("codes")
        if codes is None:
            raise SystemExit("SNAC container missing 'codes'.")
    else:
        model_name, sample_rate, codes = parsed

    model = SNAC.from_pretrained(model_name).to(device)
    model.eval()

    codes = map_tensors(codes, lambda tensor: tensor.to(device))
    with torch.no_grad():
        decoded = model.decode(codes)

    decoded = decoded.squeeze(0).cpu()
    torchaudio.save(args.output, decoded, int(sample_rate))
    print(f"Decoded {args.input} -> {args.output} using SNAC model {model_name}")


def main():
    parser = argparse.ArgumentParser(description="Neural codec decoder")
    parser.add_argument("input", help="input file (.ecdc | .snac)")
    parser.add_argument("output", help="output WAV")
    parser.add_argument("--codec", choices=["encodec", "snac"], default="encodec", help="codec backend")
    parser.add_argument("--threads", type=int, default=1, help="Torch thread count")
    parser.add_argument("--snac-model", default="hubertsiuzdak/snac_24khz", help="HuggingFace repo for SNAC")
    parser.add_argument("--snac-sample-rate", type=int, default=24000, help="Fallback SNAC sample rate")
    args = parser.parse_args()

    torch.set_num_threads(args.threads)

    if args.codec == "encodec":
        decode_with_encodec(args)
    else:
        decode_with_snac(args)


if __name__ == "__main__":
    main()
