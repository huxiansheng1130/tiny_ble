/*
 * BLE PHY HCI Demo — HAL
 *
 * 目标板: nRF54L15DK
 *   TIMER10 + DPPIC10 : 软件 TIFS (与 02_active_scan 相同)
 *   TIMER20 @ 1 MHz   : 连接事件时基 (nRF54L15 没有 RTC0)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hal_radio.h"

#include <hal/nrf_radio.h>
#include <hal/nrf_timer.h>
#include <hal/nrf_dppi.h>

#define SW_TIMER   NRF_TIMER10
#define SW_DPPIC   NRF_DPPIC10
#define TICK_TIMER NRF_TIMER20

/* TIMER10 PCLK=32 MHz, prescaler 5 → 1 MHz */
#define SW_TIMER_PRESCALER \
	NRF_TIMER_PRESCALER_CALCULATE(NRF_TIMER_BASE_FREQUENCY_GET(SW_TIMER), 1000000)

/* TIMER20 PCLK=16 MHz, prescaler 4 → 1 MHz */
#define TICK_TIMER_PRESCALER \
	NRF_TIMER_PRESCALER_CALCULATE(NRF_TIMER_BASE_FREQUENCY_GET(TICK_TIMER), 1000000)

static bool tmr_ready_latched;
static bool tmr_aa_latched;
static uint32_t tmr_ready_us;
static uint32_t tmr_aa_us;
static uint32_t hcto_deadline;
static bool hcto_active;

static void radio_aa_crc_set(uint32_t aa, uint32_t crc_iv)
{
	NRF_RADIO->PREFIX0 = (aa >> 24) & 0xFFUL;
	NRF_RADIO->BASE0 = (aa & 0x00FFFFFFUL) << 8;

	NRF_RADIO->CRCCNF = (RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos) |
			    (RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos);
	NRF_RADIO->CRCPOLY = PDU_CRC_POLYNOMIAL;
	NRF_RADIO->CRCINIT = crc_iv;
}

static void radio_pkt_common(uint8_t maxlen)
{
	radio_irq_disable();
	NRF_RADIO->SHORTS = 0;

	NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;
	NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_0dBm;

	NRF_RADIO->PCNF0 = (8UL << RADIO_PCNF0_LFLEN_Pos) |
			   (1UL << RADIO_PCNF0_S0LEN_Pos) |
			   (RADIO_PCNF0_PLEN_8bit << RADIO_PCNF0_PLEN_Pos);

	NRF_RADIO->PCNF1 = ((uint32_t)maxlen << RADIO_PCNF1_MAXLEN_Pos) |
			   (0UL << RADIO_PCNF1_STATLEN_Pos) |
			   (3UL << RADIO_PCNF1_BALEN_Pos) |
			   (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos);

	NRF_RADIO->TXADDRESS = 0;
	NRF_RADIO->RXADDRESSES =
		RADIO_RXADDRESSES_ADDR0_Enabled << RADIO_RXADDRESSES_ADDR0_Pos;

	NRF_RADIO->TIFS = 0;
	nrf_radio_fast_ramp_up_enable_set(NRF_RADIO, true);
}

void hfclk_start(void)
{
	NRF_CLOCK->EVENTS_XOSTARTED = 0;
	NRF_CLOCK->TASKS_XOSTART = 1;
	while (NRF_CLOCK->EVENTS_XOSTARTED == 0) {
	}
	printk("HFCLK (HFXO) started\n");
}

void rtc0_start(void)
{
	nrf_timer_task_trigger(TICK_TIMER, NRF_TIMER_TASK_STOP);
	nrf_timer_task_trigger(TICK_TIMER, NRF_TIMER_TASK_CLEAR);
	nrf_timer_mode_set(TICK_TIMER, NRF_TIMER_MODE_TIMER);
	nrf_timer_bit_width_set(TICK_TIMER, NRF_TIMER_BIT_WIDTH_32);
	nrf_timer_prescaler_set(TICK_TIMER, TICK_TIMER_PRESCALER);
	nrf_timer_task_trigger(TICK_TIMER, NRF_TIMER_TASK_START);
	printk("Ticker (TIMER20) started: 1 MHz\n");
}

uint32_t ticker_now(void)
{
	nrf_timer_task_trigger(TICK_TIMER, NRF_TIMER_TASK_CAPTURE0);
	return nrf_timer_cc_get(TICK_TIMER, NRF_TIMER_CC_CHANNEL0);
}

void radio_irq_disable(void)
{
	NRF_RADIO->INTENCLR00 = 0xFFFFFFFFUL;
	NRF_RADIO->INTENCLR01 = 0xFFFFFFFFUL;
}

