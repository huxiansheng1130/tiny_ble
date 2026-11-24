/*
 * BLE PHY First Anchor Demo — 广播态: ADV_IND 发送与 CONNECT_IND 接收
 *
 * (与 03_phy_connect_ind 的 adv.c 相同)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adv.h"
#include "hal_radio.h"
#include "ll_pdu.h"

/*===========================================================================
 * 广播信道处理
 *===========================================================================*/
int adv_on_channel(uint32_t channel)
{
	uint32_t freq;

	switch (channel) {
	case 37: freq = 2;  break;
	case 38: freq = 26; break;
	case 39: freq = 80; break;
	default: return 0;
	}

	radio_freq_chan_set(freq);
	radio_whiten_iv_set(channel);

	radio_irq_disable();

	/*==============================================================
	 * 第 1 阶段: TX ADV_IND (SW TIFS)
	 *==============================================================*/

	radio_pkt_tx_set(&pdu_adv_ind);

	radio_status_reset();
	NRF_RADIO->EVENTS_DISABLED = 0;
	radio_event_end_clear();
	NRF_RADIO->EVENTS_ADDRESS = 0;

	radio_shorts_trx_set();

	sw_timer_cc_set(CC_IDX_RXEN, T_IFS_US - RX_RAMP_US);
	dppi_disable_switch();

	sw_timer_start();

	NRF_RADIO->TASKS_TXEN = 1;

	while (!radio_event_end_check()) {
	}
	radio_event_end_clear();

	/*==============================================================
	 * TX→RX 转换 (SW TIFS)
	 *==============================================================*/

	sw_timer_clear_compare(CC_IDX_RXEN);

	radio_pkt_rx_set(&pdu_adv_rx);

	dppi_enable_rxen();

	while (NRF_RADIO->EVENTS_DISABLED == 0) {
	}
	NRF_RADIO->EVENTS_DISABLED = 0;

	/*==============================================================
	 * 第 2 阶段: RX 等待 CONNECT_IND
	 *==============================================================*/

	NRF_RADIO->EVENTS_ADDRESS = 0;

	uint32_t timeout_loops = ADV_RX_TIMEOUT_US * 16;
	bool rx_done = false;

	while (timeout_loops > 0) {
		if (NRF_RADIO->EVENTS_DISABLED) {
			rx_done = true;
			break;
		}
		timeout_loops--;
	}

	/* ★ 立即捕获 T0 */
	uint32_t rx_end_rtc = ticker_now() & HAL_TICKER_CNTR_MASK;

	if (!rx_done) {
		radio_ensure_disabled();
		return 0;
	}

	dppi_disable_switch();
	NRF_RADIO->SHORTS = 0;

	if (!radio_crc_is_valid()) {
		return 0;
	}

	if (validate_connect_ind(&pdu_adv_rx)) {
		connect_end_rtc = rx_end_rtc;
		parse_connect_ind(&pdu_adv_rx);
		return 1;
	}

	return 0;
}

/*===========================================================================
 * 广播事件
 *===========================================================================*/
int adv_event(void)
{
	static const uint32_t adv_channels[] = {37, 38, 39};

	for (int i = 0; i < 3; i++) {
		int ret = adv_on_channel(adv_channels[i]);

		if (ret == 1) {
			return 1;
		}
	}

	adv_event_count++;
	if (adv_event_count <= 3 || adv_event_count % 50 == 0) {
		printk("[ADV] events: %u (waiting for connection...)\n",
		       adv_event_count);
	}
	return 0;
}
