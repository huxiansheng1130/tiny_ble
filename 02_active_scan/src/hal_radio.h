/*
 * BLE PHY 软件 TIFS Demo — HAL: Radio / TIMER10 / DPPIC10
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HAL_RADIO_INIT_H
#define HAL_RADIO_INIT_H

#include "ble_common.h"

void hfclk_start(void);
void radio_configure(void);
void sw_switch_timer_configure(void);
void dppi_configure(void);
void radio_ensure_disabled(void);

/* 运行时辅助 */
void radio_set_channel(uint8_t ble_channel);
void radio_packetptr_set(void *pkt);
void radio_status_reset(void);
void radio_shorts_trx_set(void);
void radio_shorts_clear(void);
void radio_tx_enable(void);
bool radio_crc_ok(void);

void sw_timer_cc_set(uint8_t cc_idx, uint32_t us);
void sw_timer_start(void);
void sw_timer_stop(void);
void sw_timer_clear_compare(uint8_t cc_idx);

void dppi_enable_rxen(void);
void dppi_enable_txen(void);
void dppi_disable_switch(void);

bool radio_event_end_check(void);
void radio_event_end_clear(void);
bool radio_event_disabled_check(void);
void radio_event_disabled_clear(void);
bool radio_event_address_check(void);
void radio_event_address_clear(void);

#endif /* HAL_RADIO_INIT_H */
