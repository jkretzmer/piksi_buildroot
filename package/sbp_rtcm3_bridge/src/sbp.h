/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_SBP_H
#define SWIFTNAV_SBP_H

#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/sbp_zmq_tx.h>

int sbp_init(sbp_zmq_rx_ctx_t *rx_ctx, sbp_zmq_tx_ctx_t *tx_ctx);
int sbp_message_send(u16 msg_type, u8 len, u8 *payload);
int sbp_callback_register(u16 msg_type, sbp_msg_callback_t cb, void *context);

#endif /* SWIFTNAV_SBP_H */
