/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Josh Kretzmer <jkretzmer@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "sbp_json_lws.h"

#define PROGRAM_NAME "sbp_json_lws"
#define PUB_ENDPOINT ">tcp://localhost:43031"
#define SUB_ENDPOINT ">tcp://localhost:43030"
#define WEBSERVER_PORT (5000)
#define SBP_MSG_ALL (0)
#define SBP_JSON_MAX_SIZE (2048)
#define SOCKET_BUFFER_SIZE (8192)
#define SBP_JSON_MAX_ITEMS (20)

const char INTERFACE_FILE[] = "/etc/syrinx.html";
char* SBP_JSON_QUEUE[256];

int sbp_write = 0;
int sbp_read = 0;

/* zmq connection and sbp2json thread handle */
pthread_t zmq_sbp2json_pthread_t;

/* webserver thread handle */
pthread_t webserver_pthread_t;

/* mutex for SBP JSON data */
pthread_mutex_t sbp_json_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

/* condition variable for buffer control */
pthread_cond_t sbp_json_data_ready_cond = PTHREAD_COND_INITIALIZER;
bool sbp_json_data_ready = false;

char SBP_JSON_BUFFER[SBP_JSON_MAX_SIZE];

/* lws_protocols - struct listing protocols in use by this webserver
 *
 * Notes:
 * The first protocol must always be the HTTP handler
 * The last protocol must be a terminator -  {NULL, NULL, 0}
 *
 * Protocols are of the format:
 *  {"name", (lws_callback_function), per_session_data}
 */
static struct lws_protocols protocols[] =
{
  {"http-only", (lws_callback_function*) http_serve_file_callback, 0},
  {"sbp-ws", (lws_callback_function*) sbp_ws_callback, 0},
  {NULL, NULL, 0} /* terminator */
};

static int sbp_ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
  void* user, void* in, size_t len)
{
  switch (reason) {
  case LWS_CALLBACK_ESTABLISHED:
    printf("callback established\n");
    break;
  case LWS_CALLBACK_RECEIVE:
    printf("callback received\n");
    lws_callback_on_writable_all_protocol(lws_get_context(wsi),
      lws_get_protocol(wsi));
    break;
  case LWS_CALLBACK_SERVER_WRITEABLE:
  {
    //printf("callback server writeable received\n");
    /* setup buffer for LWS write call
     * format is LWS_SEND_BUFFER_PRE_PADDING + DATA
     * + LWS_SEND_BUFFER_POST_PADDING
     */
    unsigned char write_buffer[LWS_SEND_BUFFER_PRE_PADDING + SBP_JSON_MAX_SIZE
      + LWS_SEND_BUFFER_POST_PADDING];
		unsigned char *data_location = &write_buffer[LWS_SEND_BUFFER_PRE_PADDING];
    // clear buffer
    memset(data_location, '\0', SBP_JSON_MAX_SIZE);
    int buffer_len = 0;
    pthread_mutex_lock(&sbp_json_buffer_mutex);
    //printf("lws->lock->sbp_json_buffer_mutex\n");

    while(sbp_json_data_ready == false) {
      pthread_cond_wait(&sbp_json_data_ready_cond, &sbp_json_buffer_mutex);
    }

    buffer_len = strlen(SBP_JSON_BUFFER);
    if (buffer_len != 0) {
      size_t data_size = sprintf((char *)data_location, "%s", SBP_JSON_BUFFER);
      memset(SBP_JSON_BUFFER, '\0', SBP_JSON_MAX_SIZE);
    }

    sbp_json_data_ready = false;
    pthread_mutex_unlock(&sbp_json_buffer_mutex);
    //printf("lws->unlock->sbp_json_buffer_mutex\n");

    if (buffer_len > 0) {
      //printf("lws data: %s\n", data_location);
      lws_write(wsi, data_location, buffer_len, LWS_WRITE_TEXT);
    }
    lws_callback_on_writable_all_protocol(lws_get_context(wsi),
      lws_get_protocol(wsi));

    break;
  }
  default:
    break;
  }
  return 0;
}

