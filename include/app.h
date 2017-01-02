/*
 * License for the Ledger Nano S Snake Game Application project, originally
 * found here: https://github.com/parkerhoyes/nanos-app-snake
 *
 * Copyright (C) 2016 Parker Hoyes <contact@parkerhoyes.com>
 *
 * This software is provided "as-is", without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim
 *    that you wrote the original software. If you use this software in a
 *    product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef APP_H_
#define APP_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	APP_MODE_MENU,
	APP_MODE_PLAYING,
	APP_MODE_PAUSED,
	APP_MODE_DEAD,
} app_mode_e;

typedef enum {
	APP_DIR_UP = 0,
	APP_DIR_RIGHT,
	APP_DIR_DOWN,
	APP_DIR_LEFT,
} app_dir_e;

typedef struct {
	uint8_t x;
	uint8_t y;
} app_pos_t;

#define APP_TICK_INTERVAL 20
#define APP_DEATH_WAIT 150 // In APP_TICK_INTERVAL increments; must be <= 255
#define APP_MAX_SNAKE_LEN 60
#define APP_MAX_SNAKE_SPEED 5
#define APP_MAX_NCOINS 5

void app_init();
void app_mode_menu();
void app_mode_start();
void app_game_pause();
void app_mode_unpause();
void app_mode_die();
void app_tick();
void app_game_tick();
uint8_t app_int_to_str(int32_t i, uint8_t *str);
void app_draw_header(const unsigned char *text);
void app_draw_line1(const unsigned char *text);
void app_draw_line2(const unsigned char *text);
void app_draw_cross();
void app_draw_check();
void app_draw_snake();
void app_draw_coins();
void app_draw_badge_dashboard();
void app_draw_badge_cross();
void app_draw_stats();
void app_draw();
app_pos_t app_get_snake_pos(uint16_t n);
void app_set_snake_pos(uint16_t n, app_pos_t pos);
void app_remove_last_snake_pos();
void app_prepend_snake_pos(app_pos_t pos);
bool app_snake_intersects(app_pos_t pos);
bool app_coin_intersects(app_pos_t pos);
void app_event_button_push(unsigned int button_mask, unsigned int button_mask_counter);
void app_event_ticker();
void app_event_display_processed();

#endif
