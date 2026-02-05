#!/bin/bash
# usage:
# compress_audio.sh audio_filename.{wav,mp3,aac,...} output.{lpcnet,nesc,ecdc,snac}

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# External tools (can be overridden via env vars)
LPCNET_ENC=${LPCNET_ENC:=/opt/lpcnet/lpcnet_demo}
NESC_ENC=${NESC_ENC:=/opt/nesc/nesc_enc}
PYTHON_BIN=${PYTHON_BIN:=python3}

# Neural codec helpers
ENCODEC_ENC=${ENCODEC_ENC:="${SCRIPT_DIR}/encode_ecdc.py"}
ENCODEC_BITRATE=${ENCODEC_BITRATE:=1.5}
TORCH_THREADS=${TORCH_THREADS:=1}

# SNAC model configuration
# Support: SNAC_VARIANT can be "24khz", "32khz", or "44khz"
# Or directly set SNAC_MODEL and SNAC_SAMPLE_RATE
SNAC_VARIANT=${SNAC_VARIANT:=24khz}

case "${SNAC_VARIANT}" in
  24khz)
    SNAC_MODEL=${SNAC_MODEL:=hubertsiuzdak/snac_24khz}
    SNAC_SAMPLE_RATE=${SNAC_SAMPLE_RATE:=24000}
    ;;
  32khz)
    SNAC_MODEL=${SNAC_MODEL:=hubertsiuzdak/snac_32khz}
    SNAC_SAMPLE_RATE=${SNAC_SAMPLE_RATE:=32000}
    ;;
  44khz)
    SNAC_MODEL=${SNAC_MODEL:=hubertsiuzdak/snac_44khz}
    SNAC_SAMPLE_RATE=${SNAC_SAMPLE_RATE:=44100}
    ;;
  *)
    echo "Error: Unknown SNAC_VARIANT '${SNAC_VARIANT}'. Use '24khz', '32khz', or '44khz'."
    exit 1
    ;;
esac

if [ $# -lt 2 ]; then
  echo "Usage: $0 audio_filename.{wav,mp3,aac,...} output.{lpcnet,nesc,ecdc,snac}"
  exit 1
fi

input_file=${input_file:=${1}}
output_file=${output_file:=${2}}

AUDIO_FORMAT="${output_file##*.}"

PCM_TEMP="$(mktemp /tmp/compress-audio-XXXXXX.pcm)"
WAV_TEMP="$(mktemp /tmp/compress-audio-XXXXXX.wav)"

cleanup() {
  rm -f "${PCM_TEMP}" "${WAV_TEMP}"
}
trap cleanup EXIT

prepare_pcm_16k() {
  ffmpeg -y -i "${input_file}" -c:a pcm_s16le -f s16le -ac 1 -ar 16000 "${PCM_TEMP}" &> /dev/null
}

prepare_wav() {
  local target_sr=$1
  ffmpeg -y -i "${input_file}" -c:a pcm_s16le -ac 1 -ar "${target_sr}" -sample_fmt s16 "${WAV_TEMP}" &> /dev/null
}

case "${AUDIO_FORMAT}" in
  lpcnet)
    prepare_pcm_16k
    "${LPCNET_ENC}" -encode "${PCM_TEMP}" "${output_file}" &> /dev/null
    ;;

  nesc)
    prepare_pcm_16k
    "${NESC_ENC}" -q -b 1600 -if "${PCM_TEMP}" -of "${output_file}" &> /dev/null
    ;;

  ecdc)
    prepare_wav 24000
    "${PYTHON_BIN}" "${ENCODEC_ENC}" "${WAV_TEMP}" "${output_file}" \
      --codec encodec \
      --bitrate "${ENCODEC_BITRATE}" \
      --threads "${TORCH_THREADS}" &> /dev/null
    ;;

  snac)
    prepare_wav "${SNAC_SAMPLE_RATE}"
    "${PYTHON_BIN}" "${ENCODEC_ENC}" "${WAV_TEMP}" "${output_file}" \
      --codec snac \
      --snac-model "${SNAC_MODEL}" \
      --snac-sample-rate "${SNAC_SAMPLE_RATE}" \
      --threads "${TORCH_THREADS}" &> /dev/null
    ;;

  *)
    echo "Unsupported extension: ${AUDIO_FORMAT}."
    exit 1
    ;;
esac

echo "Final file size: $(stat -c%s "${output_file}")"
