/*
 * 200-BThome-Ranging — HC-SR04 + BThome V2 BLE Advertising
 *
 * ===================================================================
 * Ablauf pro Messzyklus (nach Deep-Sleep-Wakeup oder erstem Start):
 *
 *   1. GPIO-Init: TRIG, ECHO, Power-Gate-Pins
 *   2. Sensor einschalten (Power-Gate HIGH)
 *   3. Bluetooth aktivieren (bt_enable, ~400-600 ms)
 *   4. Sensor-Startverzoegerung abwarten (Gesamt >= SENSOR_STARTUP_MS)
 *   5. HC-SR04 messen (TRIG 10 us, auf ECHO warten, Distanz berechnen)
 *   6. Sensor ausschalten (Power-Gate LOW + gpio_hold_en)
 *   7. BThome-Paket aufbauen (Packet-ID + Distanz in mm)
 *   8. BLE Non-Connectable Advertising starten (ADV_DURATION_MS)
 *   9. Advertising stoppen, bt_disable()
 *  10. Deep-Sleep fuer MEASURE_INTERVAL_MS (RTC-Timer-Wakeup)
 *
 * Power-Strategie:
 *   - HC-SR04 waehrend Sleep abgeschaltet (Power-Gate LOW + Hold)
 *   - TRIG-Pin ebenfalls LOW + Hold (verhindert float HIGH)
 *   - BLE waehrend Sleep abgeschaltet (bt_disable vor deep sleep)
 *   - RTC-Offset akkumuliert Gesamtzeit fuer kontinuierlichen Timestamp
 *
 * BThome Payload pro Paket:
 *   0x00  Packet-ID  (uint8,  inkrementierend)
 *   0x40  Distance   (uint16, mm, max 65535 mm)
 *
 * Verdrahtung (ESP32-S3-DevKitC):
 *   HC-SR04 TRIG -> GPIO9
 *   HC-SR04 ECHO -> GPIO8  (ggf. 5V→3.3V Spannungsteiler)
 *   HC-SR04 VCC  -> Power-Gate (GPIOs 4,5,6,7,15,16,17 parallel)
 *   HC-SR04 GND  -> GND
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <esp_sleep.h>
#include <esp_attr.h>
#include <esp_mac.h>
#include <driver/gpio.h>
#include <bthome_v2/bthome_v2.h>

LOG_MODULE_REGISTER(bthome_ranging, LOG_LEVEL_INF);

/* ═══════════════════════════════════════════════════════════════════════════
 * Konfiguration
 * ═══════════════════════════════════════════════════════════════════════════ */

#define TRIG_PULSE_US        10U
#define ECHO_TIMEOUT_MS      35U
/** HC-SR04 Mindest-Startzeit nach Power-On */
#define SENSOR_STARTUP_MS    1000U
/** Dauer des BLE-Advertisings pro Messung */
#define ADV_DURATION_MS      1500U
/** Schlafzeit zwischen den Messungen */
//#define MEASURE_INTERVAL_MS  300000U
#define MEASURE_INTERVAL_MS  1000000U

#define US_TO_MM_NUM  10U
#define US_TO_MM_DEN  58U

/* ═══════════════════════════════════════════════════════════════════════════
 * Devicetree: GPIO-Specs
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DT_USER DT_PATH(zephyr_user)

static const struct gpio_dt_spec trig = GPIO_DT_SPEC_GET(DT_USER, trig_gpios);
static const struct gpio_dt_spec echo = GPIO_DT_SPEC_GET(DT_USER, echo_gpios);

#define _PWR_PIN_INIT(node, prop, idx) GPIO_DT_SPEC_GET_BY_IDX(node, prop, idx),
static const struct gpio_dt_spec power_pins[] = {
	DT_FOREACH_PROP_ELEM(DT_USER, power_gpios, _PWR_PIN_INIT)
};
#define POWER_PIN_COUNT ARRAY_SIZE(power_pins)

/* ═══════════════════════════════════════════════════════════════════════════
 * ECHO-Messung
 * ═══════════════════════════════════════════════════════════════════════════ */

