/* SPDX-License-Identifier: MIT */
/*
 * BThome V2 library for Zephyr
 *
 * Specification: https://bthome.io/format/
 * Adapted from:  https://github.com/the78mole/BThomeV2-nRF52-Zephyr
 * Additions:     bthome_v2_add_distance_mm() for ultrasonic ranging
 */

#ifndef BTHOME_V2_H_
#define BTHOME_V2_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <zephyr/bluetooth/bluetooth.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * BThome V2 constants
 * -------------------------------------------------------------------------*/

#define BTHOME_V2_UUID              0xFCD2U
#define BTHOME_V2_UUID_LE_B0        0xD2U
#define BTHOME_V2_UUID_LE_B1        0xFCU

#define BTHOME_V2_DEV_INFO_ENCRYPT  0x01U
#define BTHOME_V2_DEV_INFO_TRIGGER  0x04U
#define BTHOME_V2_DEV_INFO_VERSION  0x40U

#define BTHOME_V2_SVC_DATA_MAX_LEN  26U
#define BTHOME_V2_MAX_MEASUREMENTS  12U
#define BTHOME_V2_MAX_VALUE_LEN     4U

/* ---------------------------------------------------------------------------
 * Object IDs — numeric sensors
 * -------------------------------------------------------------------------*/
#define BTHOME_OBJ_PACKET_ID        0x00U
#define BTHOME_OBJ_BATTERY          0x01U
#define BTHOME_OBJ_TEMPERATURE      0x02U  /* sint16, 0.01 °C */
#define BTHOME_OBJ_HUMIDITY         0x03U  /* uint16, 0.01 % */
#define BTHOME_OBJ_PRESSURE         0x04U  /* uint24, 0.01 hPa */
#define BTHOME_OBJ_ILLUMINANCE      0x05U  /* uint24, 0.01 lx */
#define BTHOME_OBJ_MASS_KG          0x06U
#define BTHOME_OBJ_MASS_LB          0x07U
#define BTHOME_OBJ_DEW_POINT        0x08U  /* sint16, 0.01 °C */
#define BTHOME_OBJ_COUNT_UI8        0x09U
#define BTHOME_OBJ_ENERGY           0x0AU  /* uint24, 0.001 kWh */
#define BTHOME_OBJ_POWER            0x0BU  /* uint24, 0.01 W */
#define BTHOME_OBJ_VOLTAGE          0x0CU  /* uint16, 0.001 V */
#define BTHOME_OBJ_PM2_5            0x0DU
#define BTHOME_OBJ_PM10             0x0EU
#define BTHOME_OBJ_GENERIC_BOOL     0x0FU
#define BTHOME_OBJ_POWER_BIN        0x10U
#define BTHOME_OBJ_OPENING_BIN      0x11U
#define BTHOME_OBJ_CO2              0x12U
#define BTHOME_OBJ_TVOC             0x13U
#define BTHOME_OBJ_MOISTURE         0x14U  /* uint16, 0.01 % */

/* ---------------------------------------------------------------------------
 * Object IDs — binary sensors
 * -------------------------------------------------------------------------*/
#define BTHOME_OBJ_BATTERY_LOW      0x15U
#define BTHOME_OBJ_BATTERY_CHARGING 0x16U
#define BTHOME_OBJ_CO               0x17U
#define BTHOME_OBJ_COLD             0x18U
#define BTHOME_OBJ_CONNECTIVITY     0x19U
#define BTHOME_OBJ_DOOR             0x1AU
#define BTHOME_OBJ_GARAGE_DOOR      0x1BU
#define BTHOME_OBJ_GAS              0x1CU
#define BTHOME_OBJ_HEAT             0x1DU
#define BTHOME_OBJ_LIGHT            0x1EU
#define BTHOME_OBJ_LOCK             0x1FU
#define BTHOME_OBJ_MOISTURE_BIN     0x20U
#define BTHOME_OBJ_MOTION           0x21U
#define BTHOME_OBJ_MOVING           0x22U
#define BTHOME_OBJ_OCCUPANCY        0x23U
#define BTHOME_OBJ_PLUG             0x24U
#define BTHOME_OBJ_PRESENCE         0x25U
#define BTHOME_OBJ_PROBLEM          0x26U
#define BTHOME_OBJ_RUNNING          0x27U
#define BTHOME_OBJ_SAFETY           0x28U
#define BTHOME_OBJ_SMOKE            0x29U
#define BTHOME_OBJ_SOUND            0x2AU
#define BTHOME_OBJ_TAMPER           0x2BU
#define BTHOME_OBJ_VIBRATION        0x2CU
#define BTHOME_OBJ_WINDOW           0x2DU
#define BTHOME_OBJ_HUMIDITY_UI8     0x2EU
#define BTHOME_OBJ_MOISTURE_UI8     0x2FU

