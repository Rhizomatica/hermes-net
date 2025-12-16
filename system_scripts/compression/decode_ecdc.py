#!/usr/bin/env python3
import argparse
import torch
import torchaudio
import numpy as np
from encodec import EncodecModel

def unpack_codes(data_bytes, B, N, T, nbits=10):
    """Unpack bytes into [B,N,T] integer tensor"""
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
            # last code (pad zeros)
            codes[code_index] = (bitstream << (nbits - bits_in_buffer)) & ((1 << nbits) - 1)
            code_index += 1
            bits_in_buffer = 0
    return torch.from_numpy(codes.reshape(B, N, T)).long()

def main():
    parser = argparse.ArgumentParser(description="EnCodec decoder (~1.5 kbps)")
    parser.add_argument("input", help="input .ecdc file")
    parser.add_argument("output", help="output WAV")
    parser.add_argument("--threads", type=int, default=1)
    args = parser.parse_args()

    torch.set_num_threads(args.threads)

    frames = []
    with open(args.input, "rb") as f:
        # Read header
        sample_rate, channels, num_frames = np.frombuffer(f.read(12), dtype=np.int32)
        for _ in range(num_frames):
            B, N, T = np.frombuffer(f.read(12), dtype=np.int32)
            nbits = 10
            num_bytes = (B * N * T * nbits + 7) // 8
            data_bytes = f.read(num_bytes)
            codes = unpack_codes(data_bytes, B, N, T, nbits)
            frames.append((codes, None))  # scale=None

    model = EncodecModel.encodec_model_24khz()
    model.eval()

    with torch.no_grad():
        decoded = model.decode(frames)

    decoded = decoded.squeeze(0)
    torchaudio.save(args.output, decoded, sample_rate)
    print(f"Decoded {args.input} -> {args.output}")

if __name__ == "__main__":
    main()
