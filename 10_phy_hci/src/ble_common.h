/*
 * BLE PHY HCI Demo — 公共类型、常量与全局变量声明
 *
 * 相比 09_phy_ll_procedures 的变化:
 *   1. 新增 HCI 相关常量 (Opcode, Event Code, Packet Indicator)
 *   2. 新增 HCI RX/TX 缓冲区大小定义
 *   3. 新增 HCI 状态机状态枚举
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

/*
 * nRF54L15 没有 nRF52 的 PPI / RTC0，也不使用 Zephyr LL 内部头文件。
 * 硬件路径与 02_active_scan 相同: TIMER10 + DPPIC10 做软件 TIFS，
 * TIMER20 (1 MHz) 做连接时基。
 */

#define BDADDR_SIZE                 6
#define PDU_AC_LEG_DATA_SIZE_MAX    31
#define PDU_AC_LEG_PAYLOAD_SIZE_MAX 37
#define PDU_DC_PAYLOAD_SIZE_MIN     27
#define PDU_AC_ACCESS_ADDR          0x8e89bed6
#define PDU_AC_CRC_IV               0x555555
#define PDU_CRC_POLYNOMIAL          0x00065b
#define PDU_CHANNEL_MAP_SIZE        5

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

struct pdu_adv_connect_ind {
	uint8_t init_addr[BDADDR_SIZE];
	uint8_t adv_addr[BDADDR_SIZE];
	struct {
		uint8_t  access_addr[4];
		uint8_t  crc_init[3];
		uint8_t  win_size;
		uint16_t win_offset;
		uint16_t interval;
		uint16_t latency;
		uint16_t timeout;
		uint8_t  chan_map[PDU_CHANNEL_MAP_SIZE];
		uint8_t  hop:5;
		uint8_t  sca:3;
	} __packed;
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
		struct pdu_adv_connect_ind connect_ind;
	} __packed;
} __packed;

enum pdu_data_llid {
	PDU_DATA_LLID_RESV = 0x00,
	PDU_DATA_LLID_DATA_CONTINUE = 0x01,
	PDU_DATA_LLID_DATA_START = 0x02,
	PDU_DATA_LLID_CTRL = 0x03,
};

enum pdu_data_llctrl_type {
	PDU_DATA_LLCTRL_TYPE_CONN_UPDATE_IND = 0x00,
	PDU_DATA_LLCTRL_TYPE_CHAN_MAP_IND = 0x01,
	PDU_DATA_LLCTRL_TYPE_TERMINATE_IND = 0x02,
	PDU_DATA_LLCTRL_TYPE_UNKNOWN_RSP = 0x07,
	PDU_DATA_LLCTRL_TYPE_FEATURE_REQ = 0x08,
	PDU_DATA_LLCTRL_TYPE_FEATURE_RSP = 0x09,
	PDU_DATA_LLCTRL_TYPE_VERSION_IND = 0x0C,
	PDU_DATA_LLCTRL_TYPE_PER_INIT_FEAT_XCHG = 0x0E,
	PDU_DATA_LLCTRL_TYPE_PING_REQ = 0x12,
	PDU_DATA_LLCTRL_TYPE_PING_RSP = 0x13,
	PDU_DATA_LLCTRL_TYPE_LENGTH_REQ = 0x14,
	PDU_DATA_LLCTRL_TYPE_LENGTH_RSP = 0x15,
};

struct pdu_data_llctrl_conn_update_ind {
	uint8_t  win_size;
	uint16_t win_offset;
	uint16_t interval;
	uint16_t latency;
	uint16_t timeout;
	uint16_t instant;
} __packed;

struct pdu_data_llctrl_chan_map_ind {
	uint8_t  chm[PDU_CHANNEL_MAP_SIZE];
	uint16_t instant;
} __packed;

struct pdu_data_llctrl_unknown_rsp {
	uint8_t type;
} __packed;

struct pdu_data_llctrl_feature_rsp {
	uint8_t features[8];
} __packed;

struct pdu_data_llctrl_version_ind {
	uint8_t  version_number;
	uint16_t company_id;
	uint16_t sub_version_number;
} __packed;

struct pdu_data_llctrl_length_rsp {
	uint16_t max_rx_octets;
	uint16_t max_rx_time;
	uint16_t max_tx_octets;
	uint16_t max_tx_time;
} __packed;

