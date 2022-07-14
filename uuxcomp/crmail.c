/* uucomp: crmail
 * Copyright (C) 2021 Rhizomatica
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

#include "xz_decompression.h"

#define MAX_FILENAME 4096
#define S_BUF 128

#define BUF_SIZE 4096

#define ENC_TYPE_NONE 0
#define ENC_TYPE_AUDIO 1
#define ENC_TYPE_IMAGE 2


int main (int argc, char *argv[])
{

    // Variables declaration
    char decode_cmd[MAX_FILENAME];
    char *blob;
    char rmail_cmd[MAX_FILENAME];

    struct stat st;
    off_t decoded_media_file_size;
    size_t file_size;

    char *char_ptr;
    char *char_ptr1;
    char *char_ptr2;
    char *char_ptr3;

    char *boundary;

    FILE *encoded_media;
    FILE *decoded_media;

    char decoded_media_filename[MAX_FILENAME];
    char encoded_media_filename[MAX_FILENAME];


    int encoding_type = ENC_TYPE_NONE;

    // Code starts here
    char tmp_mail[MAX_FILENAME];
    FILE *tmp_mail_fp;
    sprintf(tmp_mail, "/tmp/crmail.%d", getpid ());

    // check if we are dealing with ver 1 gz or ver 2 xz

    // read stdin
    char *message_payload; // dynamic size, read from stdin
    size_t message_size;
    char tmp_buffer[BUF_SIZE];
    size_t buffer_size;

    message_size = fread(tmp_buffer, 1, BUF_SIZE, stdin);
    message_payload = malloc(message_size);
    memcpy(message_payload, tmp_buffer, message_size);
    while ( !feof(stdin) )
    {
        size_t needle = message_size;
        buffer_size = fread(tmp_buffer, 1, BUF_SIZE, stdin);
        message_size += buffer_size;
        message_payload = realloc(message_payload, message_size);
        memcpy(message_payload + needle, tmp_buffer, buffer_size);
    }

#if 0
    FILE *test = fopen("uuxcomp-message.xz", "w");
    fwrite(message_payload, 1, message_size, test);
    fclose(test);
#endif

    file_size = message_size * 20;
    blob = malloc(file_size);


    xz_decompress(blob, &file_size, message_payload, message_size);

    tmp_mail_fp = fopen(tmp_mail, "w");
    fwrite(blob, 1, file_size, tmp_mail_fp);
    fclose(tmp_mail_fp);

    char_ptr = strstr(blob, "Content-Type: image/x-vvc");

    if (char_ptr != NULL)
    {
        printf("IMAGE found.\n");
        encoding_type = ENC_TYPE_IMAGE;
    }

    if (encoding_type == ENC_TYPE_NONE)
    {
        // not image... checking if there is audio
        char_ptr = strstr(blob, "Content-Type: audio/x-lpcnet");
        if (char_ptr != NULL)
        {
            printf("AUDIO found.\n");
            encoding_type = ENC_TYPE_AUDIO;
        }
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

    if (encoding_type == ENC_TYPE_AUDIO)
        sprintf(encoded_media_filename, "/tmp/crmail_encoded.%d.lpcnet", getpid ());

    encoded_media = fopen(encoded_media_filename, "w");

    if (encoded_media == NULL)
    {
        printf("%s could not be opened.\n", encoded_media_filename);
        goto send_mail;
    }

    fwrite(char_ptr3, (blob + file_size) - char_ptr3, 1, encoded_media);

    fclose(encoded_media);


    if (encoding_type == ENC_TYPE_IMAGE)
    {
        sprintf(decoded_media_filename, "/tmp/crmail_decoded.%d.jpg", getpid ());
        sprintf(decode_cmd, "decompress_image.sh %s %s", encoded_media_filename, decoded_media_filename);
    }

    if (encoding_type == ENC_TYPE_AUDIO)
    {
        sprintf(decoded_media_filename, "/tmp/crmail_decoded.%d.aac", getpid ());
        sprintf(decode_cmd, "decompress_audio.sh %s %s", encoded_media_filename, decoded_media_filename);
    }

    system(decode_cmd);

    // now lets start rewriting the email inline
    tmp_mail_fp = fopen(tmp_mail, "w");

    if (tmp_mail_fp == NULL)
    {
        printf("%s could not be opened.\n", tmp_mail);
        return EXIT_FAILURE;
    }


    fwrite(blob, char_ptr - blob, 1, tmp_mail_fp);

    if (encoding_type == ENC_TYPE_IMAGE)
        fprintf(tmp_mail_fp, "Content-Type: image/jpeg\n");
    if (encoding_type == ENC_TYPE_AUDIO)
        fprintf(tmp_mail_fp, "Content-Type: audio/aac\n");

    fwrite(char_ptr1, char_ptr3 - char_ptr1, 1, tmp_mail_fp);

    fprintf(tmp_mail_fp, "Content-Transfer-Encoding: base64\n\n");

    // now we need to convert the decoded media back to base64... and write
    // to the "new" email
    if (stat(decoded_media_filename, &st) != 0)
    {
        printf("%s could not be opened.\n", decoded_media_filename);
        fclose(tmp_mail_fp);
        goto send_mail;
    }
    decoded_media_file_size = st.st_size;

    if (decoded_media_file_size == 0)
    {
        printf("%s has zero size.\n", decoded_media_filename);
        fclose(tmp_mail_fp);
        goto send_mail;
    }

    decoded_media = fopen(decoded_media_filename, "r");

    if (decoded_media == NULL)
    {
        printf("%s could not be opened.\n", decoded_media_filename);
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

    printf("b64 raw data size = %ld\n", decoded_media_file_size);

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
    sprintf(rmail_cmd, "(rmail ");
    for (int i = 1; i < argc; i++)
    {
        strcat (rmail_cmd, argv[i]);
        strcat (rmail_cmd, " ");
    }
    sprintf (rmail_cmd+strlen(rmail_cmd), "< %s)", tmp_mail);

    printf("Running: %s\n", rmail_cmd);

    system(rmail_cmd);

    unlink(tmp_mail);
    free(blob);
    free(message_payload);

    return EXIT_SUCCESS;
}
