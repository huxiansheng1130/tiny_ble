/*
 * BLE PHY 软件 TIFS Demo — 广播状态机 (nRF54L15: TIMER10 + DPPI)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adv.h"
#include "hal_radio.h"
#include "ll_pdu.h"

static struct k_timer adv_timer;
static struct k_work  adv_work;

static void adv_on_channel(uint32_t channel)
{
	radio_set_channel(channel);

	/*==============================================================
	 * 第 1 阶段: TX ADV_IND
	 *==============================================================*/
	radio_packetptr_set(&pdu_adv_ind);
	radio_status_reset();
	radio_shorts_trx_set();

	/*
	 * CC[0] TX→RX: T_IFS - RX_RAMP = 110μs
	 * CC[1] RX→TX: T_IFS - TX_RAMP - RX_CHAIN = 100μs
	 */
	sw_timer_cc_set(CC_IDX_RXEN, T_IFS_US - RX_RAMP_US);
	sw_timer_cc_set(CC_IDX_TXEN, T_IFS_US - TX_RAMP_US - RX_CHAIN_DELAY_US);

	dppi_disable_switch();
	sw_timer_start();
	radio_tx_enable();

	/* 与 tiny_ble 相同：等 DISABLED（SHORTS: PHYEND→DISABLE） */
	while (!radio_event_disabled_check()) {
	}
	radio_event_disabled_clear();
	radio_event_end_clear();

	/*==============================================================
	 * 第 1.5 阶段: TX→RX (SW TIFS)
	 *==============================================================*/
	sw_timer_clear_compare(CC_IDX_RXEN);
	sw_timer_clear_compare(CC_IDX_TXEN);

	radio_packetptr_set(&pdu_rx_buf);
	radio_shorts_trx_set();
	dppi_enable_rxen();

	/*==============================================================
	 * 第 2 阶段: RX SCAN_REQ
	 *==============================================================*/
	radio_event_address_clear();

	{
		uint32_t start = k_cycle_get_32();
		uint32_t timeout = k_us_to_cyc_ceil32(RX_TIMEOUT_US);

		while (!radio_event_address_check()) {
			if ((k_cycle_get_32() - start) > timeout) {
				radio_ensure_disabled();
				return;
			}
		}
	}

	dppi_disable_switch();
	dppi_enable_txen();

	while (!radio_event_end_check()) {
	}
	radio_event_end_clear();

	sw_timer_clear_compare(CC_IDX_TXEN);
	sw_timer_clear_compare(CC_IDX_RXEN);

	if (!radio_crc_ok() || !validate_scan_req(&pdu_rx_buf)) {
		radio_ensure_disabled();
		return;
	}

	scan_req_count++;

	/*==============================================================
	 * 第 3 阶段: TX SCAN_RSP (DPPI 自动 TXEN)
	 *==============================================================*/
	radio_packetptr_set(&pdu_scan_rsp);
	radio_shorts_trx_set();
	radio_event_end_clear();
	radio_event_disabled_clear();

	while (!radio_event_disabled_check()) {
	}

	dppi_disable_switch();
	radio_shorts_clear();
	scan_rsp_count++;
}

void adv_worker(struct k_work *work)
{
	ARG_UNUSED(work);

	adv_on_channel(37);
	adv_on_channel(38);
	adv_on_channel(39);

	adv_event_count++;

	if (adv_event_count <= 3 || adv_event_count % 10 == 0) {
		printk("[SW-TIFS] events: %u | SCAN_REQ rx: %u | SCAN_RSP tx: %u\n",
		       adv_event_count, scan_req_count, scan_rsp_count);
	}
}

void adv_timer_expiry(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	k_work_submit(&adv_work);
}

void adv_init(void)
{
	k_work_init(&adv_work, adv_worker);
	k_timer_init(&adv_timer, adv_timer_expiry, NULL);
}

void adv_start(void)
{
	k_timer_start(&adv_timer, K_NO_WAIT, K_MSEC(ADV_INTERVAL_MS));
}
