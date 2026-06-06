/*
 * 100-US-Ranging - HC-SR04 Ultraschall-Entfernungsmessung
 *
 * ECHO-Puls wird per GPIO-Interrupt auf beide Flanken gemessen.
 * Timing ueber k_cycle_get_32() (ESP32-S3: 240 MHz -> ~4 ns Aufloesung).
 *
 * Verdrahtung (ESP32-S3-DevKitC):
 *   HC-SR04 VCC  -> 5 V (oder 3,3 V bei HC-SR04P)
 *   HC-SR04 GND  -> GND
 *   HC-SR04 TRIG -> GPIO9
 *   HC-SR04 ECHO -> GPIO8  (ggf. 5->3,3 V Spannungsteiler)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <esp_sleep.h>
#include <esp_attr.h>
#include <driver/gpio.h>

LOG_MODULE_REGISTER(us_ranging, LOG_LEVEL_INF);

#define DT_USER DT_PATH(zephyr_user)
static const struct gpio_dt_spec trig = GPIO_DT_SPEC_GET(DT_USER, trig_gpios);
static const struct gpio_dt_spec echo = GPIO_DT_SPEC_GET(DT_USER, echo_gpios);

/* Power-Gate: alle GPIOs aus der DT-Property als Array */
#define _PWR_PIN_INIT(node, prop, idx) GPIO_DT_SPEC_GET_BY_IDX(node, prop, idx),
static const struct gpio_dt_spec power_pins[] = {
	DT_FOREACH_PROP_ELEM(DT_USER, power_gpios, _PWR_PIN_INIT)
};
#define POWER_PIN_COUNT ARRAY_SIZE(power_pins)

#define TRIG_PULSE_US         10
#define ECHO_TIMEOUT_MS       35
#define MEASURE_INTERVAL_MS   10000
/* HC-SR04 benoetigt nach Power-on mindestens 50 ms bis zum ersten Trigger */
#define SENSOR_STARTUP_MS     550

#define US_TO_MM_NUM  10
#define US_TO_MM_DEN  58

static K_SEM_DEFINE(echo_sem, 0, 1);
static volatile uint32_t echo_rise_cycles;
static volatile uint32_t echo_fall_cycles;
static volatile bool     echo_complete;

/*
 * Akkumulierter Zeitversatz in ms.
 * RTC_DATA_ATTR legt die Variable in den RTC-Fast-Speicher (0x600fe000),
 * der Deep-Sleep ueberlebt. Bei Power-on-Reset wird er auf 0 zurueckgesetzt.
 */
RTC_DATA_ATTR static uint32_t s_rtc_offset_ms;

static log_timestamp_t persistent_ts(void)
{
	return (log_timestamp_t)(s_rtc_offset_ms + (uint32_t)k_uptime_get());
}

/* Sensor einschalten: alle Power-Pins HIGH */
static void sensor_power_on(void)
{
	for (int i = 0; i < POWER_PIN_COUNT; i++) {
		gpio_hold_dis(power_pins[i].pin);
		gpio_pin_set_dt(&power_pins[i], 1);
	}
}

/* Sensor ausschalten: alle Power-Pins LOW und einfrieren */
static void sensor_power_off(void)
{
	for (int i = 0; i < POWER_PIN_COUNT; i++) {
		gpio_pin_set_dt(&power_pins[i], 0);
		gpio_hold_en(power_pins[i].pin);
	}
}

static struct gpio_callback echo_cb_data;

static void echo_isr(const struct device *dev, struct gpio_callback *cb,
		     uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	if (gpio_pin_get_dt(&echo)) {
		echo_rise_cycles = k_cycle_get_32();
		echo_complete = false;
	} else {
		echo_fall_cycles = k_cycle_get_32();
		echo_complete = true;
		k_sem_give(&echo_sem);
	}
}

