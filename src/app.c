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

#include "app.h"

#include <stdbool.h>
#include <stdint.h>

#include "os.h"
#include "os_io_seproxyhal.h"
#include "syscalls.h"

#include "bui.h"

static const uint16_t app_snake_growth_limit[] = {20, 30, 40, 50, APP_MAX_SNAKE_LEN};

static bui_bitmap_128x32_t app_disp_buffer;
static signed char app_disp_progress;

static app_mode_e app_mode;
static uint32_t app_game_tickn;
static uint16_t app_snake_len;
static app_pos_t app_snake_pos[APP_MAX_SNAKE_LEN];
static uint16_t app_snake_pos_offset;
static app_dir_e app_snake_dir;
static bool app_snake_turn_cooldown;
static uint8_t app_snake_speed; // Must be >= 1 and <= 5
static int8_t app_snake_to_grow;
static app_pos_t app_coins[APP_MAX_NCOINS];
static uint8_t app_ncoins;
static uint8_t app_dead_tickn;

void app_init() {
	app_disp_progress = -1;
	app_mode_menu();

	// Set a ticker interval of APP_TICK_INTERVAL ms
	G_io_seproxyhal_spi_buffer[0] = SEPROXYHAL_TAG_SET_TICKER_INTERVAL;
	G_io_seproxyhal_spi_buffer[1] = 0;
	G_io_seproxyhal_spi_buffer[2] = 2;
	G_io_seproxyhal_spi_buffer[3] = 0;
	G_io_seproxyhal_spi_buffer[4] = APP_TICK_INTERVAL;
	io_seproxyhal_spi_send(G_io_seproxyhal_spi_buffer, 5);

	// First app tick
	app_tick();
}

void app_mode_menu() {
	// Set globals
	app_mode = APP_MODE_MENU;
}

void app_mode_start() {
	// Set globals
	app_mode = APP_MODE_PLAYING;
	app_game_tickn = 0;
	app_snake_len = 7;
	os_memset(app_snake_pos, 0, sizeof(app_snake_pos));
	app_snake_pos_offset = 0;
	app_set_snake_pos(0, (app_pos_t) {64, 24});
	app_set_snake_pos(1, (app_pos_t) {64, 25});
	app_set_snake_pos(2, (app_pos_t) {64, 26});
	app_set_snake_pos(3, (app_pos_t) {64, 27});
	app_set_snake_pos(4, (app_pos_t) {64, 28});
	app_set_snake_pos(5, (app_pos_t) {64, 29});
	app_set_snake_pos(6, (app_pos_t) {64, 30});
	app_snake_dir = APP_DIR_UP;
	app_snake_turn_cooldown = false;
	app_snake_speed = 1;
	app_snake_to_grow = 0;
	os_memset(app_coins, 0, sizeof(app_coins));
	app_ncoins = 0;
}

void app_game_pause() {
	// Set globals
	app_mode = APP_MODE_PAUSED;
}

void app_mode_unpause() {
	// Set globals
	app_mode = APP_MODE_PLAYING;
}

void app_mode_die() {
	// Set globals
	app_mode = APP_MODE_DEAD;
	app_dead_tickn = 0;
}

void app_tick() {
	switch (app_mode) {
	case APP_MODE_MENU:
		// Do nothing
		break;
	case APP_MODE_PLAYING:
		app_game_tick();
		break;
	case APP_MODE_PAUSED:
		// Do nothing
		break;
	case APP_MODE_DEAD:
		if (app_dead_tickn != APP_DEATH_WAIT)
			app_dead_tickn += 1;
		break;
	}
	if (app_disp_progress == -1) {
		bui_fill(&app_disp_buffer, false);
		app_draw();
		app_disp_progress = bui_display(&app_disp_buffer, 0);
	}
}

