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

 #define PROGRAM_NAME "sbp_json_bridge"

 #define PUB_ENDPOINT ">tcp://localhost:43031"
 #define SUB_ENDPOINT ">tcp://localhost:43030"

 static void sbp2json_callback(u16 sender_id, u8 len, u8 msg[], void *context)
 {
   sbp_zmq_rx_ctx_t *ctx = (sbp_zmq_rx_ctx_t *)context;
   sbp_state_t *s = &ctx->sbp_state;
   u16 s_msg_type = s->msg_type;
   char json_str[1024];
   sbp2json(sender_id, s_msg_type, len, msg, 1024, json_str);
   printf("%s\n", json_str);
   return;
 }

 static void sbp_msg_pos_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context)
 {
   printf("Recieved SBP_MSG_POS_LLH\n");
   char test_str[1024];
   msg_pos_llh_t_to_json_str( sender_id, SBP_MSG_POS_LLH, len,
     ( msg_pos_llh_t* ) msg, 1024, test_str);

   printf("%s\n", test_str);
   return;
 }

 void callback_setup(sbp_zmq_rx_ctx_t *rx_ctx, sbp_zmq_tx_ctx_t *tx_ctx)
 {
   sbp_zmq_rx_callback_register(rx_ctx, 0,
                                sbp2json_callback, rx_ctx, NULL);
 }

 int main(void)
 {
   logging_init(PROGRAM_NAME);

   /* Prevent czmq from catching signals */
   zsys_handler_set(NULL);

   sbp_zmq_pubsub_ctx_t *ctx = sbp_zmq_pubsub_create(PUB_ENDPOINT, SUB_ENDPOINT);
   if (ctx == NULL) {
     exit(EXIT_FAILURE);
   }

   callback_setup(sbp_zmq_pubsub_rx_ctx_get(ctx),
                  sbp_zmq_pubsub_tx_ctx_get(ctx));

   zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

   sbp_zmq_pubsub_destroy(&ctx);

   exit(EXIT_SUCCESS);
 }