static K_SEM_DEFINE(echo_sem, 0, 1);
static volatile uint32_t echo_rise_cycles;
static volatile uint32_t echo_fall_cycles;
static volatile bool     echo_complete;
static struct gpio_callback echo_cb_data;

static void echo_isr(const struct device *dev, struct gpio_callback *cb,
		     uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	if (gpio_pin_get_dt(&echo)) {
		echo_rise_cycles = k_cycle_get_32();
		echo_complete    = false;
	} else {
		echo_fall_cycles = k_cycle_get_32();
		echo_complete    = true;
		k_sem_give(&echo_sem);
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 * RTC-Persistent-Timestamp (ueberlebt Deep-Sleep)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* RTC_DATA_ATTR / RTC_NOINIT_ATTR sind in Zephyr 3.7 fuer ESP32-S3 leer
 * (CONFIG_SOC_RTC_*_MEM_SUPPORTED nicht gesetzt). Direkt auf die Section
 * ".rtc_noinit" abbilden, die im Linker-Script als NOLOAD in rtc_slow_seg
 * definiert ist und Deep-Sleep ueberlebt. Initialisierung erfolgt manuell
 * beim Erststart (s. main()). */
#define RTC_PERSIST  __attribute__((section(".rtc_noinit")))

RTC_PERSIST static uint32_t s_rtc_offset_ms;
RTC_PERSIST static uint8_t  s_pkt_id;

static log_timestamp_t persistent_ts(void)
{
	return (log_timestamp_t)(s_rtc_offset_ms + (uint32_t)k_uptime_get());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Power-Gate
 * ═══════════════════════════════════════════════════════════════════════════ */

static void sensor_power_on(void)
{
	for (int i = 0; i < POWER_PIN_COUNT; i++) {
		gpio_hold_dis(power_pins[i].pin);
		gpio_pin_set_dt(&power_pins[i], 1);
	}
}

static void sensor_power_off(void)
{
	for (int i = 0; i < POWER_PIN_COUNT; i++) {
		gpio_pin_set_dt(&power_pins[i], 0);
		gpio_hold_en(power_pins[i].pin);
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Distanzmessung
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Fuehrt eine HC-SR04-Messung durch.
 * Gibt die Distanz in mm zurueck, 0 bei Timeout (kein Echo / zu weit).
 */
static uint32_t measure_distance_mm(void)
{
	k_sem_reset(&echo_sem);
	echo_complete = false;

	gpio_pin_set_dt(&trig, 1);
	k_busy_wait(TRIG_PULSE_US);
	gpio_pin_set_dt(&trig, 0);

	int rc = k_sem_take(&echo_sem, K_MSEC(ECHO_TIMEOUT_MS));

	if (rc == -EAGAIN || !echo_complete) {
		LOG_WRN("Kein Echo (Objekt zu weit oder Sensor fehlt)");
		return 0U;
	}

	uint32_t cycles   = echo_fall_cycles - echo_rise_cycles;
	uint64_t pulse_us = (uint64_t)cycles * 1000000ULL
			    / sys_clock_hw_cycles_per_sec();
	uint32_t dist_mm  = (uint32_t)(pulse_us * US_TO_MM_NUM / US_TO_MM_DEN);

	LOG_INF("Distanz: %4u mm  [Echo: %llu us]", dist_mm, pulse_us);
	return dist_mm;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * BThome V2 BLE Advertising
 * ═══════════════════════════════════════════════════════════════════════════ */

static struct bthome_v2_ctx bthome;

/**
 * Baut ein BThome-V2-Paket und sendet es ADV_DURATION_MS lang via
 * nicht-verbindungsfaehigem Legacy-BLE-Advertising.
 *
 * Payload:
 *   0x00  Packet-ID   (uint8)
 *   0x40  Distanz mm  (uint16)
 */
static void advertise_distance(uint32_t dist_mm)
{
	bthome_v2_clear(&bthome);
	bthome_v2_add_packet_id(&bthome, s_pkt_id);
	bthome_v2_add_distance_mm(&bthome, (uint16_t)dist_mm);
	bthome_v2_encode(&bthome);

	/* AD-Struktur: Flags + Name + BThome Service Data */
	struct bt_data ad[3] = {
		BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
		BT_DATA(BT_DATA_NAME_COMPLETE,
			CONFIG_BT_DEVICE_NAME,
			sizeof(CONFIG_BT_DEVICE_NAME) - 1),
		{ 0 },  /* wird von bthome_v2_get_bt_data befuellt */
	};
	bthome_v2_get_bt_data(&bthome, &ad[2]);

	/* Nicht-verbindungsfaehiges, undirected Advertising (Legacy PDU) */
	/* BT_LE_ADV_OPT_USE_IDENTITY: erzwingt die eFuse-basierte Identity-Adresse
	 * im Advertising-PDU statt einer neu generierten Random-Adresse. */
	int ret = bt_le_adv_start(
		BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_IDENTITY,
				BT_GAP_ADV_FAST_INT_MIN_2,
				BT_GAP_ADV_FAST_INT_MAX_2, NULL),
		ad, ARRAY_SIZE(ad), NULL, 0);

	if (ret) {
		LOG_ERR("bt_le_adv_start: %d", ret);
		return;
	}

	LOG_INF("BThome ADV: %u mm  pkt_id=%u  (%u ms)", dist_mm, s_pkt_id,
		ADV_DURATION_MS);

	k_msleep(ADV_DURATION_MS);
	bt_le_adv_stop();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
	int ret;
	int64_t boot_time_ms;

	/* Persistenten Timestamp registrieren */
	log_set_timestamp_func(persistent_ts, 1000U);

	/* Wakeup-Ursache bestimmen */
	esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

	/* Beim echten Erststart (kein Timer-Wakeup) RTC-Variablen initialisieren. */
	if (cause != ESP_SLEEP_WAKEUP_TIMER) {
		s_rtc_offset_ms = 0;
		s_pkt_id        = 0;
	}

	/* Paket-ID sofort hochzaehlen — vor allen anderen Operationen, damit
	 * sie auch bei einem spaeter auftretendem Crash nicht verloren geht. */
	s_pkt_id++;

	/* ── GPIO initialisieren ─────────────────────────────────────────── */
	if (!gpio_is_ready_dt(&trig) || !gpio_is_ready_dt(&echo)) {
		LOG_ERR("GPIO nicht bereit");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&trig, GPIO_OUTPUT_INACTIVE);
	gpio_hold_dis(trig.pin);
	gpio_pin_configure_dt(&echo, GPIO_INPUT);
	gpio_hold_dis(echo.pin);

	for (int i = 0; i < POWER_PIN_COUNT; i++) {
		if (!gpio_is_ready_dt(&power_pins[i])) {
			LOG_ERR("Power-GPIO %d nicht bereit", power_pins[i].pin);
			return -ENODEV;
		}
		gpio_pin_configure_dt(&power_pins[i], GPIO_OUTPUT_INACTIVE);
		gpio_hold_dis(power_pins[i].pin);
	}

	gpio_pin_interrupt_configure_dt(&echo, GPIO_INT_EDGE_BOTH);
	gpio_init_callback(&echo_cb_data, echo_isr, BIT(echo.pin));
	gpio_add_callback(echo.port, &echo_cb_data);

	/* ── Wakeup-Info ─────────────────────────────────────────────────── */
	if (cause == ESP_SLEEP_WAKEUP_TIMER) {
		LOG_INF("Wakeup: Timer  (Offset: %u ms)", s_rtc_offset_ms);
	} else {
		LOG_INF("Erster Start  (Power-on / Hard-Reset)");
	}

	/* ── Sensor einschalten + Bluetooth initialisieren ───────────────── *
	 * bt_enable() dauert ~400-600 ms → zaehlt zur Sensor-Startzeit.     */
	sensor_power_on();
	boot_time_ms = k_uptime_get();

	/* eFuse-MAC als feste BT-Identitaet setzen, damit die BLE-Adresse
	 * stabil bleibt (andernfalls generiert bt_enable() bei jedem Boot
	 * eine neue zufaellige Adresse). */
	{
		uint8_t mac[6];
		esp_read_mac(mac, ESP_MAC_BT);
		bt_addr_le_t fixed_addr = {
			.type  = BT_ADDR_LE_RANDOM,
			/* ESP-MAC Big-Endian → BT-Adresse Little-Endian */
			.a.val = { mac[5], mac[4], mac[3], mac[2], mac[1], mac[0] },
		};
		/* Bit 7+6 des MSB auf 11 setzen = Static Random Address (BT-Spec) */
		fixed_addr.a.val[5] |= 0xC0;
		int id_ret = bt_id_create(&fixed_addr, NULL);

		if (id_ret < 0) {
			LOG_ERR("bt_id_create fehlgeschlagen: %d — Adresse nicht stabil!", id_ret);
		} else {
			LOG_INF("BT-ID %d: %02X:%02X:%02X:%02X:%02X:%02X", id_ret,
				fixed_addr.a.val[5], fixed_addr.a.val[4],
				fixed_addr.a.val[3], fixed_addr.a.val[2],
				fixed_addr.a.val[1], fixed_addr.a.val[0]);
		}
	}

	bthome_v2_init(&bthome, false, false);

	ret = bt_enable(NULL);
	if (ret) {
		LOG_ERR("bt_enable: %d", ret);
		return ret;
	}
	LOG_INF("Bluetooth bereit");

	/* Verbleibende Sensor-Startzeit abwarten */
	int64_t elapsed_ms = k_uptime_get() - boot_time_ms;
	int32_t remaining_ms = (int32_t)SENSOR_STARTUP_MS - (int32_t)elapsed_ms;

	if (remaining_ms > 0) {
		LOG_INF("Sensor-Startzeit: noch %d ms...", remaining_ms);
		k_msleep((uint32_t)remaining_ms);
	}

	/* ── Messen ──────────────────────────────────────────────────────── */
	uint32_t dist_mm = measure_distance_mm();

	/* ── Sensor ausschalten ──────────────────────────────────────────── */
	sensor_power_off();

	/* ── BThome-Paket senden ─────────────────────────────────────────── */
	advertise_distance(dist_mm);

	/* ── Bluetooth deaktivieren ──────────────────────────────────────── */
	bt_disable();

	/* ── Zeitversatz akkumulieren + Deep-Sleep ───────────────────────── */
	s_rtc_offset_ms += (uint32_t)k_uptime_get() + MEASURE_INTERVAL_MS;

	LOG_INF("Deep-Sleep fuer %d s  (naechste Messung bei %u s Offset)",
		MEASURE_INTERVAL_MS / 1000, s_rtc_offset_ms / 1000);

	/* TRIG LOW einfrieren */
	gpio_pin_set_dt(&trig, 0);
	gpio_hold_en(trig.pin);

	/* ECHO-Pin als Output LOW einfrieren (war Input — floatendes Signal
	 * bei ausgeschaltetem Sensor verursacht Leakage-Strom) */
	gpio_pin_configure_dt(&echo, GPIO_OUTPUT_INACTIVE);
	gpio_hold_en(echo.pin);

	log_panic();
	k_busy_wait(50000U);

	esp_sleep_enable_timer_wakeup((uint64_t)MEASURE_INTERVAL_MS * 1000ULL);
	esp_deep_sleep_start(); /* kehrt nicht zurueck */

	return 0;
}
