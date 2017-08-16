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
#define SBP_JSON_REQ "SBP_JSON_REQ"
#define PUB_ENDPOINT ">tcp://localhost:43031"
#define SUB_ENDPOINT ">tcp://localhost:43030"
#define WEBSERVER_PORT (5000)
#define SBP_MSG_ALL (0)
#define SBP_JSON_MAX_SIZE (2048)
#define SBP_JSON_MAX_ITEMS (100)
#define SBP_PRUNE_SIZE (20)
#define SOCKET_BUFFER_SIZE (4096)
#define LOCAL_RESOURCE_PATH "/etc/client/"

const char INTERFACE_FILE[] = "/etc/client/index.html";

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

int sbp_json_list_size = 0;

struct sbp_json_node {
  char json_text[SBP_JSON_MAX_SIZE];
  struct sbp_json_node* next;
};

struct sbp_json_node* sbp_head = NULL;
struct sbp_json_node* sbp_tail = NULL;

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

static const struct lws_http_mount mount = {
	NULL,		/* linked-list pointer to next*/
	"/",		/* mountpoint in URL namespace on this vhost */
	LOCAL_RESOURCE_PATH, /* where to go on the filesystem for that */
	"index.html",	/* default filename if none given */
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0,
	0,
	0,
	0,
	0,
	LWSMPRO_FILE,	/* mount type is a directory in a filesystem */
	1,		/* strlen("/"), ie length of the mountpoint */
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
    char ws_cmd_buffer[255];
    memset(ws_cmd_buffer, '\0', 255);
    memcpy(&ws_cmd_buffer, in, len);

    if (strncmp(ws_cmd_buffer, SBP_JSON_REQ, 255) == 0){
      printf("recv: %s\n", ws_cmd_buffer);
      lws_callback_on_writable_all_protocol(lws_get_context(wsi),
        lws_get_protocol(wsi));
    }

    break;
  case LWS_CALLBACK_SERVER_WRITEABLE:
  {
    // keep track of how much data is to be sent
    int write_msg_size = 0;
    // store a pointer to where the JSON data begins in the write_buffer
    unsigned char *data_location = NULL;
    // lock mutex
    pthread_mutex_lock(&sbp_json_buffer_mutex);
    // if the sbp_head is NULL, the queue has not yet been initialized.
    if (!sbp_head) {
      printf("sbp_head is NULL - JSON queue is empty\n");
    }
    else {
      /* setup buffer for LWS write call
       * format is LWS_SEND_BUFFER_PRE_PADDING + DATA
       * + LWS_SEND_BUFFER_POST_PADDING
       */
      unsigned char write_buffer[LWS_SEND_BUFFER_PRE_PADDING + SOCKET_BUFFER_SIZE
         + LWS_SEND_BUFFER_POST_PADDING];

      data_location = &write_buffer[LWS_SEND_BUFFER_PRE_PADDING];
      // null out buffer
      memset(data_location, '\0', SOCKET_BUFFER_SIZE);
      // pointer for keeping track of write location
      unsigned char *buffer_write_location = data_location;
      // loop through list of sbp_jsone_node until the next node is the tail
      while(sbp_head->next != sbp_tail) {
        // create a pointer to store the current head
        struct sbp_json_node* current_node = sbp_head;
        /* sbp json messages are assumed to be null terminated
         * use strlen to get the length of the current json string
         */
        int current_msg_size =  strlen(current_node->json_text);
        /* check to see if the current message (plus a null character)
         * will fit in the write_buffer
         */
        if (write_msg_size + current_msg_size +1 < SOCKET_BUFFER_SIZE) {
          /* copy JSON from current_node in to write_buffer
           * @ buffer_write_location
           */
          memcpy(buffer_write_location, current_node->json_text, current_msg_size);
          // move buffer_write_location forward by amount written + 1
          //printf("%s\n", buffer_write_location);
          buffer_write_location += current_msg_size+1;
          // update the size of msg to be sent
          write_msg_size += current_msg_size+1;
          // move the sbp_head pointer to the next node
          sbp_head = sbp_head->next;
          // decrement the sbp_json_list_size (debugging info only)
          sbp_json_list_size--;
          // free the current node
          free(current_node);
          if (sbp_json_list_size > 40){
            printf("sbp_json_list_size: %d\n", sbp_json_list_size);
          }
        }
        else {
          break;
        }
      }
    }
    pthread_mutex_unlock(&sbp_json_buffer_mutex);
    if (write_msg_size > 0) {
      lws_write(wsi, data_location, write_msg_size, LWS_WRITE_TEXT);
    }
    else
    {
      //printf("No data to send\n");
      pthread_mutex_lock(&sbp_json_buffer_mutex);
      sbp_json_data_ready = false;

      while(sbp_json_data_ready == false) {
        pthread_cond_wait(&sbp_json_data_ready_cond, &sbp_json_buffer_mutex);
      }
      pthread_mutex_unlock(&sbp_json_buffer_mutex);
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


  struct sbp_json_node* sbp_data = (struct sbp_json_node*)malloc(sizeof(struct sbp_json_node));
  if (sbp_data != NULL) {
    memset(&sbp_data->json_text, '\0', SBP_JSON_MAX_SIZE);
    sbp2json(sender_id, s_msg_type, len, msg, SBP_JSON_MAX_SIZE, sbp_data->json_text);
  }


  pthread_mutex_lock(&sbp_json_buffer_mutex);

  if (sbp_json_list_size >= SBP_JSON_MAX_ITEMS) {
      // printf("SBP JSON BUFFER full, pruning\n");
      // consumer is not keeping up with buffer, prune first SBP_PRUNE_SIZE items
      for(int i=0; i < SBP_PRUNE_SIZE; i++) {
        struct sbp_json_node* current_node = sbp_head;
        sbp_head = sbp_head->next;
        sbp_json_list_size--;
        free(current_node);
      }
  }

  if(sbp_head == NULL && sbp_tail == NULL) {
    sbp_head = sbp_tail = sbp_data;
  }

  sbp_tail->next = sbp_data;
  sbp_tail = sbp_data;
  sbp_json_list_size++;

  sbp_json_data_ready = true;
  pthread_cond_signal(&sbp_json_data_ready_cond);
  pthread_mutex_unlock(&sbp_json_buffer_mutex);

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
  /* tell lws about our mount we want */
	context_info.mounts = &mount;
  // create lws_context from context info struct
  struct lws_context *sbp_ws_context = lws_create_context(&context_info);
  // loop infinitley on lws_service
  while(1) {
    lws_service(sbp_ws_context, 50);
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
