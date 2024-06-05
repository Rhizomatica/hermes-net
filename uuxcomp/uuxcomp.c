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

#define DEBUG_MODE 0 // 0, 1 and 2
#define DEBUG_FILENAME "/var/log/uucp/uuxcomp_debug.txt"

// for old version 1 of our stack,
#define USE_GZ 0
// for version 2, use XZ compression
#define USE_XZ 1

// use Fraunhofer Neural End-2-End Speech Coder instead of LPCNet
#define USE_NESC 0

#define MAIL_SIZE_SCRIPT "mail_size_enforcement.sh"


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

#include <cmime.h>

#include "xz_compression.h"
#include "gz_compress.h"
#include "utils.h"
#include "daemon.h"

// support for hermes messaging subsystem
#define HERMES_SMS "sms@hermes.radio"

#if ((USE_GZ == 1) && (USE_XZ == 1)) || ((USE_GZ == 0) && (USE_XZ == 0))
#error Wrong compression configuration
#endif

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

    bool is_deltachat = false;

    FILE *debug_output;


    if (argc < 2)
    {
        fprintf(stderr, "-- uuxcomp version %s by rhizomatica --\n\n", VERSION);
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "uuxcomp [uux parameters]\n");
        return EXIT_FAILURE;
    }

    // read stdin
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

    /*  CHOP SOME HEADERS OFF */
    // we skip the first like which seems like a UUCP stuff
    char *line_break = uuxcomp_determine_linebreak(message_payload);
    char first_line[512];
    char *processed_msg_payload = strstr(message_payload, line_break) + strlen(line_break);
    int first_line_size = strlen(message_payload) - strlen(processed_msg_payload);
    strncpy(first_line, message_payload, first_line_size);
    first_line[first_line_size] = 0;

    CMimeMessage_T *message = cmime_message_new();
    int ret_val = cmime_message_from_string(&message, processed_msg_payload, 1);
    char *msg_header = cmime_message_to_string(message);
    int old_header_size = strlen(msg_header);
    free(msg_header);
    if (ret_val != 0)
        goto compress;
    CMimeListElem_T *elem = NULL, *elem_to_remove = NULL;
    CMimeHeader_T *header = NULL;
    CMimeHeader_T *header_deleted = NULL;
    char *header_name = NULL;
    int received_count = 0;

    // Here we identify if it is SMS/Message
    bool is_sms = false;
    CMimeListElem_T *list_recv = cmime_list_head(message->recipients);
    while(list_recv != NULL)
    {
        CMimeAddress_T *addr = list_recv->data;
        printf("data: %s\n", addr->email);
        for (size_t i = 0; i < strlen(addr->email); i++)
        {
            if (strstr(addr->email, HERMES_SMS))
               is_sms = true;
        }
        list_recv = list_recv->next;
    }
    // And we send the SMS/Message
    if (is_sms)
    {
        char *body = message_payload+first_line_size + old_header_size;
        char *tmp_sender = cmime_message_get_sender_string(message);
        char sender[BUF_SIZE];

        // clean up sender
        size_t tmp_pos = 0; size_t pos = 0;
        while (tmp_sender[tmp_pos] != 0)
        {
            if (tmp_sender[tmp_pos] != ' ' && tmp_sender[tmp_pos] != '<' && tmp_sender[tmp_pos] != '>')
            {
                sender[pos] = tmp_sender[tmp_pos];
                pos++;
            }
            tmp_pos++;
        }
        sender[pos] = '\n';
        sender[++pos] = 0;
        free(tmp_sender);

        // sent over uux
        char cmd_message[MAX_FILENAME];

        sprintf(cmd_message, "uux -r - gw\\!dec_message");
        FILE *msg_fp = popen(cmd_message, "w");
        // write an email as first thing in a message
        fwrite("From: ", 1, 6, msg_fp);
        fwrite(sender, 1, strlen(sender), msg_fp);
        fwrite(body, 1, strlen(body), msg_fp);
        pclose(msg_fp);

        // we could opt not to return... in the case, comment both lines below:
        free(message_payload);
        return EXIT_SUCCESS;

    }
    // END SMS

    // Delete some fat headers
    elem = cmime_list_head(message->headers);
    while(elem != NULL)
    {
        header = (CMimeHeader_T *) cmime_list_data(elem);
        header_name = cmime_header_get_name(header);

        elem_to_remove = elem;
        elem = elem->next;

        if ( !strcmp(header_name, "Chat-Version") )
            is_deltachat = true;

        if ( (!strcmp(header_name, "DKIM-Signature")) ||
             (!strcmp(header_name, "Authentication-Results")) ||
             (!strcmp(header_name, "X-Virus-Scanned")) ||
             (!strcmp(header_name, "Autocrypt")) ||
             (!strcmp(header_name, "X-Google-DKIM-Signature")) ||
             (!strcmp(header_name, "X-Gm-Message-State")) ||
             (!strcmp(header_name, "X-Google-Smtp-Source")) ||
             (!strcmp(header_name, "X-Received")) )
        {
            cmime_list_remove(message->headers, elem_to_remove, (void *)&header_deleted);
            cmime_header_free(header_deleted);
        }

        if (!strcmp(header_name, "Received"))
        {
            received_count++;
        }
    }
    // then we remove all Received header apart of the last (which is the first to be added)
    if (received_count > 1)
    {
        int received_aux = 0;
        elem = cmime_list_head(message->headers);
        while(elem != NULL)
        {
            header = (CMimeHeader_T *) cmime_list_data(elem);
            header_name = cmime_header_get_name(header);
            elem_to_remove = elem;
            elem = elem->next;
            if (!strcmp(header_name, "Received"))
            {
                received_aux++;
                if (received_aux < received_count)
                {
                    cmime_list_remove(message->headers, elem_to_remove, (void *)&header_deleted);
                    cmime_header_free(header_deleted);
                }
            }
        }
    }
    // printing the headers for debug purpose...
    char *msg_header_stripped = cmime_message_to_string(message);
    int new_header_size = strlen(msg_header_stripped);

    size_t new_message_size = message_size - old_header_size + new_header_size;
    char *tmp_msg_payload = malloc(new_message_size);
    strcpy(tmp_msg_payload, first_line);
    strcat(tmp_msg_payload, msg_header_stripped);
    strcat(tmp_msg_payload, message_payload + first_line_size + old_header_size);

    free(msg_header_stripped);
    free(message_payload);

    message_payload = tmp_msg_payload;
    message_size = new_message_size;
    output_message = message_payload;

    // printf("%s",message_payload);
    /* END CHOP SOME HEADERS OFF */

    // our parser only works for DC at the moment, skip this is not a DC message
    if (is_deltachat == false)
        goto compress;

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

    original_filename = strstr(char_ptr, "Content-Disposition:");
    if (original_filename == NULL)
    {
        encoding_type = ENC_TYPE_NONE;
        goto compress;
    }

    char_ptr3 = strstr(char_ptr, "Content-Transfer-Encoding: base64");
    if (char_ptr3 == NULL)
    {
        encoding_type = ENC_TYPE_NONE;
        goto compress;
    }

    // cope with headers re-ordering...
    if ((original_filename > char_ptr2) && (original_filename > char_ptr3))
        char_ptr3 = original_filename;
    if ((char_ptr2 > original_filename) && (char_ptr2 > char_ptr3))
        char_ptr3 = original_filename;

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

    char *compressed_message = malloc(message_size * 1.5);
    size_t compressed_size = message_size * 1.5;