/* ---------------------------------------------------------------------------
 * Object IDs — events
 * -------------------------------------------------------------------------*/
#define BTHOME_OBJ_BUTTON           0x3AU
#define BTHOME_OBJ_DIMMER           0x3CU
#define BTHOME_OBJ_COUNT_UI16       0x3DU
#define BTHOME_OBJ_COUNT_UI32       0x3EU
#define BTHOME_OBJ_ROTATION         0x3FU  /* sint16, 0.1 ° */

/* ---------------------------------------------------------------------------
 * Object IDs — extended numeric sensors
 * -------------------------------------------------------------------------*/
#define BTHOME_OBJ_DISTANCE_MM      0x40U  /* uint16, mm */
#define BTHOME_OBJ_DISTANCE_M       0x41U  /* uint16, 0.1 m */
#define BTHOME_OBJ_DURATION         0x42U  /* uint24, 0.001 s */
#define BTHOME_OBJ_CURRENT          0x43U  /* uint16, 0.001 A */
#define BTHOME_OBJ_SPEED            0x44U  /* uint16, 0.01 m/s */
#define BTHOME_OBJ_TEMPERATURE_01   0x45U  /* sint16, 0.1 °C */
#define BTHOME_OBJ_UV_INDEX         0x46U  /* uint8, 0.1 */
#define BTHOME_OBJ_VOLUME_L_01      0x47U
#define BTHOME_OBJ_VOLUME_L         0x48U
#define BTHOME_OBJ_VOLUME_FLOW_RATE 0x49U
#define BTHOME_OBJ_VOLTAGE_01       0x4AU  /* uint16, 0.1 V */
#define BTHOME_OBJ_GAS_UI24         0x4BU
#define BTHOME_OBJ_GAS_UI32         0x4CU
#define BTHOME_OBJ_ENERGY_UI32      0x4DU
#define BTHOME_OBJ_VOLUME_UI32      0x4EU
#define BTHOME_OBJ_WATER            0x4FU
#define BTHOME_OBJ_TIMESTAMP        0x50U  /* uint32, Unix s */
#define BTHOME_OBJ_ACCELERATION     0x51U  /* uint16, 0.001 m/s² */
#define BTHOME_OBJ_GYROSCOPE        0x52U  /* uint16, 0.001 °/s */
#define BTHOME_OBJ_TEXT             0x53U
#define BTHOME_OBJ_RAW              0x54U
#define BTHOME_OBJ_VOLUME_STORAGE   0x55U
#define BTHOME_OBJ_CONDUCTIVITY     0x56U
#define BTHOME_OBJ_TEMPERATURE_1    0x57U  /* sint8, 1 °C */
#define BTHOME_OBJ_TEMPERATURE_035  0x58U
#define BTHOME_OBJ_COUNT_SI8        0x59U
#define BTHOME_OBJ_COUNT_SI16       0x5AU
#define BTHOME_OBJ_COUNT_SI32       0x5BU
#define BTHOME_OBJ_POWER_SI32       0x5CU
#define BTHOME_OBJ_CURRENT_SI16     0x5DU
#define BTHOME_OBJ_DIRECTION        0x5EU
#define BTHOME_OBJ_PRECIPITATION    0x5FU
#define BTHOME_OBJ_CHANNEL          0x60U
#define BTHOME_OBJ_ACCELERATION_AXIS 0x63U /* sint32, 0.000001 m/s² */

/* ---------------------------------------------------------------------------
 * Button / Dimmer event values
 * -------------------------------------------------------------------------*/
#define BTHOME_BTN_EVT_NONE              0x00U
#define BTHOME_BTN_EVT_PRESS             0x01U
#define BTHOME_BTN_EVT_DOUBLE_PRESS      0x02U
#define BTHOME_BTN_EVT_TRIPLE_PRESS      0x03U
#define BTHOME_BTN_EVT_LONG_PRESS        0x04U
#define BTHOME_BTN_EVT_LONG_DOUBLE_PRESS 0x05U
#define BTHOME_BTN_EVT_LONG_TRIPLE_PRESS 0x06U
#define BTHOME_BTN_EVT_HOLD_PRESS        0x08U

#define BTHOME_DIMMER_NONE               0x00U
#define BTHOME_DIMMER_ROTATE_LEFT        0x01U
#define BTHOME_DIMMER_ROTATE_RIGHT       0x02U

/* ---------------------------------------------------------------------------
 * Context
 * -------------------------------------------------------------------------*/

struct bthome_v2_meas {
	uint8_t obj_id;
	uint8_t data_len;
	uint8_t data[BTHOME_V2_MAX_VALUE_LEN];
};

