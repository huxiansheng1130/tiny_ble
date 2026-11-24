/*
 * BLE PHY Connection Loop Demo — 公共类型、常量与全局变量声明
 *
 * 相比 04_phy_first_anchor 新增:
 *   - anchor_tracker 扩展: consecutive_misses, anchor_update_count 等
 *   - conn_event_rx_crc_err 统计
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

/* 广播间隔 (ms) */
#define ADV_INTERVAL_MS  100

/* T_IFS = 150 μs */
#define T_IFS_US         150

/* ADV RX 超时 (μs) */
#define ADV_RX_TIMEOUT_US  600

/* BLE 定时常量 */
#define WIN_DELAY_LEGACY            1250  /* transmitWindowDelay (μs) */
#define CONN_INT_UNIT_US            1250  /* connInterval unit (1.25ms) */
#define EVENT_JITTER_US             16    /* Active clock jitter margin (μs) */
#define EVENT_TICKER_RES_MARGIN_US  31    /* RTC tick resolution ~30.52μs ceil */
#define ADDR_US_1M                  40    /* Preamble(8)+AA(32) for 1M PHY */
#define RTC0_CMP_OFFSET_MIN         3     /* NRF_RTC0 CC 最小比较偏移 */

#define ADV_NAME         "ConLoop"

/* TX/RX ramp-up 延迟 (μs): fast ramp-up ≈ 40 μs */
#define RX_RAMP_US       40
#define TX_RAMP_US       40

/* Chain delay: RADIO END 事件相对于实际空中包结束的延迟
 * nRF52832 1M PHY: RX chain delay = 9.4μs (ceil→10), TX chain delay = 0.6μs (ceil→1) */
#define RX_CHAIN_DELAY_1M_US  10
#define TX_CHAIN_DELAY_US      1

/*===========================================================================
 * PPI 通道分配 (SW TIFS)
 *===========================================================================*/
#define DPPI_CH_TIMER_CLEAR  0   /* RADIO PHYEND -> TIMER CLEAR */
#define DPPI_CH_RXEN         1   /* TIMER CC[0] -> RADIO RXEN */
#define DPPI_CH_TXEN         2   /* TIMER CC[1] -> RADIO TXEN */

#define CC_IDX_RXEN  0   /* Timer CC[0]: TX→RX 转换 */
#define CC_IDX_TXEN  1   /* Timer CC[1]: RX→TX 转换 */

#define LOCAL_SCA_PPM 50

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

/*===========================================================================
 * 锚点追踪结构体
 *
 * 相比 04: 增加了 consecutive_misses 和 anchor_update_count
 * (为 supervision timeout 和统计用)
 *===========================================================================*/
struct anchor_tracker {
	uint32_t initial_anchor_rtc;     /* 首次计算出的锚点 RTC tick */
	uint32_t initial_anchor_us;      /* 首次锚点的 μs 偏移量 */
	uint32_t current_anchor_rtc;     /* 当前锚点 RTC tick */
	uint32_t consecutive_misses;     /* 连续 RX 失败次数 */
	uint32_t max_consecutive_misses; /* 历史最大连续失败 */
	uint32_t anchor_update_count;    /* 锚点更新次数 (= 成功 RX 次数) */
};

/*===========================================================================
 * 全局变量声明 (定义在 main.c)
 *===========================================================================*/

extern const uint8_t adv_addr[BDADDR_SIZE];
extern const uint8_t adv_data[];
#define ADV_DATA_LEN 12  /* sizeof(adv_data): 3 (flags) + 9 (name "ConLoop") */

extern struct conn_param conn_params;

/* 连接态变量 */
extern uint8_t tx_sn;
extern uint8_t rx_nesn;
extern uint16_t conn_event_counter;
extern uint8_t last_unmapped_chan;

extern struct pdu_data tx_pdu_buf;

/* PDU 缓冲区 */
extern struct pdu_adv pdu_adv_ind;
extern struct pdu_adv pdu_adv_rx;
extern struct pdu_data pdu_data_rx;

/* CONNECT_IND 接收时刻 */
extern uint32_t connect_end_rtc;

/* SCA PPM 映射表 */
extern const uint16_t sca_ppm_table[];

/* 锚点追踪 */
extern struct anchor_tracker anchor;

/* 统计 */
extern volatile uint32_t adv_event_count;
extern volatile uint32_t conn_event_rx_ok;
extern volatile uint32_t conn_event_rx_timeout;
extern volatile uint32_t conn_event_rx_crc_err;

#endif /* BLE_COMMON_H */
