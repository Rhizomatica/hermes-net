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
#include <strings.h> // strcasecmp / strncasecmp
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include <b64/cdecode.h>

#include "xz_compression.h"
#include "gz_compress.h"
#include "utils.h"
#include "daemon.h"

// support for hermes messaging subsystem
#define HERMES_SMS "sms@hermes.radio"

#if ((USE_GZ == 1) && (USE_XZ == 1)) || ((USE_GZ == 0) && (USE_XZ == 0))
#error Wrong compression configuration
#endif

// case-insensitive compare of a header field name of known length against a
// NUL-terminated literal
static bool header_name_eq(const char *name, size_t name_len, const char *lit)
{
    return strlen(lit) == name_len && strncasecmp(name, lit, name_len) == 0;
}

// Headers we keep.  Everything else (Received, DKIM-Signature, ARC-*, Autocrypt,
// Authentication-Results, Received-SPF, Return-Path, Delivered-To, every X-*, ...)
// is dropped.  An allowlist is safer than chasing an ever-growing blocklist.
static bool header_is_allowed(const char *name, size_t name_len)
{
    static const char *allow[] = {
        "From", "To", "Cc", "Bcc", "Reply-To", "Subject", "Date",
        "Message-ID", "In-Reply-To", "References", "MIME-Version",
        "Content-Type", "Content-Transfer-Encoding", "Content-Disposition",
        "Content-ID", "Content-Description", NULL
    };

    // DeltaChat control headers: Chat-Version, Chat-Group-ID, Chat-Content, ...
    if (name_len >= 5 && strncasecmp(name, "Chat-", 5) == 0)
        return true;

    for (int i = 0; allow[i] != NULL; i++)
        if (header_name_eq(name, name_len, allow[i]))
            return true;

    return false;
}

// Pull a bare e-mail address out of a header value such as
//   "Display Name" <user@host>   /   <user@host>   /   user@host (comment)
// The result is written NUL-terminated and whitespace-free into out.
static void extract_email(const char *value, size_t value_len,
                          char *out, size_t out_size)
{
    const char *start = value;
    const char *end = value + value_len;
    const char *lt, *gt;

    out[0] = '\0';
    if (out_size == 0)
        return;

    lt = memchr(start, '<', end - start);
    if (lt != NULL)
    {
        gt = memchr(lt, '>', end - lt);
        if (gt != NULL)
        {
            start = lt + 1;
            end = gt;
        }
    }

    size_t j = 0;
    for (const char *p = start; p < end && j + 1 < out_size; p++)
    {
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            continue;
        out[j++] = *p;
    }
    out[j] = '\0';
}

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

    // dry-run mode: transform stdin and write the result to stdout, without
    // daemonizing or invoking uux/compression.  Used by tests/run_tests.sh.
    bool dry_run = (getenv("UUXCOMP_DRY_RUN") != NULL);

    // read stdin (one extra byte so we can always NUL-terminate)
    message_size = fread(tmp_buffer, 1, BUF_SIZE, stdin);
    message_payload = malloc(message_size + 1);
    memcpy(message_payload, tmp_buffer, message_size);
    while ( !feof(stdin) )
    {
        size_t needle = message_size;
        buffer_size = fread(tmp_buffer, 1, BUF_SIZE, stdin);
        message_size += buffer_size;
        message_payload = realloc(message_payload, message_size + 1);
        memcpy(message_payload + needle, tmp_buffer, buffer_size);
    }
    // everything below treats message_payload as a C string (strstr etc.)
    message_payload[message_size] = '\0';
    output_message = message_payload;

    // daemonize and return the parent...
    if (!dry_run && become_daemon() != 0)
    {
        fprintf(stderr, "Error in daemon()\n");
    }

#if DEBUG_MODE > 0
    debug_output = dry_run ? stderr : fopen(DEBUG_FILENAME,"a");
    if (debug_output == NULL)
    {
        fprintf(stderr, "Failed to open debug output\n");
        exit(EXIT_FAILURE);
    }
#else
    debug_output = stderr;