struct pdu_data_llctrl {
	uint8_t opcode;
	union {
		struct pdu_data_llctrl_conn_update_ind conn_update_ind;
		struct pdu_data_llctrl_chan_map_ind chan_map_ind;
		struct pdu_data_llctrl_unknown_rsp unknown_rsp;
		struct pdu_data_llctrl_feature_rsp feature_rsp;
		struct pdu_data_llctrl_version_ind version_ind;
		struct pdu_data_llctrl_length_rsp length_rsp;
	} __packed;
} __packed;

struct pdu_data {
	uint8_t ll_id:2;
	uint8_t nesn:1;
	uint8_t sn:1;
	uint8_t md:1;
	uint8_t rfu:3;
	uint8_t len;
	union {
		struct pdu_data_llctrl llctrl;
		uint8_t lldata[PDU_DC_PAYLOAD_SIZE_MIN];
	} __packed;
} __packed;

/* TIMER20 @ 1 MHz: 1 tick = 1 us, 32-bit */
#define HAL_TICKER_CNTR_CLK_UNIT_FSEC 1000000000UL
#define HAL_TICKER_CNTR_MASK          0xFFFFFFFFUL
#define HAL_TICKER_FSEC_PER_USEC      1000000000UL
#define HAL_TICKER_PSEC_PER_USEC      1000000UL
#define HAL_TICKER_FSEC_PER_PSEC      1000UL

#define HAL_TICKER_US_TO_TICKS(x) (((uint32_t)(x)) & HAL_TICKER_CNTR_MASK)
#define HAL_TICKER_TICKS_TO_US(x) ((uint32_t)(x))
#define HAL_TICKER_REMAINDER(x)   (0U)

/*===========================================================================
 * 配置参数
 *===========================================================================*/

#define ADV_INTERVAL_MS  100
#define T_IFS_US         150
#define ADV_RX_TIMEOUT_US  600

#define WIN_DELAY_LEGACY            1250
#define CONN_INT_UNIT_US            1250
#define EVENT_JITTER_US             16
#define EVENT_TICKER_RES_MARGIN_US  31
#define ADDR_US_1M                  40
#define RTC0_CMP_OFFSET_MIN         3

#define ADV_NAME         "HCI-BLE"

#define RX_RAMP_US       40
#define TX_RAMP_US       40

#define RX_CHAIN_DELAY_1M_US  10
#define TX_CHAIN_DELAY_US      1

/* RADIO 同域: TIMER10 + DPPIC10 */
#define DPPI_CH_TIMER_CLEAR  0
#define DPPI_CH_RXEN         1
#define DPPI_CH_TXEN         2

#define CC_IDX_RXEN  0
#define CC_IDX_TXEN  1

#define LOCAL_SCA_PPM 50

/* L2CAP */
#define L2CAP_CID_ATT       0x0004
#define L2CAP_CID_SIG       0x0005
#define L2CAP_CID_SMP       0x0006
#define L2CAP_HDR_SIZE      4
#define L2CAP_MAX_PAYLOAD   512
#define LL_DATA_MTU_DEFAULT 27

/*===========================================================================
 * HCI 常量
 *===========================================================================*/

/* H4 Packet Indicators */
#define HCI_H4_CMD          0x01
#define HCI_H4_ACL          0x02
#define HCI_H4_EVT          0x04

/* HCI Event Codes */
#define HCI_EVT_DISCONNECT_COMPLETE  0x05
#define HCI_EVT_CMD_COMPLETE         0x0E
#define HCI_EVT_CMD_STATUS           0x0F
#define HCI_EVT_NUM_COMPLETED_PKTS   0x13
#define HCI_EVT_LE_META              0x3E

/* LE Meta Subevent Codes */
#define HCI_LE_SUBEVENT_CONN_COMPLETE  0x01

/* HCI Command OpCodes (OGF | OCF) */
#define HCI_OP_RESET                    0x0C03
#define HCI_OP_SET_EVENT_MASK           0x0C01
#define HCI_OP_READ_LOCAL_VERSION       0x1001
#define HCI_OP_READ_LOCAL_COMMANDS      0x1002
#define HCI_OP_READ_LOCAL_FEATURES      0x1003
#define HCI_OP_READ_BD_ADDR             0x1009
#define HCI_OP_LE_SET_EVENT_MASK        0x2001
#define HCI_OP_LE_READ_BUFFER_SIZE      0x2002
#define HCI_OP_LE_READ_LOCAL_FEATURES   0x2003
#define HCI_OP_LE_SET_ADV_PARAM         0x2006
#define HCI_OP_LE_READ_ADV_TX_POWER     0x2007
#define HCI_OP_LE_SET_ADV_DATA          0x2008
#define HCI_OP_LE_SET_ADV_ENABLE        0x200A
#define HCI_OP_LE_SET_SCAN_PARAM        0x200B
#define HCI_OP_LE_SET_SCAN_ENABLE       0x200C
#define HCI_OP_LE_READ_WHITE_LIST_SIZE  0x200F
#define HCI_OP_LE_READ_SUPP_STATES      0x201C

