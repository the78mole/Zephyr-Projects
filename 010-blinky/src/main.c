/*
 * Blinky RGB – ESP32-S3-DevKitC
 *
 * Blendet sanft zwischen Farben über (WS2812 via SPI3, GPIO48).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#define LOG_LEVEL 4
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(blinky_rgb);

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#define STRIP_NODE       DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS DT_PROP(DT_ALIAS(led_strip), chain_length)

/* Dauer eines Übergangs: FADE_STEPS × FADE_STEP_MS = 1200 ms */
#define FADE_STEPS    60
#define FADE_STEP_MS  20

#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

/* ~20 % Helligkeit (0x33 = 51 von 255) */
static const struct led_rgb colors[] = {
	RGB(0x33, 0x00, 0x00), /* Rot     */
	RGB(0x33, 0x0f, 0x00), /* Orange  */
	RGB(0x33, 0x33, 0x00), /* Gelb    */
	RGB(0x00, 0x33, 0x00), /* Grün    */
	RGB(0x00, 0x33, 0x33), /* Cyan    */
	RGB(0x00, 0x00, 0x33), /* Blau    */
	RGB(0x33, 0x00, 0x33), /* Magenta */
	RGB(0x33, 0x33, 0x33), /* Weiß    */
	RGB(0x00, 0x00, 0x00), /* Aus     */
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

int main(void)
{
	size_t color_idx = 0;
	int rc;

	if (!device_is_ready(strip)) {
		LOG_ERR("LED-Strip-Gerät '%s' nicht bereit", strip->name);
		return 0;
	}

	LOG_INF("RGB-Blinky gestartet auf '%s' (%u Pixel)",
		strip->name, (unsigned int)STRIP_NUM_PIXELS);

	while (1) {
		const struct led_rgb *from = &colors[color_idx];
		color_idx = (color_idx + 1) % ARRAY_SIZE(colors);
		const struct led_rgb *to = &colors[color_idx];

		for (int step = 0; step <= FADE_STEPS; step++) {
			pixels[0].r = (uint8_t)((int)from->r + (int)(to->r - from->r) * step / FADE_STEPS);
			pixels[0].g = (uint8_t)((int)from->g + (int)(to->g - from->g) * step / FADE_STEPS);
			pixels[0].b = (uint8_t)((int)from->b + (int)(to->b - from->b) * step / FADE_STEPS);

			rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
			if (rc) {
				LOG_ERR("Strip-Update fehlgeschlagen: %d", rc);
			}
			k_sleep(K_MSEC(FADE_STEP_MS));
		}
	}

	return 0;
}
