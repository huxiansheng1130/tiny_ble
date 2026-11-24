/*
 * BLE PHY 软件 TIFS Demo — HAL
 *
 * 目标板: nRF54L15DK (TIMER10 + DPPIC10)
 * 若误选 nRF52 会给出明确编译错误。
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hal_radio.h"

#include <soc.h>
#include <hal/nrf_radio.h>
#include <hal/nrf_timer.h>
#include <hal/nrf_clock.h>
#include <hal/nrf_dppi.h>

#define SW_TIMER   NRF_TIMER10
#define SW_DPPIC   NRF_DPPIC10

/* HFXO=32MHz, PRESCALER=5 → 1MHz (1μs) */
#define SW_TIMER_PRESCALER  5

/*===========================================================================
 * 启动 HFXO —— 与 tiny_ble 相同
 *===========================================================================*/
void hfclk_start(void)
{
	NRF_CLOCK->EVENTS_XOSTARTED = 0;
	NRF_CLOCK->TASKS_XOSTART = 1;
	while (NRF_CLOCK->EVENTS_XOSTARTED == 0) {
	}
	printk("HFCLK (HFXO) started\n");
}

/*===========================================================================
 * 配置 RADIO —— 寄存器写法对齐 tiny_ble
 *===========================================================================*/
void radio_configure(void)
{
	NRF_RADIO->INTENCLR00 = 0xFFFFFFFFUL;
	NRF_RADIO->INTENCLR01 = 0xFFFFFFFFUL;
	NRF_RADIO->SHORTS = 0;

	NRF_RADIO->MODE = RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos;
	NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_0dBm;

	NRF_RADIO->PCNF0 = (8UL << RADIO_PCNF0_LFLEN_Pos) |
			   (1UL << RADIO_PCNF0_S0LEN_Pos) |
			   (RADIO_PCNF0_PLEN_8bit << RADIO_PCNF0_PLEN_Pos);

	NRF_RADIO->PCNF1 = ((uint32_t)PDU_AC_LEG_PAYLOAD_SIZE_MAX << RADIO_PCNF1_MAXLEN_Pos) |
			   (0UL << RADIO_PCNF1_STATLEN_Pos) |
			   (3UL << RADIO_PCNF1_BALEN_Pos) |
			   (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos);

	NRF_RADIO->TXADDRESS = 0;
	NRF_RADIO->RXADDRESSES =
		RADIO_RXADDRESSES_ADDR0_Enabled << RADIO_RXADDRESSES_ADDR0_Pos;
	NRF_RADIO->PREFIX0 = (PDU_AC_ACCESS_ADDR >> 24) & 0xFFUL;
	NRF_RADIO->BASE0 = (PDU_AC_ACCESS_ADDR & 0x00FFFFFFUL) << 8;

	NRF_RADIO->CRCCNF = (RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos) |
			    (RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos);
	NRF_RADIO->CRCPOLY = PDU_CRC_POLYNOMIAL;
	NRF_RADIO->CRCINIT = PDU_AC_CRC_IV;

	nrf_radio_fast_ramp_up_enable_set(NRF_RADIO, true);

	printk("Radio configured: BLE 1M (nRF54L15 / tiny_ble style)\n");
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

	printk("SW_SWITCH_TIMER (TIMER10) configured: 1MHz, 16-bit\n");
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

void radio_ensure_disabled(void)
{
	NRF_RADIO->SHORTS = 0;
	nrf_dppi_channels_disable(SW_DPPIC, BIT(DPPI_CH_RXEN) | BIT(DPPI_CH_TXEN));
	nrf_timer_task_trigger(SW_TIMER, NRF_TIMER_TASK_STOP);

	if (NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled) {
		NRF_RADIO->EVENTS_DISABLED = 0;
		NRF_RADIO->TASKS_DISABLE = 1;
		while (NRF_RADIO->EVENTS_DISABLED == 0) {
		}
	}
}

void radio_set_channel(uint8_t ble_channel)
{
	uint32_t freq_off;

	switch (ble_channel) {
	case 37: freq_off = 2;  break;
	case 38: freq_off = 26; break;
	case 39: freq_off = 80; break;
	default: return;
	}

	NRF_RADIO->FREQUENCY = freq_off;
	NRF_RADIO->DATAWHITE = RADIO_DATAWHITE_ResetValue | ble_channel;
	NRF_RADIO->PCNF1 |= RADIO_PCNF1_WHITEEN_Msk;
}

void radio_packetptr_set(void *pkt)
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

void radio_shorts_trx_set(void)
{
	NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk |
			    RADIO_SHORTS_PHYEND_DISABLE_Msk;
}

void radio_shorts_clear(void)
{
	NRF_RADIO->SHORTS = 0;
}

void radio_tx_enable(void)
{
	NRF_RADIO->TASKS_TXEN = 1;
}

bool radio_crc_ok(void)
{
	return (NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) ==
	       (RADIO_CRCSTATUS_CRCSTATUS_CRCOk << RADIO_CRCSTATUS_CRCSTATUS_Pos);
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

void sw_timer_stop(void)
{
	nrf_timer_task_trigger(SW_TIMER, NRF_TIMER_TASK_STOP);
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
}

bool radio_event_disabled_check(void)
{
	return NRF_RADIO->EVENTS_DISABLED != 0;
}

void radio_event_disabled_clear(void)
{
	NRF_RADIO->EVENTS_DISABLED = 0;
}

bool radio_event_address_check(void)
{
	return NRF_RADIO->EVENTS_ADDRESS != 0;
}

void radio_event_address_clear(void)
{
	NRF_RADIO->EVENTS_ADDRESS = 0;
}
