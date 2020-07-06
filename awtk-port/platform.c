/**
 * file:   platform.c
 * author: li xianjing <xianjimli@hotmail.com>
 * brief:  platform dependent function of stm32
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
 * 2018-05-12 li xianjing <xianjimli@hotmail.com> created
 *
 */

#include "delay.h"
#include "tkc/mem.h"
#include "base/timer.h"
#include "tkc/platform.h"

#define MEM2_MAX_SIZE 16 * 1024 * 1024
#define MEM2_ADDR (uint8_t*)0XC0000000 + 2 * 1024 * 1024

ret_t platform_prepare(void) {
  static bool_t inited = FALSE;
	
  if (!inited) {
    inited = TRUE;
    tk_mem_init(MEM2_ADDR, MEM2_MAX_SIZE);
  }
  
  return RET_OK;
}