void app_game_tick() {
	if (app_game_tickn % (APP_MAX_SNAKE_SPEED + 1 - app_snake_speed) == 0) {
		app_pos_t head = app_get_snake_pos(0);
		switch (app_snake_dir) {
		case APP_DIR_UP:
			if (head.y == 0)
				app_mode_die();
			else
				head.y -= 1;
			break;
		case APP_DIR_RIGHT:
			if (head.x == 127)
				app_mode_die();
			else
				head.x += 1;
			break;
		case APP_DIR_DOWN:
			if (head.y == 31)
				app_mode_die();
			else
				head.y += 1;
			break;
		case APP_DIR_LEFT:
			if (head.x == 0)
				app_mode_die();
			else
				head.x -= 1;
			break;
		}
		if (app_snake_intersects(head))
			app_mode_die();
		app_snake_turn_cooldown = false;
		if (app_snake_to_grow == 0) {
			app_remove_last_snake_pos();
		} else if (app_snake_to_grow > 0) {
			app_snake_to_grow -= 1;
		} else {
			app_remove_last_snake_pos();
			app_remove_last_snake_pos();
			app_snake_to_grow += 1;
		}
		app_prepend_snake_pos(head);
	}
	for (uint8_t i = 0; i < app_ncoins; i++) {
		if (app_snake_intersects(app_coins[i])) {
			for (uint8_t j = i; i < app_ncoins - 1; i++) {
				app_coins[i] = app_coins[i + 1];
			}
			app_ncoins -= 1;
			if (app_snake_len + app_snake_to_grow < app_snake_growth_limit[app_snake_speed - 1]) {
				app_snake_to_grow += 1;
			} else if (app_snake_speed < APP_MAX_SNAKE_SPEED) {
				app_snake_to_grow -= 5;
				app_snake_speed += 1;
			}
		}
	}
	if (app_ncoins == 0 || (app_ncoins != APP_MAX_NCOINS && app_game_tickn % 2 == 0 && cx_rng_u8() == 0)) {
		app_pos_t new_coin;
		do {
			new_coin.x = cx_rng_u8() & 0x7F;
			new_coin.y = cx_rng_u8() & 0x1F;
		} while (new_coin.x <= 3 || new_coin.x >= 124 || new_coin.y <= 3 || new_coin.y >= 28 ||
				app_snake_intersects(new_coin) || app_coin_intersects(new_coin));
		app_coins[app_ncoins++] = new_coin;
	}
	app_game_tickn += 1;
}

uint8_t app_int_to_str(int32_t i, uint8_t *str) {
	if (i == 0) {
		*str++ = '0';
		return 1;
	}
	uint8_t len = 0;
	if (i < 0) {
		*str++ = '-';
		len += 1;
		i = -i;
	}
	uint8_t digits[10];
	uint8_t ndigits = 0;
	for (; i != 0; i /= 10) {
		digits[ndigits++] = i % 10;
	}
	for (int j = ndigits - 1; j != -1; j--) {
		*str++ = '0' + digits[j];
	}
	len += ndigits;
	return len;
}

void app_draw_header(const unsigned char *text) {
	bui_draw_string(&app_disp_buffer, text, 64, 0, BUI_HORIZONTAL_ALIGN_CENTER | BUI_VERTICAL_ALIGN_TOP,
			BUI_FONT_OPEN_SANS_BOLD_13);
}

void app_draw_line1(const unsigned char *text) {
	bui_draw_string(&app_disp_buffer, text, 64, 15, BUI_HORIZONTAL_ALIGN_CENTER | BUI_VERTICAL_ALIGN_TOP,
			BUI_FONT_LUCIDA_CONSOLE_8);
}

void app_draw_line2(const unsigned char *text) {
	bui_draw_string(&app_disp_buffer, text, 64, 24, BUI_HORIZONTAL_ALIGN_CENTER | BUI_VERTICAL_ALIGN_TOP,
			BUI_FONT_LUCIDA_CONSOLE_8);
}

void app_draw_cross() {
	bui_draw_bitmap(&app_disp_buffer, bui_bitmap_cross_bitmap, bui_bitmap_cross_w, 0, 0, 3, 12, bui_bitmap_cross_w,
			bui_bitmap_cross_h);
}

void app_draw_check() {
	bui_draw_bitmap(&app_disp_buffer, bui_bitmap_check_bitmap, bui_bitmap_check_w, 0, 0, 117, 13, bui_bitmap_check_w,
			bui_bitmap_check_h);
}

void app_draw_snake() {
	for (uint16_t i = 0; i < app_snake_len; i++) {
		app_pos_t pos = app_get_snake_pos(i);
		bui_set_pixel(&app_disp_buffer, pos.x, pos.y, true);
	}
}

void app_draw_coins() {
	for (uint8_t i = 0; i < app_ncoins; i++) {
		app_pos_t pos = app_coins[i];
		bui_set_pixel(&app_disp_buffer, pos.x, pos.y, true);
	}
}

void app_draw_badge_dashboard() {
	bui_draw_bitmap(&app_disp_buffer, bui_bitmap_badge_dashboard_bitmap, bui_bitmap_badge_dashboard_w, 0, 0, 1, 9,
			bui_bitmap_badge_dashboard_w, bui_bitmap_badge_dashboard_h);
}

void app_draw_badge_cross() {
	bui_draw_bitmap(&app_disp_buffer, bui_bitmap_badge_cross_bitmap, bui_bitmap_badge_cross_w, 0, 0, 113, 9,
			bui_bitmap_badge_cross_w, bui_bitmap_badge_cross_h);
}