void radio_shorts_trx_set(void)
{
	NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk |
			    RADIO_SHORTS_PHYEND_DISABLE_Msk;
}

void radio_ensure_disabled(void)
{
	NRF_RADIO->SHORTS = 0;
	dppi_disable_switch();
	nrf_timer_task_trigger(SW_TIMER, NRF_TIMER_TASK_STOP);
	hcto_active = false;

	if (NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled) {
		NRF_RADIO->EVENTS_DISABLED = 0;
		NRF_RADIO->TASKS_DISABLE = 1;
		while (NRF_RADIO->EVENTS_DISABLED == 0) {
		}
	}
}

void radio_configure_adv(void)
{
	radio_pkt_common(PDU_AC_LEG_PAYLOAD_SIZE_MAX);
	radio_aa_crc_set(PDU_AC_ACCESS_ADDR, PDU_AC_CRC_IV);
	printk("Radio configured: ADV 1M (nRF54L15)\n");
}

void radio_configure_conn(void)
{
	uint32_t crc_iv = conn_params.crc_init[0] |
			  ((uint32_t)conn_params.crc_init[1] << 8) |
			  ((uint32_t)conn_params.crc_init[2] << 16);

	radio_pkt_common(PDU_DC_PAYLOAD_SIZE_MIN);
	radio_aa_crc_set(sys_get_le32(conn_params.access_addr), crc_iv);
	printk("Radio configured: CONN 1M (nRF54L15)\n");
}

void sw_switch_timer_configure(void)
{
	nrf_timer_task_trigger(SW_TIMER, NRF_TIMER_TASK_STOP);
	nrf_timer_task_trigger(SW_TIMER, NRF_TIMER_TASK_CLEAR);
	nrf_timer_int_disable(SW_TIMER, 0xFFFFFFFFUL);

	for (int i = 0; i < 8; i++) {
		nrf_timer_event_clear(SW_TIMER, nrf_timer_compare_event_get(i));
	}

	nrf_timer_mode_set(SW_TIMER, NRF_TIMER_MODE_TIMER);
	nrf_timer_bit_width_set(SW_TIMER, NRF_TIMER_BIT_WIDTH_16);
	nrf_timer_prescaler_set(SW_TIMER, SW_TIMER_PRESCALER);

	printk("SW_SWITCH_TIMER (TIMER10) configured: 1 MHz, 16-bit\n");
}

void dppi_configure(void)
{
	nrf_radio_publish_set(NRF_RADIO, NRF_RADIO_EVENT_PHYEND, DPPI_CH_TIMER_CLEAR);
	nrf_timer_subscribe_set(SW_TIMER, NRF_TIMER_TASK_CLEAR, DPPI_CH_TIMER_CLEAR);

	nrf_timer_publish_set(SW_TIMER, nrf_timer_compare_event_get(CC_IDX_RXEN),
			      DPPI_CH_RXEN);
	nrf_radio_subscribe_set(NRF_RADIO, NRF_RADIO_TASK_RXEN, DPPI_CH_RXEN);

	nrf_timer_publish_set(SW_TIMER, nrf_timer_compare_event_get(CC_IDX_TXEN),
			      DPPI_CH_TXEN);
	nrf_radio_subscribe_set(NRF_RADIO, NRF_RADIO_TASK_TXEN, DPPI_CH_TXEN);

	nrf_dppi_channels_enable(SW_DPPIC, BIT(DPPI_CH_TIMER_CLEAR));
	nrf_dppi_channels_disable(SW_DPPIC, BIT(DPPI_CH_RXEN) | BIT(DPPI_CH_TXEN));

	printk("DPPI configured (PHYEND→CLEAR, CC→RXEN/TXEN)\n");
}

void radio_freq_chan_set(uint32_t freq_off)
{
	NRF_RADIO->FREQUENCY = freq_off;
}

void radio_whiten_iv_set(uint8_t iv)
{
	NRF_RADIO->DATAWHITE = RADIO_DATAWHITE_ResetValue | iv;
	NRF_RADIO->PCNF1 |= RADIO_PCNF1_WHITEEN_Msk;
}

void radio_pkt_tx_set(void *pkt)
{
	NRF_RADIO->PACKETPTR = (uint32_t)pkt;
}

void radio_pkt_rx_set(void *pkt)
{
	NRF_RADIO->PACKETPTR = (uint32_t)pkt;
}

void radio_status_reset(void)
{
	NRF_RADIO->EVENTS_READY = 0;
	NRF_RADIO->EVENTS_ADDRESS = 0;
	NRF_RADIO->EVENTS_END = 0;
	NRF_RADIO->EVENTS_PHYEND = 0;
	NRF_RADIO->EVENTS_DISABLED = 0;
}

