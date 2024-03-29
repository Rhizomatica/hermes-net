/* uuxcomp: crmail
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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include <b64/cencode.h>
#include <zlib.h>

#include "xz_decompression.h"
#include "gz_compress.h"
#include "daemon.h"

#define DEBUG_MODE 0 // 0, 1 and 2
#define DEBUG_FILENAME "/var/log/uucp/crmail_debug.txt"

#define ADD_CHAT_HEADER 1

#define MAX_FILENAME 4096
#define S_BUF 128

#define BUF_SIZE 4096

#define ENC_TYPE_NONE 0
#define ENC_TYPE_AUDIO_LPCNET 1
#define ENC_TYPE_AUDIO_NESC 2
#define ENC_TYPE_IMAGE 3

#include "utils.h"

int main (int argc, char *argv[])
{

    // Variables declaration
    char decode_cmd[MAX_FILENAME];
    char *blob;
    char rmail_cmd[MAX_FILENAME];

    struct stat st;
    off_t decoded_media_file_size;
    size_t file_size = 0;

    char *char_ptr;
    char *char_ptr1;
    char *char_ptr2;
    char *char_ptr3;

    char *boundary;

    FILE *encoded_media;
    FILE *decoded_media;

    FILE *debug_output;

    char decoded_media_filename[MAX_FILENAME];
    char encoded_media_filename[MAX_FILENAME];


    int encoding_type = ENC_TYPE_NONE;

    // Code starts here
    char tmp_mail[MAX_FILENAME];
    FILE *tmp_mail_fp;
    sprintf(tmp_mail, "/tmp/crmail.%d", getpid ());

    // read stdin
    uint8_t *message_payload; // dynamic size, read from stdin
    size_t message_size;
    char tmp_buffer[BUF_SIZE];
    size_t buffer_size;

    message_size = fread(tmp_buffer, 1, BUF_SIZE, stdin);
    message_payload = malloc(message_size);
    memcpy(message_payload, tmp_buffer, message_size);

    // read the compressed email from stdin
    while ( !feof(stdin) )
    {
        size_t needle = message_size;
        buffer_size = fread(tmp_buffer, 1, BUF_SIZE, stdin);
        message_size += buffer_size;
        message_payload = realloc(message_payload, message_size);
        memcpy(message_payload + needle, tmp_buffer, buffer_size);
    }

    // prepare the rmail command
    sprintf(rmail_cmd, "(rmail ");
    for (int i = 1; i < argc; i++)
    {
        strcat (rmail_cmd, argv[i]);
        strcat (rmail_cmd, " ");
    }
    sprintf (rmail_cmd+strlen(rmail_cmd), "< %s)", tmp_mail);


#if 0
    FILE *test = fopen("uuxcomp-message.xz", "w");
    fwrite(message_payload, 1, message_size, test);
    fclose(test);
#endif

    // daemonize and return the parent...
    if (become_daemon() != 0)
    {
        fprintf(stderr, "Error in daemon()\n");
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

    fprintf(debug_output, "Daemon creation succeed!\n");


    // if XZ
    // xz magic is: FD 37 7A 58  5A
    if (message_payload[0] == 0xfd && message_payload[1] == 0x37 && message_payload[2] == 0x7a && message_payload[3] == 0x58 && message_payload[4] == 0x5a)
    {
        file_size = get_uncompressed_size(message_payload, message_size);

        fprintf(debug_output, "decompressed size = %lu\n", file_size);

        blob = malloc(file_size);

        xz_decompress((uint8_t *) blob, &file_size, message_payload, message_size);
    }
    else if (message_payload[0] == 0x1f && message_payload[1] == 0x8B)
    {
        // the uncompressed size of of the gunzip is expressed in the last 4 bytes of the file...
        file_size = ((size_t)(message_payload[message_size - 1]) << 24 | (size_t)(message_payload[message_size - 2] << 16) | (size_t)(message_payload[message_size - 3] << 8) | (size_t)(message_payload[message_size - 4]));

        blob = malloc(file_size);

        gz_decompress(message_payload, message_size, (uint8_t *) blob, &file_size);

    }
    else
    {
        fprintf(debug_output,"No compressed payload found... just forwarding as is...\n");
        blob = (char *) message_payload;
        file_size = message_size;
    }


#if ADD_CHAT_HEADER == 1
    // skip first rmail line
    // check which is the line terminator
    // add:
    // Chat-Version: 1.0
    char *line_break = uuxcomp_determine_linebreak(blob);
    char new_header[512];
    sprintf(new_header,"Chat-Version: 1.0%s",line_break);
    int new_header_size = strlen(new_header);

    if (!strstr(blob, new_header))
    {

        char *msg_payload = strstr(blob, line_break) + strlen(line_break);
        int first_line_size = file_size - strlen(msg_payload);

        char *edited_message = malloc(file_size + new_header_size);
        memcpy(edited_message, blob, first_line_size);
        memcpy(edited_message + first_line_size, new_header, new_header_size);
        memcpy(edited_message + first_line_size + new_header_size, msg_payload, strlen(msg_payload));
        free(blob);
        blob = edited_message;
        file_size += new_header_size;
    }
#endif

    fprintf(debug_output, "writing decompressed file to: %s\n", tmp_mail);
    tmp_mail_fp = fopen(tmp_mail, "w");
    fwrite(blob, 1, file_size, tmp_mail_fp);
    fclose(tmp_mail_fp);

    char *tmp_ptr;
    // we need to iterate for multipart... improve this in general...
    tmp_ptr = strstr(blob, "Content-Type: image/x-vvc");
    if (tmp_ptr != NULL)
    {
        fprintf(debug_output, "IMAGE found.\n");
        encoding_type = ENC_TYPE_IMAGE;
        char_ptr = tmp_ptr;
    }

    // not image... checking if there is audio
    tmp_ptr = strstr(blob, "Content-Type: audio/x-lpcnet");
    if (tmp_ptr != NULL)
    {
        fprintf(debug_output, "LPC_NET AUDIO found.\n");
        encoding_type = ENC_TYPE_AUDIO_LPCNET;
        char_ptr = tmp_ptr;
    }

    // not image... checking if there is audio
    tmp_ptr = strstr(blob, "Content-Type: audio/x-nesc");
    if (tmp_ptr != NULL)
    {
        fprintf(debug_output, "NESC AUDIO found.\n");
        encoding_type = ENC_TYPE_AUDIO_NESC;
        char_ptr = tmp_ptr;
    }


// if it is a message just gzipped... we go to send email target
    if (encoding_type == ENC_TYPE_NONE)
        goto send_mail;

    char_ptr1 = strstr(char_ptr, "Content-Disposition: attachment;");

    char_ptr2 = strstr(char_ptr1, "filename=");

    char_ptr3 = strstr(char_ptr2, "\n");
    char_ptr3++;

    if (encoding_type == ENC_TYPE_IMAGE)
        sprintf(encoded_media_filename, "/tmp/crmail_encoded.%d.vvc", getpid ());

    if (encoding_type == ENC_TYPE_AUDIO_LPCNET)
        sprintf(encoded_media_filename, "/tmp/crmail_encoded.%d.lpcnet", getpid ());

    if (encoding_type == ENC_TYPE_AUDIO_NESC)
        sprintf(encoded_media_filename, "/tmp/crmail_encoded.%d.nesc", getpid ());


    encoded_media = fopen(encoded_media_filename, "w");

    if (encoded_media == NULL)
    {
        fprintf(debug_output, "%s could not be opened.\n", encoded_media_filename);
        goto send_mail;
    }

    fwrite(char_ptr3, (blob + file_size) - char_ptr3, 1, encoded_media);

    fclose(encoded_media);


    if (encoding_type == ENC_TYPE_IMAGE)
    {
        sprintf(decoded_media_filename, "/tmp/crmail_decoded.%d.jpg", getpid ());
        sprintf(decode_cmd, "decompress_image.sh %s %s", encoded_media_filename, decoded_media_filename);
    }

    if (encoding_type == ENC_TYPE_AUDIO_LPCNET || encoding_type == ENC_TYPE_AUDIO_NESC)
    {
        sprintf(decoded_media_filename, "/tmp/crmail_decoded.%d.aac", getpid ());
        sprintf(decode_cmd, "decompress_audio.sh %s %s", encoded_media_filename, decoded_media_filename);
    }

    system(decode_cmd);

    // now lets start rewriting the email inline
    tmp_mail_fp = fopen(tmp_mail, "w");

    if (tmp_mail_fp == NULL)
    {
        fprintf(debug_output, "%s could not be opened.\n", tmp_mail);
        return EXIT_FAILURE;
    }


    fwrite(blob, char_ptr - blob, 1, tmp_mail_fp);

    if (encoding_type == ENC_TYPE_IMAGE)
        fprintf(tmp_mail_fp, "Content-Type: image/jpeg\n");
    if (encoding_type == ENC_TYPE_AUDIO_LPCNET || encoding_type == ENC_TYPE_AUDIO_NESC)
        fprintf(tmp_mail_fp, "Content-Type: audio/aac\n");

    fwrite(char_ptr1, char_ptr3 - char_ptr1, 1, tmp_mail_fp);

    fprintf(tmp_mail_fp, "Content-Transfer-Encoding: base64\n\n");

    // now we need to convert the decoded media back to base64... and write
    // to the "new" email
    if (stat(decoded_media_filename, &st) != 0)
    {
        fprintf(debug_output, "%s could not be opened.\n", decoded_media_filename);
        fclose(tmp_mail_fp);
        goto send_mail;
    }
    decoded_media_file_size = st.st_size;

    if (decoded_media_file_size == 0)
    {
        fprintf(debug_output, "%s has zero size.\n", decoded_media_filename);
        fclose(tmp_mail_fp);
        goto send_mail;
    }

    decoded_media = fopen(decoded_media_filename, "r");

    if (decoded_media == NULL)
    {
        fprintf(debug_output, "%s could not be opened.\n", decoded_media_filename);
        fclose(tmp_mail_fp);
        goto send_mail;
    }

    // now extract the payload, decode it, convert back to base64, and write to the email...
    char *decoded_media_blob;
    decoded_media_blob = malloc(decoded_media_file_size);
    fread(decoded_media_blob, decoded_media_file_size, 1, decoded_media);
    fclose(decoded_media);

    base64_encodestate b64_state;
    base64_init_encodestate(&b64_state);

    char *b64_blob = malloc(4 * decoded_media_file_size);

    fprintf(debug_output, "b64 raw data size = %ld\n", decoded_media_file_size);

    int rc;
    rc = base64_encode_block(decoded_media_blob, decoded_media_file_size, b64_blob, &b64_state);
    fwrite(b64_blob, rc, 1, tmp_mail_fp);
    rc = base64_encode_blockend(b64_blob, &b64_state);
    fwrite(b64_blob, rc, 1, tmp_mail_fp);

    free(b64_blob);

    fprintf(tmp_mail_fp, "\n");

    // we need to get the boundary... (... it is a strrstr)
    boundary = char_ptr;
    boundary--; boundary--;
    while (*boundary != 10) // 10 is '\n'
        boundary--;
    boundary++;

    // this includes a needed new line.
    fwrite(boundary, char_ptr - boundary, 1, tmp_mail_fp);

    // finished rewriting the email!

    fclose(tmp_mail_fp);

    unlink(decoded_media_filename);
    unlink(encoded_media_filename);

    free(decoded_media_blob);

send_mail:
    fprintf(debug_output, "Running: %s\n", rmail_cmd);

    system(rmail_cmd);

    unlink(tmp_mail);
    if ((uint8_t *)blob == message_payload)
    {
        free(blob);
    }
    else
    {
        free(message_payload);
        free(blob);
    }

    return EXIT_SUCCESS;
}