#endif

    fprintf(debug_output, "uuxcomp %s starting.\n", VERSION);

    /* -----------------------------------------------------------------------
     * STRIP E-MAIL CRUFT
     *
     * Keep only an allowlist of headers plus the body, byte-for-byte.  We do
     * NOT round-trip through a MIME parser: re-serialising headers changes
     * their bytes (folding, address canonicalisation, auto-generated
     * Message-ID) and leaves no reliable way to find where the body begins.
     * --------------------------------------------------------------------- */
    char *line_break = uuxcomp_determine_linebreak(message_payload);
    if (line_break == NULL)
    {
        fprintf(debug_output, "No line break found, forwarding as is.\n");
        goto compress;
    }
    size_t lb_len = strlen(line_break);

    // Optional mbox "From " envelope line (RFC 4155): kept verbatim, it is not
    // an RFC 5322 header.  Absent when uux is called without one.
    char *env_line = message_payload;
    size_t env_len = 0;
    if (message_size >= 5 && strncmp(message_payload, "From ", 5) == 0)
    {
        char *eol = uuxcomp_mem_find(message_payload, message_size, line_break, lb_len);
        if (eol != NULL)
            env_len = (size_t)(eol - message_payload) + lb_len;
    }

    char *hdr_start = message_payload + env_len;
    size_t hdr_region = message_size - env_len;

    // header/body boundary == the first blank line
    char *sep = uuxcomp_find_blank_line(hdr_start, hdr_region, line_break);
    char *body;
    size_t body_len;
    size_t hdr_len;
    if (sep != NULL)
    {
        hdr_len  = (size_t)(sep - hdr_start) + lb_len;   // keep last header's EOL
        body     = sep + (2 * lb_len);
        body_len = (size_t)((message_payload + message_size) - body);
    }
    else
    {
        hdr_len  = hdr_region;
        body     = message_payload + message_size;
        body_len = 0;
    }

    // Walk the header block one (possibly folded) header at a time, copying
    // only allowlisted headers into new_headers.
    char *new_headers = malloc(hdr_len + 1);
    size_t new_headers_len = 0;
    bool to_is_sms = false;
    char sms_sender[256];
    sms_sender[0] = '\0';

    char *cur = hdr_start;
    char *hdr_end = hdr_start + hdr_len;
    int hdr_kept = 0, hdr_dropped = 0;
    while (cur < hdr_end)
    {
        char *hstart = cur;
        char *nl = uuxcomp_mem_find(cur, hdr_end - cur, line_break, lb_len);
        cur = (nl != NULL) ? nl + lb_len : hdr_end;
        while (cur < hdr_end && (*cur == ' ' || *cur == '\t'))   // folded continuation
        {
            nl = uuxcomp_mem_find(cur, hdr_end - cur, line_break, lb_len);
            cur = (nl != NULL) ? nl + lb_len : hdr_end;
        }
        size_t whole_len = (size_t)(cur - hstart);

        char *colon = memchr(hstart, ':', whole_len);
        if (colon == NULL)
        {
            hdr_dropped++;
            continue;                       // not a header line - drop it
        }

        size_t name_len = (size_t)(colon - hstart);
        while (name_len > 0 &&
               (hstart[name_len - 1] == ' ' || hstart[name_len - 1] == '\t'))
            name_len--;

        const char *val = colon + 1;
        size_t val_len = (size_t)(cur - (colon + 1));

        if (header_name_eq(hstart, name_len, "Chat-Version"))
            is_deltachat = true;

        if (header_name_eq(hstart, name_len, "From") && sms_sender[0] == '\0')
            extract_email(val, val_len, sms_sender, sizeof(sms_sender));

        if ((header_name_eq(hstart, name_len, "To") ||
             header_name_eq(hstart, name_len, "Cc")) &&
            uuxcomp_mem_find(val, val_len, HERMES_SMS, strlen(HERMES_SMS)) != NULL)
            to_is_sms = true;

        if (header_is_allowed(hstart, name_len))
        {
            memcpy(new_headers + new_headers_len, hstart, whole_len);
            new_headers_len += whole_len;
            hdr_kept++;
        }
        else
        {
            hdr_dropped++;
        }
    }
    fprintf(debug_output, "Headers: kept %d, dropped %d.\n", hdr_kept, hdr_dropped);

    // HERMES messaging / SMS: forward the body over uux and stop.
    if (to_is_sms)
    {
        fprintf(debug_output, "SMS/message for '%s'.\n", sms_sender);
        free(new_headers);

        FILE *msg_fp = dry_run ? stdout : popen("uux -r - gw\\!dec_message", "w");
        if (msg_fp != NULL)
        {
            fprintf(msg_fp, "From: %s\n", sms_sender);
            fwrite(body, 1, body_len, msg_fp);
            if (!dry_run)
                pclose(msg_fp);
        }
        free(message_payload);
        return EXIT_SUCCESS;
    }

    // Reassemble: [envelope] + kept headers + blank line + body.  Trailing
    // whitespace (MIME epilogue, stray blank lines) is dropped; one final
    // line break is guaranteed.
    while (body_len > 0)
    {
        char c = body[body_len - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            break;
        body_len--;
    }

    size_t rebuilt_size = env_len + new_headers_len + lb_len + body_len + lb_len;
    char *rebuilt = malloc(rebuilt_size + 1);
    size_t off = 0;
    if (env_len > 0)
    {
        memcpy(rebuilt + off, env_line, env_len);
        off += env_len;
    }
    memcpy(rebuilt + off, new_headers, new_headers_len);
    off += new_headers_len;
    memcpy(rebuilt + off, line_break, lb_len);       // blank line after headers
    off += lb_len;
    if (body_len > 0)
    {
        memcpy(rebuilt + off, body, body_len);
        off += body_len;
        memcpy(rebuilt + off, line_break, lb_len);   // final EOL
        off += lb_len;
    }
    rebuilt[off] = '\0';

    free(new_headers);
    free(message_payload);
    message_payload = rebuilt;
    message_size = off;
    output_message = message_payload;

    if (dry_run)
    {
        fwrite(output_message, 1, message_size, stdout);
        free(message_payload);
        return EXIT_SUCCESS;
    }
    /* END STRIP E-MAIL CRUFT */

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

    // start of the base64 payload: right after the part's header/body blank line
    char_ptr4 = uuxcomp_find_blank_line(char_ptr3, strlen(char_ptr3), line_break);

    if (char_ptr4 == NULL)
    {
        encoding_type = ENC_TYPE_NONE;
        goto compress;
    }

    char_ptr4 += 2 * lb_len;

    // end of the base64 payload: the blank line before the next MIME boundary
    char_ptr5 = uuxcomp_find_blank_line(char_ptr4, strlen(char_ptr4), line_break);

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