bool radio_crc_is_valid(void)
{
	return (NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) ==
	       (RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos);
}

bool radio_is_done(void)
{
	return NRF_RADIO->EVENTS_PHYEND != 0;
}

void sw_timer_cc_set(uint8_t cc_idx, uint32_t us)
{
	nrf_timer_cc_set(SW_TIMER, (nrf_timer_cc_channel_t)cc_idx, us);
}

void sw_timer_start(void)
{
	nrf_timer_task_trigger(SW_TIMER, NRF_TIMER_TASK_CLEAR);
	nrf_timer_task_trigger(SW_TIMER, NRF_TIMER_TASK_START);
}

void sw_timer_clear_compare(uint8_t cc_idx)
{
	nrf_timer_event_clear(SW_TIMER, nrf_timer_compare_event_get(cc_idx));
	(void)nrf_timer_event_check(SW_TIMER, nrf_timer_compare_event_get(cc_idx));
}

void dppi_enable_rxen(void)
{
	nrf_dppi_channels_enable(SW_DPPIC, BIT(DPPI_CH_RXEN));
}

void dppi_enable_txen(void)
{
	nrf_dppi_channels_enable(SW_DPPIC, BIT(DPPI_CH_TXEN));
}

void dppi_disable_switch(void)
{
	nrf_dppi_channels_disable(SW_DPPIC, BIT(DPPI_CH_RXEN) | BIT(DPPI_CH_TXEN));
}

bool radio_event_end_check(void)
{
	return NRF_RADIO->EVENTS_PHYEND != 0;
}

void radio_event_end_clear(void)
{
	NRF_RADIO->EVENTS_PHYEND = 0;
	NRF_RADIO->EVENTS_END = 0;
}

void data_chan_set(uint8_t chan)
{
	uint32_t freq;

	if (chan <= 10) {
		freq = 4 + (chan * 2U);
	} else if (chan <= 36) {
		freq = 28 + ((chan - 11) * 2U);
	} else {
		return;
	}

	radio_freq_chan_set(freq);
	radio_whiten_iv_set(chan);
}

uint32_t radio_tmr_start(uint8_t trx, uint32_t ticks_start, uint32_t remainder)
{
	ARG_UNUSED(remainder);

	while (((ticks_start - ticker_now()) & HAL_TICKER_CNTR_MASK) <=
	       (HAL_TICKER_CNTR_MASK >> 1)) {
	}

	tmr_ready_latched = false;
	tmr_aa_latched = false;
	tmr_ready_us = 0;
	tmr_aa_us = 0;

	if (trx == 0) {
		NRF_RADIO->TASKS_RXEN = 1;
	} else {
		NRF_RADIO->TASKS_TXEN = 1;
	}

	return 0;
}

void radio_tmr_status_reset(void)
{
	hcto_active = false;
	tmr_ready_latched = false;
	tmr_aa_latched = false;
}

void radio_tmr_aa_capture(void)
{
}

void radio_tmr_aa_save(uint32_t aa)
{
	ARG_UNUSED(aa);
}

void radio_tmr_end_capture(void)
{
}

void radio_tmr_hcto_configure(uint32_t hcto_us)
{
	hcto_deadline = ticker_now() + hcto_us;
	hcto_active = true;
}

void radio_tmr_stamp_poll(void)
{
	if (!tmr_ready_latched && NRF_RADIO->EVENTS_READY) {
		tmr_ready_us = ticker_now();
		tmr_ready_latched = true;
	}
	if (!tmr_aa_latched && NRF_RADIO->EVENTS_ADDRESS) {
		tmr_aa_us = ticker_now();
		tmr_aa_latched = true;
	}
}

bool radio_hcto_expired(void)
{
	if (!hcto_active) {
		return false;
	}
	return (int32_t)(ticker_now() - hcto_deadline) >= 0;
}

uint32_t radio_tmr_aa_get(void)
{
	return tmr_aa_us;
}

uint32_t radio_tmr_ready_get(void)
{
	return tmr_ready_us;
}

uint32_t radio_rx_ready_delay_get(uint8_t phy, uint8_t flags)
{
	ARG_UNUSED(phy);
	ARG_UNUSED(flags);
	return RX_RAMP_US;
}

uint32_t radio_rx_chain_delay_get(uint8_t phy, uint8_t flags)
{
	ARG_UNUSED(phy);
	ARG_UNUSED(flags);
	return RX_CHAIN_DELAY_1M_US;
}
