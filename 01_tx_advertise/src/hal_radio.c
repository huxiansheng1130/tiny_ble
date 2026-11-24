/*
 * BLE PHY Raw TX Demo — HAL (nRF54L15)
 *
 * 直接写 RADIO 寄存器，不依赖 Zephyr Controller HAL。
 * nRF54L15 用 PHYEND→DISABLE，没有 nRF52 的 END_DISABLE shortcut。
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hal_radio.h"

#include <hal/nrf_radio.h>

void hfclk_start(void)
{
	NRF_CLOCK->EVENTS_XOSTARTED = 0;
	NRF_CLOCK->TASKS_XOSTART = 1;
	while (NRF_CLOCK->EVENTS_XOSTARTED == 0) {
	}
	printk("HFCLK (HFXO) started\n");
}

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

	printk("Radio configured: BLE 1M (nRF54L15)\n");
}

void send_on_channel(uint32_t channel)
{
	uint32_t freq;

	switch (channel) {
	case 37: freq = 2;  break;
	case 38: freq = 26; break;
	case 39: freq = 80; break;
	default: return;
	}

	NRF_RADIO->FREQUENCY = freq;
	NRF_RADIO->DATAWHITE = RADIO_DATAWHITE_ResetValue | channel;
	NRF_RADIO->PCNF1 |= RADIO_PCNF1_WHITEEN_Msk;
	NRF_RADIO->PACKETPTR = (uint32_t)&pdu;

	NRF_RADIO->EVENTS_READY = 0;
	NRF_RADIO->EVENTS_ADDRESS = 0;
	NRF_RADIO->EVENTS_END = 0;
	NRF_RADIO->EVENTS_PHYEND = 0;
	NRF_RADIO->EVENTS_DISABLED = 0;

	NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk |
			    RADIO_SHORTS_PHYEND_DISABLE_Msk;

	NRF_RADIO->TASKS_TXEN = 1;

	while (NRF_RADIO->EVENTS_DISABLED == 0) {
	}

	NRF_RADIO->SHORTS = 0;
}
