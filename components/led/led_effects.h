#ifndef LED_EFFECTS_H
#define LED_EFFECTS_H

#include <stdint.h>

// Rainbow som rör sig längs stripen
void led_rainbow_chase(uint8_t speed, uint32_t duration_ms);

// Roterande rainbow (hela stripen ändrar färg tillsammans)
void led_rainbow_cycle(uint8_t speed, uint32_t duration_ms);

// En färgad "punkt" som rör sig fram och tillbaka
void led_bouncing_ball(uint8_t r, uint8_t g, uint8_t b, uint8_t speed);

// Färgvåg som färdas längs stripen
void led_color_wipe(uint8_t r, uint8_t g, uint8_t b, uint16_t delay_ms);

// Hjälpfunktion: konvertera HSV till RGB
void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g,
                uint8_t *b);

#endif
