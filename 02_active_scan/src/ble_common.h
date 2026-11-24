/*
 * BLE PHY 软件 TIFS Demo — 公共类型、常量与全局变量声明
 * 目标平台: nRF54L15 (TIMER10 + DPPIC10)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_COMMON_H
#define BLE_COMMON_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/*===========================================================================
 * Legacy Advertising PDU (精简自 Zephyr controller pdu.h)
 *===========================================================================*/

#define BDADDR_SIZE                 6
#define PDU_AC_LEG_DATA_SIZE_MAX    31
#define PDU_AC_LEG_PAYLOAD_SIZE_MAX 37
#define PDU_AC_ACCESS_ADDR          0x8e89bed6
#define PDU_AC_CRC_IV               0x555555
#define PDU_CRC_POLYNOMIAL          0x00065b

enum pdu_adv_type {
	PDU_ADV_TYPE_ADV_IND = 0x00,
	PDU_ADV_TYPE_DIRECT_IND = 0x01,
	PDU_ADV_TYPE_NONCONN_IND = 0x02,
	PDU_ADV_TYPE_SCAN_REQ = 0x03,
	PDU_ADV_TYPE_SCAN_RSP = 0x04,
	PDU_ADV_TYPE_CONNECT_IND = 0x05,
	PDU_ADV_TYPE_SCAN_IND = 0x06,
};

struct pdu_adv_adv_ind {
	uint8_t addr[BDADDR_SIZE];
	uint8_t data[PDU_AC_LEG_DATA_SIZE_MAX];
} __packed;

struct pdu_adv_scan_rsp {
	uint8_t addr[BDADDR_SIZE];
	uint8_t data[PDU_AC_LEG_DATA_SIZE_MAX];
} __packed;

struct pdu_adv_scan_req {
	uint8_t scan_addr[BDADDR_SIZE];
	uint8_t adv_addr[BDADDR_SIZE];
} __packed;

struct pdu_adv {
	uint8_t type:4;
	uint8_t rfu:1;
	uint8_t chan_sel:1;
	uint8_t tx_addr:1;
	uint8_t rx_addr:1;
	uint8_t len;
	union {
		uint8_t payload[PDU_AC_LEG_PAYLOAD_SIZE_MAX];
		struct pdu_adv_adv_ind adv_ind;
		struct pdu_adv_scan_req scan_req;
		struct pdu_adv_scan_rsp scan_rsp;
	} __packed;
} __packed;

/*===========================================================================
 * 配置参数
 *===========================================================================*/

#define ADV_INTERVAL_MS  100

/* T_IFS: BLE 规范帧间间隔 = 150 μs */
#define T_IFS_US         150

/* nRF54L Fast Ramp-Up ≈ 40 μs */
#define RX_RAMP_US       40
#define TX_RAMP_US       40

/* RX chain delay (μs): 空口最后 bit → END/PHYEND ≈ 10 μs (1M PHY) */
#define RX_CHAIN_DELAY_US  10

#define RX_TIMEOUT_US    500
#define ADV_NAME         "ZephyrRaw"

/*===========================================================================
 * DPPI / TIMER 通道分配 (RADIO 同域: TIMER10 + DPPIC10)
 *===========================================================================*/
#define DPPI_CH_TIMER_CLEAR  0   /* RADIO PHYEND → TIMER CLEAR */
#define DPPI_CH_RXEN         1   /* TIMER CC[0] → RADIO RXEN */
#define DPPI_CH_TXEN         2   /* TIMER CC[1] → RADIO TXEN */

#define CC_IDX_RXEN  0
#define CC_IDX_TXEN  1

/*===========================================================================
 * 全局变量声明 (定义在 main.c)
 *===========================================================================*/

extern const uint8_t adv_addr[BDADDR_SIZE];

extern struct pdu_adv pdu_adv_ind;
extern struct pdu_adv pdu_scan_rsp;
extern struct pdu_adv pdu_rx_buf;

extern volatile uint32_t adv_event_count;
extern volatile uint32_t scan_req_count;
extern volatile uint32_t scan_rsp_count;

#endif /* BLE_COMMON_H */
