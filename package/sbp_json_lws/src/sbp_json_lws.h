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

#ifndef SWIFTNAV_SBP_JSON_LWS_H
#define SWIFTNAV_SBP_JSON_LWS_H
#endif

/*  libpiksi includes for zmq connection */
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/sbp_zmq_tx.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>

/*  libsbp includes for sbp2json parsing */
#include <libsbp/sbp.h>

/* libwebsockets for lightweight webserver */
#include <libwebsockets.h>

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>

/* sbp websocket traffic callback */
static int sbp_ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
  void* user, void* in, size_t len);

/* http serve file callback */
static int http_serve_file_callback(struct lws *wsi,
  enum lws_callback_reasons reason, void *user, void *in, size_t len);

/* sbp2json_callback - called upon receipt of new SBP data */
void sbp2json_callback(u16 sender_id, u8 len, u8 msg[], void *context);