struct bthome_v2_ctx {
	struct bthome_v2_meas meas[BTHOME_V2_MAX_MEASUREMENTS];
	uint8_t  meas_count;
	bool     encrypted;
	bool     trigger_based;
	uint32_t pkt_cnt;
	uint8_t  svc_data[BTHOME_V2_SVC_DATA_MAX_LEN];
	uint8_t  svc_data_len;
};

/* ---------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------*/
void bthome_v2_init(struct bthome_v2_ctx *ctx, bool encrypted, bool trigger_based);
void bthome_v2_clear(struct bthome_v2_ctx *ctx);

/* ---------------------------------------------------------------------------
 * Measurement add API
 * -------------------------------------------------------------------------*/
int bthome_v2_add_packet_id(struct bthome_v2_ctx *ctx, uint8_t id);
int bthome_v2_add_battery(struct bthome_v2_ctx *ctx, uint8_t percent);

/** Temperature, 0.01 °C resolution (e.g. 2350 = 23.50 °C) */
int bthome_v2_add_temperature(struct bthome_v2_ctx *ctx, int16_t temp_cdegc);

/** Temperature, 0.1 °C resolution (e.g. 235 = 23.5 °C) */
int bthome_v2_add_temperature_01(struct bthome_v2_ctx *ctx, int16_t temp_ddegc);

/** Humidity, 0.01 % resolution (e.g. 5500 = 55.00 %) */
int bthome_v2_add_humidity(struct bthome_v2_ctx *ctx, uint16_t humidity_cpct);

/** Pressure, 0.01 hPa (e.g. 101325 = 1013.25 hPa) */
int bthome_v2_add_pressure(struct bthome_v2_ctx *ctx, uint32_t pressure_chpa);

/** Illuminance, 0.01 lx */
int bthome_v2_add_illuminance(struct bthome_v2_ctx *ctx, uint32_t illuminance_clx);

/** CO2 in ppm */
int bthome_v2_add_co2(struct bthome_v2_ctx *ctx, uint16_t ppm);

/** TVOC in µg/m³ */
int bthome_v2_add_tvoc(struct bthome_v2_ctx *ctx, uint16_t ugm3);

/** Voltage, 0.001 V resolution (value in millivolts) */
int bthome_v2_add_voltage(struct bthome_v2_ctx *ctx, uint16_t millivolts);

/** Dew point, 0.01 °C */
int bthome_v2_add_dew_point(struct bthome_v2_ctx *ctx, int16_t temp_cdegc);

/** Distance in mm (uint16, OBJ 0x40) */
int bthome_v2_add_distance_mm(struct bthome_v2_ctx *ctx, uint16_t mm);

/** Acceleration magnitude, 0.001 m/s² */
int bthome_v2_add_acceleration(struct bthome_v2_ctx *ctx, uint16_t milli_ms2);

/** Single acceleration axis (sint32, 0.000001 m/s²) */
int bthome_v2_add_acceleration_axis(struct bthome_v2_ctx *ctx, int32_t micro_ms2);

/** Gyroscope, 0.001 °/s */
int bthome_v2_add_gyroscope(struct bthome_v2_ctx *ctx, uint16_t milli_degs);

/** Unix timestamp */
int bthome_v2_add_timestamp(struct bthome_v2_ctx *ctx, uint32_t unix_s);

/** Binary sensor state (any BTHOME_OBJ_* binary constant) */
int bthome_v2_add_binary(struct bthome_v2_ctx *ctx, uint8_t obj_id, bool active);

/** Button event */
int bthome_v2_add_button(struct bthome_v2_ctx *ctx, uint8_t event);

/** Dimmer event */
int bthome_v2_add_dimmer(struct bthome_v2_ctx *ctx, uint8_t direction, uint8_t steps);

/** Variable-length raw/text object */
int bthome_v2_add_raw(struct bthome_v2_ctx *ctx, uint8_t obj_id,
		      const uint8_t *data, uint8_t data_len);

/* ---------------------------------------------------------------------------
 * Encode + bt_data helper
 * -------------------------------------------------------------------------*/

/**
 * Sort measurements by OBJ_ID and encode into svc_data buffer.
 * Must be called before bthome_v2_get_bt_data().
 */
int bthome_v2_encode(struct bthome_v2_ctx *ctx);

/**
 * Populate a bt_data entry pointing at the encoded service data.
 * ctx->svc_data must remain valid for the lifetime of the advertisement.
 */
int bthome_v2_get_bt_data(const struct bthome_v2_ctx *ctx, struct bt_data *out_data);

#ifdef __cplusplus
}
#endif

#endif /* BTHOME_V2_H_ */
