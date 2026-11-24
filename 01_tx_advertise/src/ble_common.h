/*
 * BLE PHY Raw TX Demo — nRF54L15
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_COMMON_H
#define BLE_COMMON_H

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <soc.h>

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
	} __packed;
} __packed;

extern const uint8_t adv_addr[BDADDR_SIZE];
extern struct pdu_adv pdu;

#endif /* BLE_COMMON_H */
