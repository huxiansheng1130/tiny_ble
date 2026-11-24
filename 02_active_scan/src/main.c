/*
 * BLE PHY 软件 TIFS (SW TIFS) Demo — nRF54L15
 *
 * 使用 TIMER10 + DPPIC10 硬件互联实现软件 TIFS 切换
 * (对应 nRF52 上 TIMER1 + PPI 的教学路径)。
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ble_common.h"
#include "hal_radio.h"
#include "ll_pdu.h"
#include "adv.h"

/* MSB=0xC0 → Random Static Address（与 tiny_ble 一致） */
const uint8_t adv_addr[BDADDR_SIZE] = {
	0x11, 0x22, 0x33, 0x44, 0x55, 0xC0
};

struct pdu_adv pdu_adv_ind __aligned(4);
struct pdu_adv pdu_scan_rsp __aligned(4);
struct pdu_adv pdu_rx_buf __aligned(4);

volatile uint32_t adv_event_count;
volatile uint32_t scan_req_count;
volatile uint32_t scan_rsp_count;

int main(void)
{
	printk("\n=== BLE PHY SW TIFS Demo (nRF54L15) ===\n");
	printk("广播名: %s  (搜这个)\n", ADV_NAME);
	printk("使用 TIMER10 + DPPIC10 实现软件 TIFS 定时\n\n");

	hfclk_start();
	radio_configure();
	sw_switch_timer_configure();
	dppi_configure();
	build_adv_ind_pdu();
	build_scan_rsp_pdu();

	adv_init();
	adv_start();

	printk("Advertising started (SW TIFS: TIMER10 + DPPI)\n\n");

	while (1) {
		k_sleep(K_SECONDS(5));
		printk("[Main] alive — adv: %u, scan_req: %u, scan_rsp: %u\n",
		       adv_event_count, scan_req_count, scan_rsp_count);
	}

	return 0;
}
