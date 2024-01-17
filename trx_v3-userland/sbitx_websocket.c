/* sBitx control daemon - HERMES
 *
 * Copyright (C) 2023-2024 Rhizomatica
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

// Based on https://mongoose.ws/tutorials/websocket-server/

#include <pthread.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <stdatomic.h>

#include "mongoose.h"
#include "sbitx_websocket.h"
#include "sbitx_core.h"

static const char *s_listen_on = "wss://0.0.0.0:8080";
static char s_web_root[1000];
static char session_cookie[100];
struct mg_mgr mgr;  // Event manager

extern _Atomic bool send_ws_update;
extern _Atomic bool shutdown_;

char request[200];
int request_index = 0;

static void web_respond(struct mg_connection *c, char *message)
{
    mg_ws_send(c, message, strlen(message), WEBSOCKET_OP_TEXT);
}

static void web_despatcher(struct mg_connection *c, struct mg_ws_message *wm){
    if (wm->data.len > 99)
        return;

    strncpy(request, wm->data.ptr, wm->data.len);
    request[wm->data.len] = 0;
    char *cookie = NULL;
    char *field = NULL;
    // char *value = NULL;

    cookie = strtok(request, "\n");
    field = strtok(NULL, "=");
    // value = strtok(NULL, "\n");

    if (cookie != NULL)
    {
        printf("Cookie: %s\nSession Cookie: %s\n", cookie, session_cookie);
	}

    if (field == NULL || cookie == NULL)
    {
        printf("Invalid request on websocket\n");
        web_respond(c, "quit Invalid request on websocket");
        c->is_draining = 1;
    }
    else if (strlen(field) > 100 || strlen(field) <  2 || strlen(cookie) > 40 || strlen(cookie) < 4)
    {
        printf("Ill formed request on websocket\n");
        web_respond(c, "quit Ill formed request");
        c->is_draining = 1;
    }
}

// This RESTful server implements the following endpoints:
//   /websocket - upgrade to Websocket, and implement HERMES websocket streaming
//   any other URI serves static files from s_web_root
static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_ACCEPT)
    {
        struct mg_tls_opts opts =
            {
                .cert = CFG_SSL_CERT,    // Certificate file
                .key = CFG_SSL_KEY,  // Private key file
            };
        mg_tls_init(c, &opts);
    }
    else if (ev == MG_EV_OPEN)
    {
        // c->is_hexdumping = 1;
	}
    else if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE)
    {
//		if (ev == MG_EV_ERROR)
//			printf("closing with MG_EV_ERROR : ");
//		if (ev = MG_EV_CLOSE)
//			printf("closing with MG_EV_CLOSE : ");
    }
    else if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        if (mg_http_match_uri(hm, "/websocket"))
        {
            // Upgrade to websocket. From now on, a connection is a full-duplex
            // Websocket connection, which will receive MG_EV_WS_MSG events.
            mg_ws_upgrade(c, hm, NULL);
            printf("WebSocket opened Ptr %p\n", c);
        }
        else if (mg_http_match_uri(hm, "/rest"))
        {
            // Serve REST response
            mg_http_reply(c, 200, "", "{\"result\": %d}\n", 123);
        }
        else
        {
            // Serve static files
            struct mg_http_serve_opts opts = {.root_dir = s_web_root};
            mg_http_serve_dir(c, ev_data, &opts);
        }
    }
    else if (ev == MG_EV_WS_MSG)
    {
        // Got websocket frame. Received data is wm->data. Echo it back!
        struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
        //		printf("ws request,  client to %x:%d\n", c->rem.ip, c->rem.port);
        web_despatcher(c, wm);
    }
}

void *webserver_thread_function(void *radio_h_v)
{
    mg_mgr_init(&mgr);  // Initialise event manager
    mg_http_listen(&mgr, s_listen_on, fn, NULL);  // Create HTTPS listener
    uint64_t counter = 0;
    uint64_t pwr_counter = 1;

    radio *radio_h = (radio *) radio_h_v;

    while (!shutdown_)
    {
        // lock
        mg_mgr_poll(&mgr, 100);

        for(struct mg_connection* c = mgr.conns; c != NULL; c = c->next)
        {
            if( c->is_accepted && c->is_websocket && !c->is_draining)
            {
// A: caso INTX
//  .type: Number , // 0
//  .fwd_watts: Number,
//  .swr: Number,

                char buff[4096];
                if ((radio_h->txrx_state == IN_TX) && !(pwr_counter % 2))
                {
                    sprintf(buff, "{\"type\": 0,\n");
                    sprintf(buff+strlen(buff), "\"fwd_watts\": %u,\n", get_fwd_power(radio_h));
                    sprintf(buff+strlen(buff), "\"swr\": %u}", get_swr(radio_h));
                    mg_ws_send(c, buff, strlen(buff), WEBSOCKET_OP_TEXT);
                }
                else
// B: caso geral
//  .type: Number , // 1
//  .tx: Boolean,
//  .rx: Boolean,
//  .led: Boolean,
//  .connection: Boolean,
//  .mode: String,
//  .protection: Boolean,
//  .freq: Number

                if ((!(counter % 4)) || radio_h->send_ws_update) // each 300 ms... or when there are changes
                {
                    sprintf(buff, "{\"type\": 1,\n");
                    sprintf(buff+strlen(buff), "\"rx\": %s,\n", radio_h->txrx_state ? "false":"true");
                    sprintf(buff+strlen(buff), "\"tx\": %s,\n", radio_h->txrx_state ? "true":"false");
                    sprintf(buff+strlen(buff), "\"led\": %s,\n", radio_h->system_is_ok ? "true":"false");
                    sprintf(buff+strlen(buff), "\"connection\": %s,\n", radio_h->system_is_connected ? "true":"false");
                    sprintf(buff+strlen(buff), "\"profile\": %u,\n", radio_h->profile_active_idx);
                    for (int i = 0; i < radio_h->profiles_count; i++)
                    {
                        radio_profile *curr_prof = &radio_h->profiles[i];
                        sprintf(buff+strlen(buff), "\"p%d_freq\": %u,\n", i, curr_prof->freq);
                        sprintf(buff+strlen(buff), "\"p%d_volume\": %d,\n", i, curr_prof->speaker_level);
                        if (curr_prof->mode == MODE_USB)
                            sprintf(buff+strlen(buff), "\"p%d_mode\": \"USB\",\n", i);
                        else if (curr_prof->mode == MODE_LSB)
                            sprintf(buff+strlen(buff), "\"p%d_mode\": \"LSB\",\n", i);
                        else if (curr_prof->mode == MODE_CW)
                            sprintf(buff+strlen(buff), "\"p%d_mode\": \"CW\",\n", i);
                    }
                    sprintf(buff+strlen(buff), "\"protection\": %s}", radio_h->swr_protection_enabled ?"true":"false");

                    mg_ws_send( c, buff, strlen(buff), WEBSOCKET_OP_TEXT );
                }
            }
        }
        counter++;
        pwr_counter++;
        if (radio_h->send_ws_update)
            radio_h->send_ws_update = false;
    }

    for(struct mg_connection* c = mgr.conns; c != NULL; c = c->next )
    {
        c->is_closing = 1;
    }
    mg_mgr_poll(&mgr, 100);

	printf("Exiting webserver thread\n");

    return NULL;
}

void websocket_shutdown(pthread_t *web_tid)
{
    pthread_join(*web_tid, NULL);
    mg_mgr_free(&mgr);
}

static pthread_t webserver_thread;

void websocket_init(radio *radio_h, char *web_path, pthread_t *web_tid){
	strcpy(s_web_root, web_path);
 	pthread_create(&webserver_thread, NULL, webserver_thread_function, (void*) radio_h);
}
