#!/usr/bin/env python3
import argparse
import torch
import torchaudio
import numpy as np
from encodec import EncodecModel
from encodec.utils import convert_audio

# ffmpeg -i in.wav -ac 1 -ar 24000 -sample_fmt s16 in_24k.wav
# ./encodec_test.py in_24k.wav out.wav --bitrate 1.5 --threads 1
# --bitrate 3.0
# --bitrate 6.0


def pack_codes(codes, nbits=10):
    """Pack [B,N,T] integer codes into a bitstream (bytes)"""
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

def main():
    parser = argparse.ArgumentParser(description="EnCodec encoder (~1.5 kbps)")
    parser.add_argument("input", help="input WAV (mono, 24 kHz recommended)")
    parser.add_argument("output", help="output .ecdc file")
    parser.add_argument("--bitrate", type=float, default=1.5, help="target bandwidth kbps")
    parser.add_argument("--threads", type=int, default=1)
    args = parser.parse_args()

    torch.set_num_threads(args.threads)

    # Load model
    model = EncodecModel.encodec_model_24khz()
    model.set_target_bandwidth(args.bitrate)
    model.eval()

    # Load audio
    wav, sr = torchaudio.load(args.input)
    wav = convert_audio(wav, sr, model.sample_rate, model.channels)
    wav = wav.unsqueeze(0)

    # Encode
    with torch.no_grad():
        encoded_frames = model.encode(wav)

    # Write binary file
    with open(args.output, "wb") as f:
        # Header: sample_rate, channels, num_frames
        f.write(np.array([model.sample_rate, model.channels, len(encoded_frames)], dtype=np.int32).tobytes())
        for frame in encoded_frames:
            codes, scale = frame
            B, N, T = codes.shape
            # Store frame shape
            f.write(np.array([B, N, T], dtype=np.int32).tobytes())
            # Pack codes
            f.write(pack_codes(codes, nbits=10))
    print(f"Encoded {args.input} -> {args.output} (~{args.bitrate} kbps)")

if __name__ == "__main__":
    main()