int main(void)
{
	int rc;

	/* Persistenten Timestamp sofort registrieren - gilt fuer alle Logs
	 * dieses Boots. Frequenz 1000 = Zephyr interpretiert Wert als ms.
	 * Bei Power-on: s_rtc_offset_ms == 0 -> Timestamp beginnt bei 0.
	 * Nach Deep-Sleep: s_rtc_offset_ms enthaelt akkumulierte Vorlaufzeit.
	 */
	log_set_timestamp_func(persistent_ts, 1000U);

	/* Wakeup-Ursache bestimmen (vor GPIO-Init, damit fruehe Logs korrekt
	 * gestempelt werden) */
	esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

	if (!gpio_is_ready_dt(&trig) || !gpio_is_ready_dt(&echo)) {
		LOG_ERR("GPIO nicht bereit");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&trig, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&echo, GPIO_INPUT);

	/* Pad-Hold vom letzten Deep-Sleep aufheben */
	gpio_hold_dis(trig.pin);

	/* Power-Pins initialisieren (OUTPUT LOW) und Hold aufheben */
	for (int i = 0; i < POWER_PIN_COUNT; i++) {
		if (!gpio_is_ready_dt(&power_pins[i])) {
			LOG_ERR("Power-GPIO %d nicht bereit", power_pins[i].pin);
			return -ENODEV;
		}
		gpio_pin_configure_dt(&power_pins[i], GPIO_OUTPUT_INACTIVE);
	}

	gpio_pin_interrupt_configure_dt(&echo, GPIO_INT_EDGE_BOTH);
	gpio_init_callback(&echo_cb_data, echo_isr, BIT(echo.pin));
	gpio_add_callback(echo.port, &echo_cb_data);

	if (cause == ESP_SLEEP_WAKEUP_TIMER) {
		LOG_INF("Wakeup: Timer  (Offset: %u ms)", s_rtc_offset_ms);
	} else if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
		LOG_INF("Erster Start  (Power-on / Hard-Reset)");
	} else {
		LOG_INF("Wakeup-Ursache: %d  (Offset: %u ms)",
			(int)cause, s_rtc_offset_ms);
	}

	/* Sensor einschalten und Startzeit abwarten */
	sensor_power_on();
	LOG_INF("Sensor Power-on, warte %d ms...", SENSOR_STARTUP_MS);
	k_msleep(SENSOR_STARTUP_MS);

	while (1) {
		k_sem_reset(&echo_sem);
		echo_complete = false;

		gpio_pin_set_dt(&trig, 1);
		k_busy_wait(TRIG_PULSE_US);
		gpio_pin_set_dt(&trig, 0);

		rc = k_sem_take(&echo_sem, K_MSEC(ECHO_TIMEOUT_MS));

		if (rc == -EAGAIN || !echo_complete) {
			LOG_WRN("Kein Echo (Objekt zu weit oder Sensor fehlt)");
		} else {
			uint32_t cycles = echo_fall_cycles - echo_rise_cycles;
			uint64_t pulse_us = (uint64_t)cycles * 1000000ULL
					    / sys_clock_hw_cycles_per_sec();
			uint32_t dist_mm = (uint32_t)(pulse_us * US_TO_MM_NUM
						      / US_TO_MM_DEN);

			LOG_INF("Distanz: %4u mm  [Echo: %llu us]",
				dist_mm, pulse_us);
		}

		/* Zeitversatz fuer den naechsten Boot akkumulieren:
		 * aktuelle Laufzeit + nominelle Sleep-Dauer.
		 */
		s_rtc_offset_ms += (uint32_t)k_uptime_get() + MEASURE_INTERVAL_MS;
		LOG_INF("Deep-Sleep fuer %d s...", MEASURE_INTERVAL_MS / 1000);
		log_panic();
		/* Sensor abschalten und TRIG/Power-Pins einfrieren (LOW) */
		sensor_power_off();
		gpio_pin_set_dt(&trig, 0);
		gpio_hold_en(trig.pin);
		k_busy_wait(50000U);
		esp_sleep_enable_timer_wakeup(
			(uint64_t)MEASURE_INTERVAL_MS * 1000ULL);
		esp_deep_sleep_start(); /* kehrt nicht zurueck */
	}

	return 0;
}
