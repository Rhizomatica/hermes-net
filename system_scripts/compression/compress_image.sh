#!/bin/bash
# usage:
# compress_image.sh image_filename.{png,gif,...} [output.{jpg,avif,heic,vvc}]

## env vars:
# VVC_ENC: vvc enc binary
# TARGET SIZE: target size in bits
# input_file
# output_file

# initial VVC QP to try without downscaling...
VVC_QP=38

MAX_DIMENSION_SIZE=${MAX_DIMENSION_SIZE:=840}

QUALITY=75 # initial start quality to try for jpeg

VVC_ENC=${VVC_ENC:=/opt/vvc/vvencapp}
VVC_ENC_FF=${VVC_ENC_FF:=/opt/vvc/vvencFFapp}
EVC_ENC=${EVC_ENC:=/root/xeve/build/bin/xeve_app}
AV1_ENC=${AV1_ENC:=/root/aom/build2/aomenc}

# maximum image size setting
MAX_SIZE=${MAX_SIZE:=10000} # 10kB max size limit

# converting to bits...
TARGET_SIZE=${TARGET_SIZE:=$((${MAX_SIZE} * 8))}


# vvc and evc  are the state of the art, no integration to userlad
# avif and heic are already implemented and integrated to userland
# jpg is the legacy format
# IMAGE_FORMAT=${IMAGE_FORMAT:=heic}

