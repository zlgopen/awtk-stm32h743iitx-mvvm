#include "sys.h"
#include "delay.h"
#include "usart.h" 
#include "led.h"
#include "key.h"
#include "ltdc.h"
#include "lcd.h"
#include "sdram.h"
#include "usmart.h"
#include "pcf8574.h"
#include "mpu.h"
#include "malloc.h"
#include "w25qxx.h"
#include "sdmmc_sdcard.h"
#include "nand.h"    
#include "ftl.h"  
#include "ff.h"
#include "touch.h"
#include "exfuns.h"
/************************************************
 ALIENTEK 阿波罗STM32H7开发板 实验42
 FATFS实验-HAL库函数版
 技术支持：www.openedv.com
 淘宝店铺：http://eboard.taobao.com 
 关注微信公众平台微信号："正点原子"，免费获取STM32资料。
 广州市星翼电子科技有限公司  
 作者：正点原子 @ALIENTEK
************************************************/

#include "tkc/fs.h"
#include "tkc/thread.h"
#include "platforms/common/rtos.h"

extern void sleep_ms(int ms);
extern void sys_tick_init(int SYSCLK);
extern ret_t platform_prepare(void);
extern void systick_enable_int(void);
extern int gui_app_start(int lcd_w, int lcd_h);
extern int sqlite3_demo(const char* db_filename);

void* awtk_thread(void* args) {
  sqlite3_demo("/data/test.db");  
  assert(file_exist("0://awtk/assets/default/raw/fonts/default.ttf"));
	
  gui_app_start(lcdltdc.width, lcdltdc.height);

  return NULL;
}

static ret_t awtk_start_ui_thread(void) {
  tk_thread_t* ui_thread = tk_thread_create(awtk_thread, NULL);
  return_value_if_fail(ui_thread != NULL, RET_BAD_PARAMS);

  tk_thread_set_priority(ui_thread, 3);
  tk_thread_set_name(ui_thread, "awtk");
  tk_thread_set_stack_size(ui_thread, 0xc000);

  return tk_thread_start(ui_thread);
}

int main(void)
{
  Cache_Enable();                 
  MPU_Memory_Protection();        
  HAL_Init();                   
  Stm32_Clock_Init(160,5,2,4); 
  delay_init(400);            
  uart_init(115200);            
  usmart_dev.init(200);     
  LED_Init();               
  KEY_Init();               
  SDRAM_Init();      
  LCD_Init();               
  W25QXX_Init();        
  LTDC_Display_Dir(1);  
  sys_tick_init(400);
  
  tp_dev.init();
  
  platform_prepare();
    

  LCD_ShowString(30,130,200,16,16,"check sdcard");        
  while(SD_Init())
  {
    LCD_ShowString(30,150,200,16,16,"SD Card Error!");
    delay_ms(500);          
    LCD_ShowString(30,150,200,16,16,"Please Check! ");
    delay_ms(500);
  }
  LCD_ShowString(30,130,200,16,16,"check sdcard ok");       
  
  FTL_Init();
  exfuns_init();  
  f_mount(fs[0],"0:",1);

 // fs_test(os_fs());
  
  
  rtos_init();
  awtk_start_ui_thread();
  rtos_start();
  
  return 0;
}