void app_draw_stats() {
	uint8_t str_buf[30];
	uint8_t *str_ptr;
	// Draw line 1 - maximum length 11
	str_ptr = str_buf;
	os_memcpy(str_buf, "length: ", 8);
	str_ptr += 8;
	str_ptr += app_int_to_str(app_snake_len + app_snake_to_grow, str_ptr);
	*str_ptr = '\0';
	app_draw_line1(str_buf);
	// Draw line 2 - maximum length 30 (bigger than the screen)
	str_ptr = str_buf;
	os_memcpy(str_ptr, "speed: ", 7);
	str_ptr += 7;
	str_ptr += app_int_to_str(app_snake_speed, str_ptr);
	os_memcpy(str_ptr, ", time: ", 8);
	str_ptr += 8;
	str_ptr += app_int_to_str(app_game_tickn * APP_TICK_INTERVAL, str_ptr);
	os_memcpy(str_ptr, "ms", 2);
	str_ptr += 2;
	*str_ptr = '\0';
	app_draw_line2(str_buf);
}

void app_draw() {
	switch (app_mode) {
	case APP_MODE_MENU:
		app_draw_header("Snake");
		app_draw_line1("Would you like");
		app_draw_line2("to play?");
		app_draw_cross();
		app_draw_check();
		break;
	case APP_MODE_PLAYING:
		app_draw_snake();
		app_draw_coins();
		break;
	case APP_MODE_PAUSED:
		app_draw_header("PAUSED");
		app_draw_badge_dashboard();
		app_draw_badge_cross();
		app_draw_stats();
		break;
	case APP_MODE_DEAD:
		if (app_dead_tickn == APP_DEATH_WAIT) {
			app_draw_header("GAME OVER");
			app_draw_stats();
		} else {
			if (app_dead_tickn % 8 <= 3)
				app_draw_snake();
			app_draw_coins();
		}
		break;
	}
}

app_pos_t app_get_snake_pos(uint16_t n) {
	n += app_snake_pos_offset;
	n %= APP_MAX_SNAKE_LEN;
	return app_snake_pos[n];
}

void app_set_snake_pos(uint16_t n, app_pos_t pos) {
	n += app_snake_pos_offset;
	n %= APP_MAX_SNAKE_LEN;
	app_snake_pos[n] = pos;
}

void app_remove_last_snake_pos() {
	app_snake_len -= 1;
}

void app_prepend_snake_pos(app_pos_t pos) {
	if (app_snake_pos_offset == 0)
		app_snake_pos_offset = APP_MAX_SNAKE_LEN - 1;
	else
		app_snake_pos_offset -= 1;
	app_snake_len += 1;
	app_set_snake_pos(0, pos);
}

bool app_snake_intersects(app_pos_t pos) {
	for (uint16_t i = 0; i < app_snake_len; i++) {
		app_pos_t p = app_get_snake_pos(i);
		if (p.x == pos.x && p.y == pos.y)
			return true;
	}
	return false;
}

bool app_coin_intersects(app_pos_t pos) {
	for (uint8_t i = 0; i < app_ncoins; i++) {
		if (app_coins[i].x == pos.x && app_coins[i].y == pos.y)
			return true;
	}
	return false;
}

void app_event_button_push(unsigned int button_mask, unsigned int button_mask_counter) {
	switch (button_mask) {
	case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT:
		switch (app_mode) {
		case APP_MODE_MENU:
			// Do nothing
			break;
		case APP_MODE_PLAYING:
			app_game_pause();
			break;
		case APP_MODE_PAUSED:
			app_mode_unpause();
			break;
		case APP_MODE_DEAD:
			if (app_dead_tickn == APP_DEATH_WAIT) {
				app_mode_menu();
			}
			break;
		}
		break;
	case BUTTON_EVT_RELEASED | BUTTON_LEFT:
		switch (app_mode) {
		case APP_MODE_MENU:
			os_sched_exit(0); // Go back to the dashboard
			break;
		case APP_MODE_PLAYING:
			if (!app_snake_turn_cooldown) {
				app_snake_dir = app_snake_dir == 0 ? 3 : app_snake_dir - 1; // Turn the snake left
				app_snake_turn_cooldown = true;
			}
			break;
		case APP_MODE_PAUSED:
			app_mode_menu();
			break;
		case APP_MODE_DEAD:
			// Do nothing
			break;
		}
		break;
	case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
		switch (app_mode) {
		case APP_MODE_MENU:
			app_mode_start();
			break;
		case APP_MODE_PLAYING:
			if (!app_snake_turn_cooldown) {
				app_snake_dir = app_snake_dir == 3 ? 0 : app_snake_dir + 1; // Turn the snake right
				app_snake_turn_cooldown = true;
			}
			break;
		case APP_MODE_PAUSED:
			app_mode_unpause();
			break;
		case APP_MODE_DEAD:
			// Do nothing
			break;
		}
		break;
	}
}

void app_event_ticker() {
	app_tick();
}

void app_event_display_processed() {
	if (app_disp_progress != -1)
		app_disp_progress = bui_display(&app_disp_buffer, app_disp_progress);
}
