/*
 * BLE PHY HCI Demo — HAL: Radio / TIMER10 / DPPIC10 / TIMER20
 *
 * 目标板: nRF54L15DK。写法对齐 02_active_scan。
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HAL_RADIO_INIT_H
#define HAL_RADIO_INIT_H

#include "ble_common.h"

void hfclk_start(void);
void rtc0_start(void);
void sw_switch_timer_configure(void);
void dppi_configure(void);

void radio_ensure_disabled(void);
void radio_configure_adv(void);
void radio_configure_conn(void);
void data_chan_set(uint8_t chan);

void radio_irq_disable(void);
void radio_shorts_trx_set(void);
void radio_freq_chan_set(uint32_t freq_off);
void radio_whiten_iv_set(uint8_t iv);
void radio_pkt_tx_set(void *pkt);
void radio_pkt_rx_set(void *pkt);
void radio_status_reset(void);
bool radio_crc_is_valid(void);
bool radio_is_done(void);

void sw_timer_cc_set(uint8_t cc_idx, uint32_t us);
void sw_timer_start(void);
void sw_timer_clear_compare(uint8_t cc_idx);

void dppi_enable_rxen(void);
void dppi_enable_txen(void);
void dppi_disable_switch(void);

bool radio_event_end_check(void);
void radio_event_end_clear(void);

uint32_t ticker_now(void);

uint32_t radio_tmr_start(uint8_t trx, uint32_t ticks_start, uint32_t remainder);
void radio_tmr_status_reset(void);
void radio_tmr_aa_capture(void);
void radio_tmr_aa_save(uint32_t aa);
void radio_tmr_end_capture(void);
void radio_tmr_hcto_configure(uint32_t hcto_us);
void radio_tmr_stamp_poll(void);
bool radio_hcto_expired(void);
uint32_t radio_tmr_aa_get(void);
uint32_t radio_tmr_ready_get(void);
uint32_t radio_rx_ready_delay_get(uint8_t phy, uint8_t flags);
uint32_t radio_rx_chain_delay_get(uint8_t phy, uint8_t flags);

#endif /* HAL_RADIO_INIT_H */
