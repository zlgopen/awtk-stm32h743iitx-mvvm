/**
 * file:   main_loop_stm32_raw.c
 * author: li xianjing <xianjimli@hotmail.com>
 * brief:  main loop for stm32
 *
 * copyright (c) 2018 - 2018 Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * license file for more details.
 *
 */

/**
 * history:
 * ================================================================
 * 2018-05-11 li xianjing <xianjimli@hotmail.com> created
 *
 */

#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "ltdc.h"
#include "string.h"
#include "sdram.h"
#include "malloc.h"
#include "string.h"
#include "touch.h"
#include "main_loop/main_loop_simple.h"


static ret_t platform_disaptch_touch_events(main_loop_t* loop) {
  int x = 0;
  int y = 0;

  tp_dev.scan(0);

  x = tp_dev.y[0];
  y = lcdltdc.height - tp_dev.x[0];

  if (tp_dev.sta & 1) {		
		if (x < lcdltdc.width && y < lcdltdc.height) {
			main_loop_post_pointer_event(loop, TRUE, x, y);
		}
  } else {
    main_loop_post_pointer_event(loop, FALSE, x, y);
  }

  return RET_OK;
}

#define MAX_KEYS_NR 3
static bool_t s_key_pressed[MAX_KEYS_NR];
static int s_key_map[MAX_KEYS_NR] = {
	TK_KEY_TAB,/*move focus*/
	TK_KEY_RETURN,/*activate*/
	TK_KEY_F3/*back*/
};

static ret_t platform_disaptch_key_events(main_loop_t* loop) {
  uint8_t value = KEY_Scan(0);

	if(value > 0) {
		int key = value - 1;
		s_key_pressed[key] = TRUE;
		main_loop_post_key_event(loop, TRUE, s_key_map[key]);
	} else {
		int i = 0;
		for (i = 0; i < MAX_KEYS_NR; i++) {
			if(s_key_pressed[i]) {
				s_key_pressed[i] = FALSE;
				main_loop_post_key_event(loop, FALSE, s_key_map[i]);
			}
		}
	}

  return RET_OK;
}

static ret_t platform_disaptch_input(main_loop_t* loop) {
	platform_disaptch_key_events(loop);
	platform_disaptch_touch_events(loop);
	
  return RET_OK;
}

extern lcd_t* lcd_impl_create(wh_t w, wh_t h);

lcd_t* platform_create_lcd(wh_t w, wh_t h) {
	return lcd_impl_create(w, h);
}

#include "main_loop/main_loop_raw.inc"
