/* uuxcomp: uuxcomp
 * Copyright (C) 2022 Rhizomatica
 * Author: Rafael Diniz <rafael@riseup.net>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 */

#define VERSION "0.1"

#define DEBUG_MODE 2 // 0, 1 and 2
#define DEBUG_FILENAME "/var/log/uucp/uuxcomp_debug.txt"

// use Fraunhofer Neural End-2-End Speech Coder instead of LPCNet
#define USE_NESC 1

#define MAX_FILENAME 4096
#define S_BUF 128

#define BUF_SIZE 4096

#define ENC_TYPE_NONE 0
#define ENC_TYPE_AUDIO 1
#define ENC_TYPE_IMAGE 2

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include <b64/cdecode.h>

#include "xz_compression.h"

int main (int argc, char *argv[])
{
    char *message_payload; // dynamic size, read from stdin
    size_t message_size;
    size_t buffer_size;
    char tmp_buffer[BUF_SIZE];

    char *output_message;

    FILE *uux_fp;

    FILE *tmp_media;
    char tmp_media_filename[MAX_FILENAME];
    char tmp_encoded_media_filename[MAX_FILENAME];

    struct stat st;
    off_t file_size;

    char *char_ptr;
    char *char_ptr2;
    char *char_ptr3;
    char *char_ptr4;
    char *char_ptr5;

    char *original_filename;

    int encoding_type = ENC_TYPE_NONE;

    char *blob = NULL;

#if DEBUG_MODE > 0
    FILE *debug_output;
#endif



    if (argc < 2)
    {
        fprintf(stderr, "-- uuxcomp version %s by rhizomatica --\n\n", VERSION);
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "uuxcomp [uux parameters]\n");
        // fprintf(stderr, "Eg.: uuxcomp C.NT4DkChAAoLT C.NT8Rg3xAAoNX ...\n");
        return EXIT_FAILURE;
    }

#if DEBUG_MODE > 0
    debug_output = fopen(DEBUG_FILENAME,"a");
    if (debug_output == NULL)
    {
        fprintf(stderr, "Failed to open debug output\n");
        exit(EXIT_FAILURE);
    }
#else
    debug_output = stderr;