static int http_serve_file_callback(struct lws *wsi,
  enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
  switch(reason) {
  case LWS_CALLBACK_HTTP:
    lws_serve_http_file( wsi, INTERFACE_FILE, "text/html", NULL, 0 );
    break;
  default:
    break;
  }
  return 0;
}

void sbp2json_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{

  sbp_zmq_rx_ctx_t *ctx = (sbp_zmq_rx_ctx_t *)context;
  sbp_state_t *s = &ctx->sbp_state;
  u16 s_msg_type = s->msg_type;

  //printf("s_msg_type: %u\n", s_msg_type);
  pthread_mutex_lock(&sbp_json_buffer_mutex);
  memset(SBP_JSON_BUFFER, '\0', SBP_JSON_MAX_SIZE);
  sbp2json(sender_id, s_msg_type, len, msg, SBP_JSON_MAX_SIZE, SBP_JSON_BUFFER);
  //printf("s_msg_type: %u - strlen: %d\n", s_msg_type, strlen(SBP_JSON_BUFFER));
  sbp_json_data_ready = true;
  pthread_cond_signal(&sbp_json_data_ready_cond);
  pthread_mutex_unlock(&sbp_json_buffer_mutex);
  //printf("%s\n", SBP_JSON_BUFFER);

  return;
}

void* webserver_thread(void *arg)
{
  pthread_t id = pthread_self();
  //create lws context info - this specifies lws connection parameters
  struct lws_context_creation_info context_info;
  // memset the context_info to 0 to start clean
  memset(&context_info, 0, sizeof(context_info));
  // port for http and websocket request
  context_info.port = WEBSERVER_PORT;
  // protocols (from struct defined above)
  context_info.protocols = protocols;
  context_info.gid = -1;
  context_info.uid = -1;
  // create lws_context from context info struct
  struct lws_context *sbp_ws_context = lws_create_context(&context_info);
  // loop infinitley on lws_service
  while(1) {
    lws_service(sbp_ws_context, 500);
  }
  lws_context_destroy(sbp_ws_context);
  return NULL;
}

void* zmq_sbp2json_thread(void *arg)
{
  pthread_t id = pthread_self();
  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);
  /* get sbp_zmq_pubsub_ctx from PUB_ENDPOINT / SUB_ENDPOINT */
  sbp_zmq_pubsub_ctx_t *ctx = sbp_zmq_pubsub_create(PUB_ENDPOINT, SUB_ENDPOINT);
  if (ctx == NULL) {
    /* if the ctx returned is NULL, exit with EXIT_FAILURE */
    exit(EXIT_FAILURE);
  }
  /* get the rx ctx */
  sbp_zmq_rx_ctx_t *rx_ctx = sbp_zmq_pubsub_rx_ctx_get(ctx);
  if (rx_ctx == NULL) {
    /* if the ctx returned is NULL, exit with EXIT_FAILURE */
    exit(EXIT_FAILURE);
  }
  /* register a callback to fire on every SBP message recieved
   * (msg_id = 0)
   */
  sbp_zmq_rx_callback_register(rx_ctx, SBP_MSG_ALL, sbp2json_callback, rx_ctx,
    NULL);
  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));
  sbp_zmq_pubsub_destroy(&ctx);
  return NULL;
}

int main(void)
{
  logging_init(PROGRAM_NAME);

  // create the zmq_sb2json  thread
  int err;
  err = pthread_create(&zmq_sbp2json_pthread_t, NULL,
    &zmq_sbp2json_thread, NULL);
  if (err != 0) {
    printf("Could not create zmq_sbp2json thread :[%s]", strerror(err));
    exit(EXIT_FAILURE);
  } else {
    printf("zmq_sbp2json thread created successfully\n");
  }
  // create the webserver thread
  err = pthread_create(&webserver_pthread_t, NULL, &webserver_thread, NULL);
  if (err != 0) {
    printf("Could not create webserver thread :[%s]", strerror(err));
    exit(EXIT_FAILURE);
  } else {
    printf("webserver thread created successfully\n");
  }
  pthread_join(zmq_sbp2json_pthread_t, NULL);
  pthread_join(webserver_pthread_t, NULL);
  exit(EXIT_SUCCESS);
}