#if USE_XZ == 1
    xz_compress((uint8_t *) compressed_message, &compressed_size, (uint8_t *) output_message, message_size);
#endif

#if USE_GZ == 1
    gz_compress((uint8_t *) output_message, message_size, (uint8_t *) compressed_message, &compressed_size);
#endif

    printf("compressed size =  %lu\n", compressed_size);

    char uux_cmd[BUF_SIZE];
    sprintf(uux_cmd, "uux");
    for (int i = 1; i < argc; i++)
    {
        // do we need to escape "!" and "(" and ")" and other cmd line stuff, like space?
        // this code just espapes the first occurence
        strcat(uux_cmd, " ");
        char *has_bang = strchr(argv[i], '!');
        char *has_open_par = strchr(argv[i], '(');
        char *has_close_par = strchr(argv[i], ')');

        if (!has_bang && !has_open_par && !has_close_par)
            strcat(uux_cmd, argv[i]);
        else
        {
            char temp_buf[BUF_SIZE];

            if (has_bang)
            {
                size_t first_part_size = has_bang - argv[i];
                strncpy(temp_buf, argv[i], first_part_size);
                strncpy(temp_buf + first_part_size + 1, has_bang ,S_BUF - 1);
                temp_buf[first_part_size] = '\\';
            }

            if (has_open_par && has_close_par)
            {
                temp_buf[0] = '"';
                size_t text_size = strlen(argv[i]);
                strncpy(temp_buf + 1, argv[i], text_size);
                temp_buf[text_size + 1] = '"';
                temp_buf[text_size + 2] = 0;
            }

            strcat(uux_cmd, temp_buf);

        }

    }




#if DEBUG_MODE > 1
    // parse command lines
    for (int i = 1; i < argc; i++)
    {
        fprintf(debug_output, "input arg[%d]:%s\n", i, argv[i]);
    }
    fprintf(debug_output, "\n\n---cmd:\n");
    fprintf(debug_output, uux_cmd);

    // message before compression:
    // fwrite(output_message, 1, message_size, debug_output);
#endif


#if DEBUG_MODE > 1

#if USE_GZ == 1
    FILE *test = fopen("/var/log/uucp/uuxcomp-message.gz", "w");
#endif
#if USE_XZ == 1
    FILE *test = fopen("/var/log/uucp/uuxcomp-message.xz", "w");
#endif
    fwrite(compressed_message, 1, compressed_size, test);
    fclose(test);

#endif

    uux_fp = popen(uux_cmd, "w");
    // write the compressed message
    fwrite(compressed_message, 1, compressed_size, uux_fp);
    pclose(uux_fp);

    if (output_message != message_payload)
        free(output_message);
    free(message_payload);
    free(compressed_message);

    // now we call mail size enforcement script
    system(MAIL_SIZE_SCRIPT);

    return EXIT_SUCCESS;
}