/* ACL */
#define HCI_ACL_HDR_SIZE   4
#define HCI_ACL_MAX_DATA   256  /* 简化上限 */

/* 连接句柄 (本 Demo 只支持 1 条连接, 固定句柄 0x0000) */
#define HCI_CONN_HANDLE   0x0000

/*===========================================================================
 * 连接参数结构体
 *===========================================================================*/
struct conn_param {
	uint8_t  access_addr[4];
	uint8_t  crc_init[3];
	uint8_t  win_size;
	uint16_t win_offset;
	uint16_t interval;
	uint16_t latency;
	uint16_t timeout;
	uint8_t  chan_map[PDU_CHANNEL_MAP_SIZE];
	uint8_t  hop;
	uint8_t  sca;
	uint8_t  chan_count;
	uint8_t  peer_addr[BDADDR_SIZE];
};

struct anchor_tracker {
	uint32_t initial_anchor_rtc;
	uint32_t initial_anchor_us;
	uint32_t current_anchor_rtc;
	uint32_t last_rx_anchor_rtc;
	uint32_t ww_current_us;
	uint32_t ww_periodic_us;
	uint32_t ww_max_us;
	uint32_t win_size_event_us;
	uint32_t anchor_update_count;
	uint32_t consecutive_misses;
	uint32_t max_consecutive_misses;
};

struct latency_tracker {
	uint16_t latency_max;
	uint16_t latency_used;
	uint32_t events_skipped;
	uint32_t events_listened;
};

struct ll_proc_conn_update {
	bool     pending;
	uint16_t instant;
	uint8_t  win_size;
	uint16_t win_offset;
	uint16_t interval;
	uint16_t latency;
	uint16_t timeout;
};

struct ll_proc_chan_map_update {
	bool     pending;
	uint16_t instant;
	uint8_t  chan_map[PDU_CHANNEL_MAP_SIZE];
};

/*===========================================================================
 * L2CAP
 *===========================================================================*/
struct l2cap_reassembly {
	uint8_t  buf[L2CAP_MAX_PAYLOAD + L2CAP_HDR_SIZE];
	uint16_t expected_len;
	uint16_t received_len;
	bool     in_progress;
};

#define TX_QUEUE_SIZE  4

struct tx_frag {
	uint8_t  data[LL_DATA_MTU_DEFAULT];
	uint8_t  len;
	uint8_t  llid;
};

struct tx_queue {
	struct tx_frag frags[TX_QUEUE_SIZE];
	uint8_t head;
	uint8_t tail;
	uint8_t count;
};

/*===========================================================================
 * 全局变量声明
 *===========================================================================*/

extern const uint8_t adv_addr[BDADDR_SIZE];
extern uint8_t adv_data[];
extern uint8_t adv_data_len;
#define ADV_DATA_LEN_MAX 31

extern struct conn_param conn_params;

extern uint8_t tx_sn;
extern uint8_t rx_nesn;
extern uint16_t conn_event_counter;
extern uint8_t last_unmapped_chan;
extern volatile bool conn_terminated;

extern struct pdu_data tx_pdu_buf;
extern bool tx_pdu_pending;

extern struct pdu_adv pdu_adv_ind;
extern struct pdu_adv pdu_adv_rx;
extern struct pdu_data pdu_data_rx;

extern uint32_t connect_end_rtc;
extern const uint16_t sca_ppm_table[];
extern struct anchor_tracker anchor;
extern struct latency_tracker lat;

extern struct ll_proc_conn_update proc_conn_update;
extern struct ll_proc_chan_map_update proc_chan_map;

extern volatile uint32_t adv_event_count;
extern volatile uint32_t conn_event_rx_ok;
extern volatile uint32_t conn_event_rx_timeout;
extern volatile uint32_t conn_event_rx_crc_err;

extern uint32_t last_rx_aa_us;
extern uint32_t last_rx_ready_us;

extern struct l2cap_reassembly l2cap_reasm;
extern struct tx_queue data_tx_q;
extern uint32_t l2cap_rx_count;
extern uint32_t l2cap_tx_count;

/* HCI 状态 */
extern volatile bool hci_adv_enabled;
extern volatile bool hci_connected;

#endif /* BLE_COMMON_H */
