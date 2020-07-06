#include "ltdc.h"
#include "tkc/mem.h"
#include "lcd/lcd_mem_bgr565.h"
#include "lcd/lcd_mem_bgra8888.h"

#define FB_ADDR (uint8_t*)LCD_FRAME_BUF_ADDR

static uint8_t* s_framebuffers[2];

lcd_t* lcd_impl_create(wh_t w, wh_t h) {
  lcd_t* lcd = NULL;
  uint32_t size = w * h * lcdltdc.pixsize;

  s_framebuffers[0] = FB_ADDR;
  s_framebuffers[1] = FB_ADDR + size;
	
#if LCD_PIXFORMAT==LCD_PIXFORMAT_ARGB8888
  lcd = lcd_mem_bgra8888_create_double_fb(w, h, s_framebuffers[0], s_framebuffers[1]);
#else
  lcd = lcd_mem_bgr565_create_double_fb(w, h, s_framebuffers[0], s_framebuffers[1]);
#endif /*LCD_PIXFORMAT*/
	
  return lcd;
}

