# Neural + Legacy Audio Compression Toolkit

Shell helpers in this directory make it easy to move audio between regular files (`.wav`, `.mp3`, `.aac`, …) and several ultra‑low bitrate codecs. Everything runs on CPU and automatically performs the FFmpeg resampling/conversion that each codec expects.

## Contents

| File | Purpose |
| --- | --- |
| `compress_audio.sh` | Front-end that inspects the requested extension (`.lpcnet`, `.nesc`, `.ecdc`, `.snac`) and invokes the proper encoder. |
| `decompress_audio.sh` | Symmetric decoder that converts compressed files back to standard audio formats. |
| `encode_ecdc.py` | Python helper that wraps the EnCodec reference model **and** the SNAC encoder bitstream writer. |
| `decode_ecdc.py` | Counterpart decoder for EnCodec bitstreams and the compact SNAC container. |
| `requirements.txt` | Python dependencies (torchaudio, encodec, snac, etc.). |

## Installation

1. Install FFmpeg and Python 3.8+.
2. Install Python deps (PyTorch, EnCodec, SNAC, Hugging Face tooling, etc.) via the curated `requirements.txt`:
   ```bash
   pip install -r requirements.txt
   ```
   - Core: `torch`, `torchaudio`, `numpy`, `soundfile`, `encodec`, `huggingface-hub`
   - Optional codecs: `descript-audio-codec`, SemantiCodec (`git+https://github.com/haoheliu/SemantiCodec-inference`)
   - SNAC: pulled directly from `git+https://github.com/hubertsiuzdak/snac`
3. (Optional) configure Hugging Face caching if you run these scripts offline later. The first SNAC run pulls the model weights from `hubertsiuzdak/*`.

## Quick start

```bash
# Compress using any supported codec (extension selects encoder)
./compress_audio.sh input.wav output.ecdc
./compress_audio.sh input.wav output.lpcnet
./compress_audio.sh input.wav output.snac

# Decompress back to a playable format
./decompress_audio.sh output.ecdc restored.wav
./decompress_audio.sh output.snac restored.wav
```

The scripts normalize inputs to mono and resample to the target model’s sample rate, so you can feed them AAC/MP3/WAV interchangeably.

## Codec matrix

| Extension | Codec | Notes |
| --- | --- | --- |
| `.lpcnet` | LPCNet demo binary (`/opt/lpcnet/lpcnet_demo`) | Speech-only legacy codec. |
| `.nesc` | NESC reference binary (`/opt/nesc/nesc_enc`) | Requires 16 kHz PCM front-end. |
| `.ecdc` | Meta EnCodec 24 kHz | Uses `encode_ecdc.py` + PyTorch. Bitrate controlled via `ENCODEC_BITRATE`. |
| `.snac` | Hubert Siuzdak’s SNAC | Uses the compact container implemented in `encode_ecdc.py`. Defaults to `snac_24khz` (0.98 kbps speech mode). |

## Configuration knobs

| Variable | Default | Description |
| --- | --- | --- |
| `PYTHON_BIN` | `python3` | Interpreter that runs the helper scripts. |
| `TORCH_THREADS` | `1` | Number of CPU threads given to PyTorch. |
| `ENCODEC_ENC` / `ENCODEC_DEC` | Local helper paths | Override if you relocate the Python scripts. |
| `ENCODEC_BITRATE` | `1.5` | Target kbps for EnCodec; accepts 1.5, 3, 6, 12, 24. |
| `SNAC_VARIANT` | `24khz` | Quick selector for SNAC model: `24khz`, `32khz`, or `44khz`. Sets `SNAC_MODEL` and `SNAC_SAMPLE_RATE` automatically. |
| `SNAC_MODEL` | `hubertsiuzdak/snac_24khz` | Hugging Face repo to load. Determines bitrate + architecture. (Override `SNAC_VARIANT` if you need custom models.) |
| `SNAC_SAMPLE_RATE` | `24000` | Resample target used before encoding and the fallback rate during decoding. (Override `SNAC_VARIANT` if you need custom rates.) |

### EnCodec bandwidth examples

```bash
export ENCODEC_BITRATE=6.0   # higher quality
./compress_audio.sh vox.wav vox.ecdc

export ENCODEC_BITRATE=1.5   # lowest bitrate
./compress_audio.sh vox.wav vox.ecdc
```

## SNAC modes and how to switch

The SNAC helper can talk to any published checkpoint; you just point it at the desired Hugging Face repo and (optionally) adjust the resample rate. Common presets:

| Model | Sample rate | Nominal bitrate* | Use case |
| --- | --- | --- | --- |
| `hubertsiuzdak/snac_24khz` | 24 kHz | **0.98 kbps** | Ultra-low bitrate speech (default). |
| `hubertsiuzdak/snac_32khz` | 32 kHz | ~1.95 kbps | Higher fidelity speech/music. |

_* Bitrate depends on content length; numbers above are the original paper’s guidance._

### Switching modes for **encoding**

```bash
# Example: move from the default 24 kHz / 0.98 kbps mode to the 32 kHz model
export SNAC_MODEL=hubertsiuzdak/snac_32khz
export SNAC_SAMPLE_RATE=32000
./compress_audio.sh speech.wav speech.snac
```

The shell script passes these env vars directly to `encode_ecdc.py`, so every `.snac` produced in that session will use the selected checkpoint. When you need to bounce back to the 0.98 kbps mode, unset or reset the variables:

```bash
unset SNAC_MODEL SNAC_SAMPLE_RATE   # or set them back to 24k explicitly
```

### Decoding files from different modes

Each `.snac` file written by the new encoder stores its model name, sample rate, and bit-packed codebooks. `decompress_audio.sh` reads that metadata automatically. The only time you need the env vars during decoding is when you’re opening **legacy** SNAC files that were saved before the new container existed; in that case set `SNAC_MODEL`/`SNAC_SAMPLE_RATE` to match the encoder that produced them so the helper knows which checkpoint to download.

## Troubleshooting

- **Missing binary/FFmpeg**: the scripts will exit with a clear message if an encoder/decoder binary is absent. Install the missing package or update the environment variable to point to your build.
- **First-run SNAC latency**: the Hugging Face download can take a minute. Subsequent runs hit the local cache (`~/.cache/huggingface/hub`).
- **Legacy `.snac` files**: the decoder still supports the original `torch.save` container format; it auto-detects and converts it on the fly.
- **SAC/SemantiCodec**: placeholders exist in `neural_codec.py`, but those flows remain experimental and may require manual installs.

With these knobs documented, you can hop between EnCodec bitrates or SNAC presets just by exporting the relevant environment variables before running the shell scripts.
