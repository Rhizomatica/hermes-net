#!/bin/sh

CODEC=vvc

SOURCE_DIR=/home/hermes/content/images
ENCODED_DIR=/home/hermes/content/encoded-vvc
RECONSTRUCTED_DIR=/home/hermes/content/reconstructed-vvc

COMPRESS_IMAGE=/home/hermes/rhizo-uuardop/scripts/compress_image.sh
DECOMPRESS_IMAGE=/home/hermes/rhizo-uuardop/scripts/decompress_image.sh

cd ${SOURCE_DIR}

mkdir -p ${ENCODED_DIR}/
mkdir -p ${RECONSTRUCTED_DIR}/

rm -f ${ENCODED_DIR}/*
rm -f ${RECONSTRUCTED_DIR}/*

# compress data
for i in *; do

    no_extension=${i%.*}

    input_file=\"${i}\"
    output_file=\"${ENCODED_DIR}/${no_extension}.${CODEC}\"
    echo "==== INPUT: ${input_file}"
    echo "==== OUTPUT: ${output_file}"
    eval ${COMPRESS_IMAGE} \"${i}\" \"${ENCODED_DIR}/${no_extension}.${CODEC}\"

    echo "==== DONE"
done

cd ${ENCODED_DIR}

# decompress data
for i in *; do

    no_extension=${i%.*}

#    input_file=\"${i}\"
#    output_file=\"${ENCODED_DIR}/${no_extension}.${CODEC}\"
#    echo compress ${COMPRESS_IMAGE} ${input_file} ${output_file}
    eval ${DECOMPRESS_IMAGE} \"${i}\" \"${RECONSTRUCTED_DIR}/${no_extension}.jpg\"

done
