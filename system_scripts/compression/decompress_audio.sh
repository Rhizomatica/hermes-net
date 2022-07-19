#!/bin/bash
# usage:
# decompress_audio file.{lpcnet,nesc} file.aac

LPCNET_DEC=${LPCNET_DEC:=/opt/lpcnet/lpcnet_demo}
NESC_DEC=${NESC_DEC:=/opt/nesc/nesc_dec}


if [ $# -lt 2 ]; then
  echo "Usage: $0 audio_filename.{lpcnet,nesc} output.aac"
  exit 1
fi

input_file=${1}
output_file=${2}

AUDIO_FORMAT="${input_file##*.}"

TEMPFILE=/tmp/temp-$$.pcm

if [ ${AUDIO_FORMAT} = "lpcnet" ]; then
  ${LPCNET_DEC} -decode "${input_file}" ${TEMPFILE}
  ffmpeg -y -f s16le -ar 16000 -ac 1 -c:a pcm_s16le -i ${TEMPFILE} "${output_file}"

elif [ ${AUDIO_FORMAT} = "nesc" ]; then
  ${NESC_DEC} -q -if "${input_file}" -of ${TEMPFILE}
  ffmpeg -y -f s16le -ar 16000 -ac 1 -c:a pcm_s16le -i ${TEMPFILE} "${output_file}"

else
  echo "Unsupported extension: ${AUDIO_FORMAT}."
  exit
fi

rm -f ${TEMPFILE}

echo "Final file size: $(stat -c%s "${output_file}")"
