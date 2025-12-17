#!/bin/bash
# usage:
# decompress_audio file.{lpcnet,nesc,ecdc,snac} output.{aac,mp3,wav,...}

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

LPCNET_DEC=${LPCNET_DEC:=/opt/lpcnet/lpcnet_demo}
NESC_DEC=${NESC_DEC:=/opt/nesc/nesc_dec}
PYTHON_BIN=${PYTHON_BIN:=python3}
ENCODEC_DEC=${ENCODEC_DEC:="${SCRIPT_DIR}/decode_ecdc.py"}
SNAC_MODEL=${SNAC_MODEL:=hubertsiuzdak/snac_24khz}
SNAC_SAMPLE_RATE=${SNAC_SAMPLE_RATE:=24000}
TORCH_THREADS=${TORCH_THREADS:=1}

if [ $# -lt 2 ]; then
  echo "Usage: $0 audio_filename.{lpcnet,nesc,ecdc,snac} output.{aac,mp3,wav,...}"
  exit 1
fi

input_file=${1}
output_file=${2}

AUDIO_FORMAT="${input_file##*.}"

PCM_TEMP="$(mktemp /tmp/decompress-audio-XXXXXX.pcm)"
WAV_TEMP="$(mktemp /tmp/decompress-audio-XXXXXX.wav)"

cleanup() {
  rm -f "${PCM_TEMP}" "${WAV_TEMP}"
}
trap cleanup EXIT

case "${AUDIO_FORMAT}" in
  lpcnet)
    "${LPCNET_DEC}" -decode "${input_file}" "${PCM_TEMP}" &> /dev/null
    ffmpeg -y -f s16le -ar 16000 -ac 1 -c:a pcm_s16le -i "${PCM_TEMP}" "${output_file}" &> /dev/null
    ;;

  nesc)
    "${NESC_DEC}" -q -if "${input_file}" -of "${PCM_TEMP}" &> /dev/null
    ffmpeg -y -f s16le -ar 16000 -ac 1 -c:a pcm_s16le -i "${PCM_TEMP}" "${output_file}" &> /dev/null
    ;;

  ecdc)
    "${PYTHON_BIN}" "${ENCODEC_DEC}" "${input_file}" "${WAV_TEMP}" \
      --codec encodec \
      --threads "${TORCH_THREADS}" &> /dev/null
    ffmpeg -y -i "${WAV_TEMP}" "${output_file}" &> /dev/null
    ;;

  snac)
    "${PYTHON_BIN}" "${ENCODEC_DEC}" "${input_file}" "${WAV_TEMP}" \
      --codec snac \
      --snac-model "${SNAC_MODEL}" \
      --snac-sample-rate "${SNAC_SAMPLE_RATE}" \
      --threads "${TORCH_THREADS}" &> /dev/null
    ffmpeg -y -i "${WAV_TEMP}" "${output_file}" &> /dev/null
    ;;

  *)
    echo "Unsupported extension: ${AUDIO_FORMAT}."
    exit 1
    ;;
esac

echo "Final file size: $(stat -c%s "${output_file}")"
