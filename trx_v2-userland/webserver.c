// Based on https://mongoose.ws/tutorials/websocket-server/

#include "mongoose.h"
#include "webserver.h"
#include <pthread.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <wiringPi.h>
#include "sdr.h"
#include "sdr_ui.h"

static const char *s_listen_on = "wss://0.0.0.0:8080";
static char s_web_root[1000];
static char session_cookie[100];
struct mg_mgr mgr;  // Event manager


int set_field(char *id, char *value);

static void web_respond(struct mg_connection *c, char *message){
	mg_ws_send(c, message, strlen(message), WEBSOCKET_OP_TEXT);
}

static void get_console(struct mg_connection *c){
	char buff[2100];
	
	int n = web_get_console(buff, 2000);
	if (!n)
		return;
	mg_ws_send(c, buff, strlen(buff), WEBSOCKET_OP_TEXT);
}


static void get_updates(struct mg_connection *c, int all){
	//send the settings of all the fields to the client
	char buff[2000];
	int i = 0;

	get_console(c);


	while(1){
		int update = remote_update_field(i, buff);
		// return of -1 indicates the eof fields
		if (update == -1)
			return;
	//send the status anyway
		if (all || update )
			mg_ws_send(c, buff, strlen(buff), WEBSOCKET_OP_TEXT); 
		i++;
	}
}

char request[200];
int request_index = 0;

static void web_despatcher(struct mg_connection *c, struct mg_ws_message *wm){
	if (wm->data.len > 99)
		return;

	strncpy(request, wm->data.ptr, wm->data.len);	
	request[wm->data.len] = 0;
	//handle the 'no-cookie' situation
	char *cookie = NULL;
	char *field = NULL;
	char *value = NULL;

	cookie = strtok(request, "\n");
	field = strtok(NULL, "=");
	value = strtok(NULL, "\n");

    if (cookie != NULL){ //  || strcmp(cookie, session_cookie)){
		printf("Cookie: %s\nSession Cookie: %s\n", cookie, session_cookie);
	}

	if (field == NULL || cookie == NULL){
		printf("Invalid request on websocket\n");
		web_respond(c, "quit Invalid request on websocket");
		c->is_draining = 1;
	}
	else if (strlen(field) > 100 || strlen(field) <  2 || strlen(cookie) > 40 || strlen(cookie) < 4){
		printf("Ill formed request on websocket\n");
		web_respond(c, "quit Illformed request");
		c->is_draining = 1;
	}
	else if (!strcmp(field, "refresh"))
		get_updates(c, 1);
	else{
		char buff[1200];
		if (value)
			sprintf(buff, "%s %s", field, value);
		else
			strcpy(buff, field);
        printf("TODO: execute %s\n", buff);
		get_updates(c, 0);
	}
}

// This RESTful server implements the following endpoints:
//   /websocket - upgrade to Websocket, and implement websocket echo server
//   /rest - respond with JSON string {"result": 123}
//   any other URI serves static files from s_web_root
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if (ev == MG_EV_ACCEPT) {
        struct mg_tls_opts opts = {
            .cert = "/etc/ssl/private/hermes.radio.crt",    // Certificate file
            .certkey = "/etc/ssl/private/hermes.key",  // Private key file
        };
        mg_tls_init(c, &opts);
    }
    else if (ev == MG_EV_OPEN) {
    // c->is_hexdumping = 1;
	} else if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE){
//		if (ev == MG_EV_ERROR)
//			printf("closing with MG_EV_ERROR : ");
//		if (ev = MG_EV_CLOSE)
//			printf("closing with MG_EV_CLOSE : ");
  } else if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    if (mg_http_match_uri(hm, "/websocket")) {
      // Upgrade to websocket. From now on, a connection is a full-duplex
      // Websocket connection, which will receive MG_EV_WS_MSG events.
        mg_ws_upgrade(c, hm, NULL);
        printf("WebSocket opened Ptr %llu\n", c);
    } else if (mg_http_match_uri(hm, "/rest")) {
      // Serve REST response
      mg_http_reply(c, 200, "", "{\"result\": %d}\n", 123);
    } else {
      // Serve static files
      struct mg_http_serve_opts opts = {.root_dir = s_web_root};
      mg_http_serve_dir(c, ev_data, &opts);
    }
  } else if (ev == MG_EV_WS_MSG) {
    // Got websocket frame. Received data is wm->data. Echo it back!
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
//		printf("ws request,  client to %x:%d\n", c->rem.ip, c->rem.port);
    web_despatcher(c, wm);
  }
  (void) fn_data;
}

void *webserver_thread_function(void *server){
  mg_mgr_init(&mgr);  // Initialise event manager
  mg_http_listen(&mgr, s_listen_on, fn, NULL);  // Create HTTP listener
  for (;;) mg_mgr_poll(&mgr, 200);             // Infinite event loop
	printf("exiting webserver thread\n");
}

void webserver_stop(){
  mg_mgr_free(&mgr);
}

static pthread_t webserver_thread;

void webserver_start(){
	strcpy(s_web_root, "/etc/sbitx/web");

 	pthread_create( &webserver_thread, NULL, webserver_thread_function,
		(void*)NULL);
}
