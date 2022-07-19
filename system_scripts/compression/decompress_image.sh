#!/bin/bash
# usage:
# decompress_image.sh file.vvc file.jpg

if [ $# -lt 2 ]; then
  echo "Usage: $0 image_filename.{vvc,evc,avif,heic} output.jpg"
  exit 1
fi

VVC_DEC=${VVC_DEC:=/opt/vvc/vvdecapp}
EVC_DEC=${EVC_DEC:=/root/xevd/build/bin/xevd_app}
CJPEG_ENC=${CJPEG:=/opt/mozjpeg/bin/cjpeg}

input_file=${1}
output_file=${2}

IMAGE_FORMAT="${input_file##*.}"

OUTPUT_FORMAT="${output_file##*.}"

TEMPFILEYUV=/tmp/temp-$$.yuv
# TEMPFILEYUV2=/tmp/temp-$$-2.yuv

if [ ${IMAGE_FORMAT} = "vvc" ]; then


    resolution=$(${VVC_DEC} -t 2 -b "${input_file}" -o ${TEMPFILEYUV} | grep SizeInfo | cut -d " " -f 4)
    #    echo ${resolution}

    ## for 10 bit output...
    # ffmpeg -pix_fmt yuv420p10le -s ${resolution} -y -i ${TEMPFILEYUV} -pix_fmt yuv420p ${TEMPFILEYUV2}
    ## for 10 bit output...
    # convert-im6 -size ${resolution} -sampling-factor 4:2:0 -depth 8 -colorspace Rec709YCbCr ${TEMPFILEYUV2} pnm:- | ${CJPEG_ENC} -quality 95 -outfile "${output_file}"

    if [ ${OUTPUT_FORMAT} = "jpg" ] || [ ${OUTPUT_FORMAT} = "JPG" ] || [ ${OUTPUT_FORMAT} = "jpeg" ] || [ ${OUTPUT_FORMAT} = "JPEG" ] || [ ${OUTPUT_FORMAT} = "Jpg" ] || [ ${OUTPUT_FORMAT} = "Jpeg" ]; then
      convert-im6 -size ${resolution} -sampling-factor 4:2:0 -depth 8 -colorspace Rec709YCbCr ${TEMPFILEYUV} pnm:- | ${CJPEG_ENC} -quality 95 -outfile "${output_file}"
    else
      convert-im6 -size ${resolution} -sampling-factor 4:2:0 -depth 8 -colorspace Rec709YCbCr ${TEMPFILEYUV} "${output_file}"
    fi

    rm -f ${TEMPFILEYUV}

elif [ ${IMAGE_FORMAT} = "evc" ]; then

  resolution=$(${EVC_DEC} -i "${input_file}" -o ${TEMPFILEYUV} | grep Resolution| cut -f 2 -d = |  tr -d '[:space:]')

  if [ ${OUTPUT_FORMAT} = "jpg" ] || [ ${OUTPUT_FORMAT} = "JPG" ] || [ ${OUTPUT_FORMAT} = "jpeg" ] || [ ${OUTPUT_FORMAT} = "JPEG" ] || [ ${OUTPUT_FORMAT} = "Jpg" ] || [ ${OUTPUT_FORMAT} = "Jpeg" ]; then
    convert-im6 -size ${resolution} -sampling-factor 4:2:0 -depth 8 -colorspace Rec709YCbCr ${TEMPFILEYUV} pnm:- | ${CJPEG_ENC} -quality 95 -outfile "${output_file}"
  else
    convert-im6 -size ${resolution} -sampling-factor 4:2:0 -depth 8 -colorspace Rec709YCbCr ${TEMPFILEYUV} "${output_file}"
  fi

  rm -f ${TEMPFILEYUV}
fi