if [ $# -lt 2 ]; then
  echo "Usage: $0 image_filename.{png,gif,...} [output.{jpg,avif,heic,vvc}]"
  exit 1
fi

input_file=${input_file:=${1}}
output_file=${output_file:=${2}}

IMAGE_FORMAT="${output_file##*.}"

TEMPFILE=/tmp/temp-$$.${IMAGE_FORMAT}
TEMPFILEYUV=/tmp/temp-$$.yuv
RCFILE=/tmp/temp-$$.rc

# resolution=$(ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=p=0 "${input_file}" | sed 's/,/x/g')
resolution=$(identify-im6 -format "%[w]x%[h]" "${input_file}")
width=$(echo -n ${resolution} | cut -f 1 -d x)
height=$(echo -n ${resolution} | cut -f 2 -d x)

changed_resolution=0

echo "Original File size = $(stat -c%s "${input_file}")"
echo "Original Resolution = ${resolution}"

if [ "${width}" -gt "${MAX_DIMENSION_SIZE}" ] || [ "${height}" -gt "${MAX_DIMENSION_SIZE}" ]; then
  if [ "${width}" -gt "${height}" ]; then
    new_width=${MAX_DIMENSION_SIZE}
    ratio=$(awk "BEGIN {x=${width}/${new_width}; printf \"%.4f\n\",x}")
    new_height=$(awk "BEGIN {x=${height}/${ratio}; printf \"%d\",x}")
  else
    new_height=${MAX_DIMENSION_SIZE}
    ratio=$(awk "BEGIN {x=${height}/${new_height}; printf \"%.4f\n\",x}")
    new_width=$(awk "BEGIN {x=${width}/${ratio}; printf \"%d\",x}")
  fi
  width=${new_width}
  height=${new_height}
  changed_resolution=1
fi

# make resolution multiple of 4
new_width=$(( (${width} / 4) * 4 ))
new_height=$(( (${height} / 4) * 4 ))

if [ "${new_width}" -ne "${width}" ] || [ "${new_height}" -ne "${height}" ]; then
  width=${new_width}
  height=${new_height}
  changed_resolution=1
fi

resolution=${width}x${height}

echo "Final Resolution = ${resolution}"

# ffprobe -v error -select_streams v:0 -show_entries stream=width -of default=nw=1:nk=1
# ffmpeg -i ../../testeCapacidadeMidias/images/FotoFoneMaq_Zap_IncendioBernardoSayao10-07-21_Felipe_Spina_Avino_WWF\ \(1\).jpeg -vf scale=840:840:force_original_aspect_ratio=decrease output.jpg
# ffmpeg -i blue.png -vf scale=out_color_matrix=bt709:flags=full_chroma_int+accurate_rnd,format=yuv420p yuv420p_709.yuv
# -vf scale=840:840:force_original_aspect_ratio=decrease



#pictures look darker... try zscale...
# ffmpeg -i DPX(what_I_want_to_convert_to_DNXHR).dpx -vf lut3d=ArriAlexa_LogCtoRec709_Resolve.cube,zscale=matrix=709,format=yuv444p10le -c:v dnxhd -profile:v dnxhr_444 -an test_zscale.mov
# ffmpeg -i "INPUT.jpg" -vf zscale=matrixin=470bg:matrix=709:rangein=full:range=limited,format=yuv422p10le -c:v dnxhd -profile:v dnxhr_hqx -an test_jpg_zscale.mov


if [ ${changed_resolution} -eq "1" ]; then
  echo "Content will be downscaled"
  convert-im6 -resize "${resolution}!" "${input_file}" -sampling-factor 4:2:0 -depth 8 -colorspace Rec709YCbCr ${TEMPFILEYUV}
  # ffmpeg  -y -i "${input_file}" -color_range 2 -vf scale=in_range=full:out_range=full:width=${width}:height=${height}:out_color_matrix=bt709,format=yuv420p10le -f rawvideo ${TEMPFILEYUV}
  # ffmpeg -y -i "${input_file}"  -f rawvideo -vf scale=width=${width}:height=${height}:out_color_matrix=bt709:flags=full_chroma_int+accurate_rnd,format=yuv420p10le ${TEMPFILEYUV}
else
  convert-im6 "${input_file}" -sampling-factor 4:2:0 -depth 8 -colorspace Rec709YCbCr ${TEMPFILEYUV}
  # ffmpeg  -y -i "${input_file}" -color_range 2 -vf scale=in_range=full:out_range=full:out_color_matrix=bt709,format=yuv420p10le -f rawvideo ${TEMPFILEYUV}
  # ffmpeg -y -i "${input_file}"  -f rawvideo -vf scale=out_color_matrix=bt709:flags=full_chroma_int+accurate_rnd,format=yuv420p10le ${TEMPFILEYUV}
fi

if [ ${IMAGE_FORMAT} = "evc" ]; then

    echo ${EVC_ENC} -w ${width} -h ${height} -d 10 -z 1 -m 2 --profile main --preset medium --bitrate $(( ${TARGET_SIZE} / 1000 ))  -i ${TEMPFILEYUV} -o  ${TEMPFILE}
    ${EVC_ENC} -w ${width} -h ${height} -d 10  -z 1 -m 2 --profile main --preset medium --bitrate $(( ${TARGET_SIZE} / 1000 ))  -i ${TEMPFILEYUV} -o  ${TEMPFILE}

elif [ ${IMAGE_FORMAT} = "vvc" ]; then


    ${VVC_ENC} -i ${TEMPFILEYUV} --profile main_10_still_picture --qpa 1 -f 1 -c yuv420 --internal-bitdepth 8 -t 2 -r 1 --qp ${VVC_QP} -s ${resolution} --preset medium -o  ${TEMPFILE}

    # if too big, we use rate-control...
    if [ "$(stat -c%s "${TEMPFILE}")" -gt "${MAX_SIZE}" ]; then
    
	QP=$( ${VVC_ENC} -i ${TEMPFILEYUV} --profile main_10_still_picture --qpa 1 -f 1 -c yuv420 --internal-bitdepth 8 -t 2 -r 1 -b ${TARGET_SIZE} -s ${resolution} --preset medium -o  ${TEMPFILE} | grep POC | cut -d Q -f 2 | cut -d " " -f 2 | tr -dc '0-9' )
	echo "QP = ${QP}"
	
	if [ "${QP}" -gt "40" ]; then
	    new_width=$(awk "BEGIN {x=${width}*0.7; printf \"%d\n\",x}")
	    new_height=$(awk "BEGIN {x=${height}*0.7; printf \"%d\n\",x}")

	    # make resolution multiple of 4
	    new_width=$(( (${new_width} / 4) * 4 ))
	    new_height=$(( (${new_height} / 4) * 4 ))

	    resolution=${new_width}x${new_height}

	    echo "Downscaling even more to: ${resolution}"
	    rm -f ${TEMPFILEYUV}
	    convert-im6 -resize "${resolution}!" "${input_file}" -sampling-factor 4:2:0 -depth 8 -colorspace Rec709YCbCr ${TEMPFILEYUV}
	    QP=$( ${VVC_ENC} -i ${TEMPFILEYUV} --profile main_10_still_picture --qpa 1 -f 1 --rcstatsfile ${RCFILE} -c yuv420 --internal-bitdepth 8 -t 2 -r 1 -b ${TARGET_SIZE} -s ${resolution} --preset medium -o  ${TEMPFILE} | grep POC | cut -d Q -f 2 | cut -d " " -f 2 | tr -dc '0-9' )
	    echo "QP 2 = ${QP}"

	    if [ "$(stat -c%s "${TEMPFILE}")" -lt "${MAX_SIZE}" ]; then
		new_size=$(( ${TARGET_SIZE} * ${TARGET_SIZE} / (8 * $(stat -c%s "${TEMPFILE}")) ))
		echo "new_size = ${new_size}"
		QP=$( ${VVC_ENC} -i ${TEMPFILEYUV} --profile main_10_still_picture --qpa 1 -f 1 --pass 2 --rcstatsfile ${RCFILE} -c yuv420 --internal-bitdepth 8 -t 2 -r 1 -b ${new_size} -s ${resolution} --preset medium -o  ${TEMPFILE} | grep POC | cut -d Q -f 2 | cut -d " " -f 2 | tr -dc '0-9' )
		echo "QP 3 = ${QP}"
	    fi
	    rm -f ${RCFILE}
	fi
    fi
#    if [ "$(stat -c%s "${TEMPFILE}")" -gt "${MAX_SIZE}" ]; then
#	${VVC_ENC} -i ${TEMPFILEYUV} --profile main_10_still_picture --qpa 1 -f 1 --rcstatsfile ${RCFILE} -c yuv420 --internal-bitdepth 8 -t 2 -r 1 -b ${TARGET_SIZE} -s ${resolution} --preset medium -o  ${TEMPFILE}
#
#	if [ "$(stat -c%s "${TEMPFILE}")" -lt "${MAX_SIZE}" ]; then
#	    new_size=$(( ${TARGET_SIZE} * ${TARGET_SIZE} / (8 * $(stat -c%s "${TEMPFILE}")) ))
#	    echo "new_size = ${new_size}"
#	    ${VVC_ENC} -i ${TEMPFILEYUV} --profile main_10_still_picture --qpa 1 -f 1 --pass 2 --rcstatsfile ${RCFILE} -c yuv420 --internal-bitdepth 8 -t 2 -r 1 -b ${new_size} -s ${resolution} --preset medium -o  ${TEMPFILE}
#	fi
#	
#	rm -f ${RCFILE}
#    fi

## old QP-based rate control
#    while [ "$(stat -c%s "${TEMPFILE}")" -gt "${MAX_SIZE}" ] && [ "$VVC_QP" -lt "61" ]; do
#      VVC_QP=$((VVC_QP+3))
#      ${VVC_ENC} -i ${TEMPFILEYUV} --profile main_10_still_picture --qpa 1 -c yuv420_10 -t 2 -r 1 --qp ${VVC_QP} -s ${resolution} --preset medium -o  ${TEMPFILE}
#    done;

  rm -f ${TEMPFILEYUV}

elif [ ${IMAGE_FORMAT} = "avif" ]; then
  echo "AVIF support not implemented."
  exit
# TODO
##  ${AV1_ENC} --target-bitrate=${TARGET_SIZE} --end-usage=cbr --bit-depth=8 ...
elif [ ${IMAGE_FORMAT} = "jpg" ]; then

  cp -f "${input_file}" ${TEMPFILE}

  while [ "$(stat -c%s "${TEMPFILE}")" -gt "$MAX_SIZE" ] && [ "${QUALITY}" -gt "5" ]; do
    convert -resize "840x840>" "${input_file}" pnm:- | /opt/mozjpeg/bin/cjpeg -quality ${QUALITY} > ${TEMPFILE}
    QUALITY=$((QUALITY-10))
  done;

elif [ ${IMAGE_FORMAT} = "heic" ]; then

  echo "HEIF support not implemented."
  exit

# TODO
##  while [ "$(stat -c%s "${TEMPFILE}")" -gt "$MAX_SIZE" ] && [ "$QUALITY" -gt "5" ]; do
##    convert -resize "840x840>"  "${input_file}" -quality ${QUALITY} ${TEMPFILE}
##    QUALITY=$((QUALITY-10))
##  done;

else
  echo "Unsupported extension: ${IMAGE_FORMAT}."
  exit
fi

# in place
#if [ $# -eq 1 ]; then
#  mv ${TEMPFILE} "${input_file}"
#fi

rm -f ${TEMPFILEYUV}

echo "Final file size: $(stat -c%s "${TEMPFILE}")"


# with output file specified
if [ $# -eq 2 ]; then
  mv ${TEMPFILE} "${output_file}"
fi
