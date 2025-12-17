#!/usr/bin/env python3
import argparse
import math
import struct
import torch
import torchaudio
import numpy as np
from encodec import EncodecModel
from encodec.utils import convert_audio


def pack_codes(codes, nbits=10):
    """Pack [B,N,T] integer codes into a bitstream (bytes)."""
    codes = codes.cpu().numpy().flatten().astype(np.uint32)
    bitstream = 0
    bits_in_buffer = 0
    out_bytes = bytearray()
    for code in codes:
        bitstream = (bitstream << nbits) | int(code)
        bits_in_buffer += nbits
        while bits_in_buffer >= 8:
            bits_in_buffer -= 8
            out_bytes.append((bitstream >> bits_in_buffer) & 0xFF)
    if bits_in_buffer > 0:
        out_bytes.append((bitstream << (8 - bits_in_buffer)) & 0xFF)
    return bytes(out_bytes)


def require_snac():
    try:
        from snac import SNAC  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            "SNAC support requires the 'snac' package. Install via: "
            "pip install git+https://github.com/hubertsiuzdak/snac"
        ) from exc
    return SNAC


def pack_int_sequence(values, nbits):
    """Bit-pack a flat iterable of integers using nbits per sample."""
    bitstream = 0
    bits_in_buffer = 0
    out_bytes = bytearray()
    for value in values:
        bitstream = (bitstream << nbits) | int(value)
        bits_in_buffer += nbits
        while bits_in_buffer >= 8:
            bits_in_buffer -= 8
            out_bytes.append((bitstream >> bits_in_buffer) & 0xFF)
    if bits_in_buffer > 0:
        out_bytes.append((bitstream << (8 - bits_in_buffer)) & 0xFF)
    return bytes(out_bytes)


def serialize_snac_codes(code_tensors, sample_rate, model_name, output_path):
    """Write SNAC codes to a compact binary container."""
    model_bytes = model_name.encode("utf-8")
    with open(output_path, "wb") as f:
        f.write(b"SNAC")  # magic
        f.write(struct.pack("<B", 1))  # version
        f.write(struct.pack("<I", int(sample_rate)))
        f.write(struct.pack("<H", len(model_bytes)))
        f.write(model_bytes)
        f.write(struct.pack("<I", len(code_tensors)))

        data_chunks = []
        for tensor in code_tensors:
            flat = (
                tensor.detach()
                .cpu()
                .squeeze(0)
                .to(torch.int64)
                .reshape(-1)
                .numpy()
                .astype(np.int32)
            )
            max_val = int(flat.max()) if flat.size else 0
            nbits = 1 if max_val == 0 else max(1, math.ceil(math.log2(max_val + 1)))
            packed = pack_int_sequence(flat, nbits)
            f.write(struct.pack("<I", flat.size))
            f.write(struct.pack("<H", nbits))
            f.write(struct.pack("<I", len(packed)))
            data_chunks.append(packed)

        for chunk in data_chunks:
            f.write(chunk)


def encode_with_encodec(args):
    model = EncodecModel.encodec_model_24khz()
    model.set_target_bandwidth(args.bitrate)
    model.eval()

    wav, sr = torchaudio.load(args.input)
    wav = convert_audio(wav, sr, model.sample_rate, model.channels)
    wav = wav.unsqueeze(0)

    with torch.no_grad():
        encoded_frames = model.encode(wav)

    with open(args.output, "wb") as f:
        f.write(
            np.array([model.sample_rate, model.channels, len(encoded_frames)], dtype=np.int32).tobytes()
        )
        for frame in encoded_frames:
            codes, _ = frame
            B, N, T = codes.shape
            f.write(np.array([B, N, T], dtype=np.int32).tobytes())
            f.write(pack_codes(codes, nbits=10))
    print(f"Encoded {args.input} -> {args.output} (~{args.bitrate} kbps)")


def encode_with_snac(args):
    SNAC = require_snac()
    device = torch.device("cpu")

    model = SNAC.from_pretrained(args.snac_model).to(device)
    model.eval()

    wav, sr = torchaudio.load(args.input)
    wav = convert_audio(wav, sr, args.snac_sample_rate, 1)
    wav = wav.unsqueeze(0).to(device)

    with torch.no_grad():
        codes = model.encode(wav)

    serialize_snac_codes(codes, args.snac_sample_rate, args.snac_model, args.output)
    print(f"Encoded {args.input} -> {args.output} using SNAC model {args.snac_model}")


def main():
    parser = argparse.ArgumentParser(description="Neural codec encoder")
    parser.add_argument("input", help="input audio file")
    parser.add_argument("output", help="output file (.ecdc | .snac)")
    parser.add_argument("--codec", choices=["encodec", "snac"], default="encodec", help="codec backend")
    parser.add_argument("--bitrate", type=float, default=1.5, help="target bandwidth kbps (EnCodec only)")
    parser.add_argument("--threads", type=int, default=1, help="Torch thread count")
    parser.add_argument("--snac-model", default="hubertsiuzdak/snac_24khz", help="HuggingFace repo for SNAC")
    parser.add_argument("--snac-sample-rate", type=int, default=24000, help="Target sample rate for SNAC")
    args = parser.parse_args()

    torch.set_num_threads(args.threads)

    if args.codec == "encodec":
        encode_with_encodec(args)
    else:
        encode_with_snac(args)


if __name__ == "__main__":
    main()
