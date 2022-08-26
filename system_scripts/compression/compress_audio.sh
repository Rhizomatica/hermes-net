#!/bin/bash
# usage:
# compress_audio.sh audio_filename.{wav,mp3,aac,...} [output.{lpcnet,nesc}]

## env vars:
# LPCNET_ENC: lpcnet enc binary
# NESC_ENC: nesc enc binary
# input_file
# output_file

LPCNET_ENC=${LPCNET_ENC:=/opt/lpcnet/lpcnet_demo}
NESC_ENC=${NESC_ENC:=/opt/nesc/nesc_enc}

if [ $# -lt 2 ]; then
  echo "Usage: $0 audio_filename.{wav,mp3,aac,...} [output.{lpcnet,nesc}]"
  exit 1
fi

input_file=${input_file:=${1}}
output_file=${output_file:=${2}}

AUDIO_FORMAT="${output_file##*.}"

TEMPFILE=/tmp/temp-$$.${AUDIO_FORMAT}

if [ ${AUDIO_FORMAT} = "lpcnet" ]; then
  ffmpeg -y -i "${input_file}"  -c:a pcm_s16le -f s16le -ac 1 -ar 16000 ${TEMPFILE} &> /dev/null
  ${LPCNET_ENC} -encode ${TEMPFILE} "${output_file}" &> /dev/null

elif [ ${AUDIO_FORMAT} = "nesc" ]; then
  ffmpeg -y -i "${input_file}"  -c:a pcm_s16le -f s16le -ac 1 -ar 16000 ${TEMPFILE} &> /dev/null
  ${NESC_ENC} -q -b 1600 -if ${TEMPFILE} -of "${output_file}" &> /dev/null

else
  echo "Unsupported extension: ${AUDIO_FORMAT}."
  exit
fi

rm -f ${TEMPFILE}


echo "Final file size: $(stat -c%s "${output_file}")"
