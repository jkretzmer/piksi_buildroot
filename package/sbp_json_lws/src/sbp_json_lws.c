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

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/sbp_zmq_tx.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libsbp/sbp.h>
#include <libsbp/navigation.h>

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <libwebsockets.h>

#define PROGRAM_NAME "sbp_json_lws"
#define PUB_ENDPOINT ">tcp://localhost:43031"
#define SUB_ENDPOINT ">tcp://localhost:43030"
#define EXAMPLE_RX_BUFFER_BYTES (10)

pthread_t sbp_zmq_pthread_t;
pthread_t json_lws_pthread_t;

struct payload
{
	unsigned char data[LWS_SEND_BUFFER_PRE_PADDING + EXAMPLE_RX_BUFFER_BYTES
	+ LWS_SEND_BUFFER_POST_PADDING];
	size_t len;
} received_payload;

static int callback_example( struct lws *wsi, enum lws_callback_reasons reason,
	void *user, void *in, size_t len )
{
	switch( reason )
	{
		case LWS_CALLBACK_RECEIVE:
			memcpy( &received_payload.data[LWS_SEND_BUFFER_PRE_PADDING], in, len );
			received_payload.len = len;
			lws_callback_on_writable_all_protocol( lws_get_context( wsi ),
				lws_get_protocol( wsi ) );
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			lws_write( wsi, &received_payload.data[LWS_SEND_BUFFER_PRE_PADDING],
				received_payload.len, LWS_WRITE_TEXT );
			break;

		default:
			break;
	}

	return 0;
}

static int callback_http( struct lws *wsi, enum lws_callback_reasons reason,
  void *user, void *in, size_t len ) {
	switch( reason ) {
		case LWS_CALLBACK_HTTP:
			lws_serve_http_file( wsi, "example.html", "text/html", NULL, 0 );
			break;

		default:
			break;
	}

	return 0;
}

enum protocols {
  PROTOCOL_HTTP = 0,
  PROTOCOL_EXAMPLE,
  PROTOCOL_COUNT
};

static struct lws_protocols protocols[] =
{
  /* The first protocol must always be the HTTP handler */
  {
    "http-only",   /* name */
    callback_http, /* callback */
    0,             /* No per session data. */
    0,             /* max frame size / rx buffer */
  },
  {
    "example-protocol",
    callback_example,
    0,
    EXAMPLE_RX_BUFFER_BYTES,
  },
  { NULL, NULL, 0, 0 } /* terminator */
};


void* json_lws_thread(void *arg)
{
  pthread_t id = pthread_self();

  struct lws_context_creation_info json_lws_info;
  memset( &json_lws_info, 0, sizeof(json_lws_info) );
  json_lws_info.port = 8000;
  json_lws_info.protocols = protocols;
  json_lws_info.gid = -1;
  json_lws_info.uid = -1;

  struct lws_context *json_lws_context = lws_create_context( &json_lws_info );

  while( 1 ) {
    lws_service( json_lws_context, 1000000 );
  }

  lws_context_destroy( json_lws_context );

  return NULL;
}

static void sbp2json_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  sbp_zmq_rx_ctx_t *ctx = (sbp_zmq_rx_ctx_t *)context;
  sbp_state_t *s = &ctx->sbp_state;
  u16 s_msg_type = s->msg_type;
  char json_str[1024];
  sbp2json(sender_id, s_msg_type, len, msg, 1024, json_str);
  //printf("%s\n", json_str);
  return;
}

void* sbp_zmq_thread(void *arg) {
  pthread_t id = pthread_self();

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  sbp_zmq_pubsub_ctx_t *ctx = sbp_zmq_pubsub_create(PUB_ENDPOINT,
    SUB_ENDPOINT);

  if (ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  sbp_zmq_rx_ctx_t *rx_ctx = sbp_zmq_pubsub_rx_ctx_get(ctx);

  if (rx_ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  sbp_zmq_rx_callback_register(rx_ctx, 0, sbp2json_callback, rx_ctx, NULL);
  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));
  sbp_zmq_pubsub_destroy(&ctx);

  return NULL;
}


int main(void) {
  logging_init(PROGRAM_NAME);

  // start the sbp zmq processing thread
  int err;
  err = pthread_create(&sbp_zmq_pthread_t, NULL, &sbp_zmq_thread, NULL);
  if (err != 0) {
    printf("can't create sbp zmq thread :[%s]", strerror(err));
  }
  else {
    printf("sbp zmq thread created successfully\n");
  }

  err = pthread_create(&json_lws_pthread_t, NULL, &json_lws_thread, NULL);
  if (err != 0) {
    printf("can't create json lws thread :[%s]", strerror(err));
  }
  else {
    printf("json lws thread created successfully\n");
  }

  pthread_join(sbp_zmq_pthread_t, NULL);
  pthread_join(json_lws_pthread_t, NULL);

  exit(EXIT_SUCCESS);
}