#endif

    // read stdin
    message_size = fread(tmp_buffer, 1, BUF_SIZE, stdin);
    fprintf(debug_output, "message_size = %lu\n", message_size);
    fflush(debug_output);
    message_payload = malloc(message_size);
    memcpy(message_payload, tmp_buffer, message_size);
    while ( !feof(stdin) )
    {
        size_t needle = message_size;
        buffer_size = fread(tmp_buffer, 1, BUF_SIZE, stdin);
        message_size += buffer_size;
        message_payload = realloc(message_payload, message_size);
        memcpy(message_payload + needle, tmp_buffer, buffer_size);
        fprintf(debug_output, "message_size = %lu\n", message_size);
        fflush(debug_output);
    }

    output_message = message_payload;

    // daemonize and return the parent...
    if (daemon(0, 0) != 0)
    {
        fprintf(debug_output, "Error in daemon()\n");
    }
    fprintf(debug_output, "Daemon creation succeed!\n");

    // here we go... parsing the stuff...
    char_ptr = strstr(message_payload, "Content-Type: multipart/mixed;");
    if (char_ptr == NULL)
    {
        fprintf(debug_output,"Not a multi-part message.\n");
        goto compress;
    }

    encoding_type = ENC_TYPE_NONE;

    char_ptr2 = strstr(char_ptr, "Content-Type: image/jpeg");
    if (char_ptr2 != NULL)
    {
        fprintf(debug_output, "JPEG image found.\n");
        encoding_type = ENC_TYPE_IMAGE;
    }

    if (encoding_type == ENC_TYPE_NONE)
    {
        char_ptr2 = strstr(char_ptr, "Content-Type: audio/aac");
        if (char_ptr2 != NULL)
        {
            fprintf(debug_output, "AAC audio found.\n");
            encoding_type = ENC_TYPE_AUDIO;
        }
    }

    if (encoding_type == ENC_TYPE_NONE)
        goto compress;

    original_filename = strstr(char_ptr2, "Content-Disposition:");
    if (original_filename == NULL)
    {
        encoding_type = ENC_TYPE_NONE;
        goto compress;
    }

    char_ptr3 = strstr(char_ptr2, "Content-Transfer-Encoding: base64");
    if (char_ptr3 == NULL)
    {
        encoding_type = ENC_TYPE_NONE;
        goto compress;
    }

    char_ptr4 = strstr(char_ptr3, "\n\n");

    if (char_ptr4 == NULL)
    {
        encoding_type = ENC_TYPE_NONE;
        goto compress;
    }

    char_ptr4++; char_ptr4++;

    char_ptr5 = strstr(char_ptr4, "\n\n");

    if (char_ptr5 == NULL)
    {
        encoding_type = ENC_TYPE_NONE;
        goto compress;
    }

    // *****************************
    // convert from base64 to binary

    base64_decodestate b64_state;
    base64_init_decodestate(&b64_state);

    blob = malloc(char_ptr5 - char_ptr4); // the decoded base64 payload will always be smaller...

    int media_size = base64_decode_block(char_ptr4, char_ptr5 - char_ptr4, blob, &b64_state);

    if (encoding_type == ENC_TYPE_IMAGE)
    {
        sprintf(tmp_media_filename, "/tmp/uucomp.%d.jpg", getpid ());
        sprintf(tmp_encoded_media_filename, "/tmp/uucomp.%d.vvc", getpid ());
    }

    if (encoding_type == ENC_TYPE_AUDIO)
    {

        sprintf(tmp_media_filename, "/tmp/uucomp.%d.aac", getpid ());
#if USE_NESC == 0
        sprintf(tmp_encoded_media_filename, "/tmp/uucomp.%d.lpcnet", getpid ());
#else
        sprintf(tmp_encoded_media_filename, "/tmp/uucomp.%d.nesc", getpid ());
#endif
    }

    tmp_media = fopen(tmp_media_filename, "w");

    if (tmp_media == NULL)
    {
        fprintf(debug_output, "Failure to open %s\n", tmp_media_filename);
        encoding_type = ENC_TYPE_NONE;
        free(blob);
        goto compress;
    }

    fwrite(blob, 1, media_size , tmp_media);
    fclose(tmp_media);
    free(blob);

    // ********************
    // compress the media
    // TODO: check if the file size is not smaller than a target size... if already smaller - just compress the message??? Or not?
    char enc_cmd[MAX_FILENAME];
    if (encoding_type == ENC_TYPE_IMAGE)
    {
        sprintf(enc_cmd, "compress_image.sh %s %s", tmp_media_filename, tmp_encoded_media_filename);
        system(enc_cmd); // or popen ??
    }

    if (encoding_type == ENC_TYPE_AUDIO)
    {
        sprintf(enc_cmd, "compress_audio.sh %s %s", tmp_media_filename, tmp_encoded_media_filename);
        system(enc_cmd); // or popen ??
    }

    unlink(tmp_media_filename);

    if (stat(tmp_encoded_media_filename, &st) != 0)
    {
        fprintf(debug_output, "%s could not be opened.\n", tmp_encoded_media_filename);
        goto compress;
    }
    file_size = st.st_size;

    fprintf(debug_output, "%s size is: %ld\n", tmp_encoded_media_filename, file_size);

    if (file_size == 0)
        goto compress;

    // we alloc memory for the message with compressed media with 50%+ size of the original message
    output_message = malloc(message_size * 1.5);


    // write to temp buffer... then the uux stdin

    // *************************************************
    // write new D. file... - In place...
    size_t needle = char_ptr2 - message_payload;
    memcpy(output_message, message_payload, needle);

    if (encoding_type == ENC_TYPE_IMAGE)
    {
        char *prt = "Content-Type: image/x-vvc\n";
        size_t prt_s = strlen(prt);
        memcpy(output_message + needle, prt, prt_s);
        needle += prt_s;
    }

    if (encoding_type == ENC_TYPE_AUDIO)
    {
#if USE_NESC == 0
        char *prt = "Content-Type: audio/x-lpcnet\n";
#else
        char *prt = "Content-Type: audio/x-nesc\n";
#endif
        size_t prt_s = strlen(prt);
        memcpy(output_message + needle, prt, prt_s);
        needle += prt_s;
    }

    blob = malloc(file_size);
    tmp_media = fopen(tmp_encoded_media_filename, "r");
    fread(blob, file_size, 1, tmp_media);
    fclose(tmp_media);
    unlink(tmp_encoded_media_filename);

    memcpy(output_message + needle, original_filename, char_ptr3 - original_filename);
    needle += char_ptr3 - original_filename;

    memcpy(output_message + needle, blob, file_size);
    needle += file_size;

    free(blob);

    message_size = needle;

    // search for: "Content-Type: image/jpeg"
    // search for: "Content-Type: audio/aac"
    // Open the payload D. file
    // check if email contains image or audio
    // if yes, compress the media and then the whole email with gzip (and add an
    // identification in the payload indicating compressed media attached)
    // othewise just compress with gzip

 compress:
    fprintf(debug_output, "Compressing now.\n");

#if DEBUG_MODE > 1
    // parse command lines
    for (int i = 1; i < argc; i++)
    {
        fprintf(debug_output, "arg[%d]:%s\n", i, argv[i]);
    }

    fwrite(output_message, 1, message_size, debug_output);
#endif

    char *compressed_message = malloc(message_size * 1.5);
    size_t compressed_size = message_size * 1.5;
    xz_compress(compressed_message, &compressed_size, output_message, message_size);

    char uux_cmd[BUF_SIZE];
    sprintf(uux_cmd, "uux");
    for (int i = 1; i < argc; i++)
    {
        // do we need to escape "!" and other cmd line stuff, like space?
        strcat(uux_cmd, " ");
        strcat(uux_cmd, argv[i]);
    }

#if 0
    FILE *test = fopen("/home/rafael2k/files/rhizomatica/hermes/hermes-net/uuxcomp/test.xz", "w");
    fwrite(compressed_message, 1, compressed_size, test);
    fclose(test);
    exit(1);
#endif

    uux_fp = popen(uux_cmd, "w");
    // write the compressed message
    fwrite(compressed_message, 1, compressed_size, uux_fp);
    pclose(uux_fp);

    if (output_message != message_payload)
        free(output_message);
    free(message_payload);
    free(compressed_message);

    return EXIT_SUCCESS;
}
