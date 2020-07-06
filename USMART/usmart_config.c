#include "usmart.h"
#include "usmart_str.h"
////////////////////////////用户配置区///////////////////////////////////////////////
//这下面要包含所用到的函数所申明的头文件(用户自己添加) 
#include "delay.h"	 	
#include "sys.h"
#include "lcd.h"
#include "sdram.h"
#include "ltdc.h"
#include "nand.h"  
#include "nandtester.h"  
#include "ftl.h" 
								 
extern void led_set(u8 sta);
extern void test_fun(void(*ledset)(u8),u8 sta);										  
//函数名列表初始化(用户自己添加)
//用户直接在这里输入要执行的函数名及其查找串
struct _m_usmart_nametab usmart_nametab[]=
{
#if USMART_USE_WRFUNS==1 	//如果使能了读写操作
	(void*)read_addr,"u32 read_addr(u32 addr)",
	(void*)write_addr,"void write_addr(u32 addr,u32 val)",	 
#endif	
	(void*)delay_ms,"void delay_ms(u16 nms)",
 	(void*)delay_us,"void delay_us(u32 nus)",	 

 	(void*)NAND_ModeSet,"u8 NAND_ModeSet(u8 mode)",    
	(void*)NAND_EraseBlock,"u8 NAND_EraseBlock(u32 BlockNum)",    
	(void*)NAND_EraseChip,"void NAND_EraseChip(void)",    
	(void*)NAND_CopyPageWithoutWrite,"u8 NAND_CopyPageWithoutWrite(u32 Source_PageNum,u32 Dest_PageNum)",    
    		
	(void*)test_copypageandwrite,"u8 test_copypageandwrite(u32 spnum,u32 dpnum,u16 colnum,u16 writebytes)",    
	(void*)test_readpage,"u8 test_readpage(u32 pagenum,u16 colnum,u16 readbytes)",    
	(void*)test_writepage,"u8 test_writepage(u32 pagenum,u16 colnum,u16 writebytes)",
	(void*)test_readspare,"u8 test_readspare(u32 pagenum,u16 colnum,u16 readbytes)",
	(void*)test_readallblockinfo,"void test_readallblockinfo(u32 sblock)",
	(void*)test_ftlwritesectors,"u8 test_ftlwritesectors(u32 secx,u16 secsize,u16 seccnt)",
	(void*)test_ftlreadsectors,"u8 test_ftlreadsectors(u32 secx,u16 secsize,u16 seccnt)",
		
	(void*)FTL_Init,"u8 FTL_Init(void)",
	(void*)FTL_CheckBadBlock,"u8 FTL_CheckBadBlock(u32 blocknum)",
	(void*)FTL_UsedBlockMark,"u8 FTL_UsedBlockMark(u32 blocknum)",
 	(void*)FTL_FindUnusedBlock,"u32 FTL_FindUnusedBlock(u32 sblock,u8 flag)",
	(void*)FTL_FindSamePlaneUnusedBlock,"u32 FTL_FindSamePlaneUnusedBlock(u32 blocknum)",
	(void*)FTL_LBNToPBN,"u16 FTL_LBNToPBN(u32 blocknum)",
	(void*)FTL_CreateLUT,"u8 FTL_CreateLUT(void)",
	(void*)FTL_Format,"u8 FTL_Format(void)",    
					
};						  
///////////////////////////////////END///////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
//函数控制管理器初始化
//得到各个受控函数的名字
//得到函数总数量
struct _m_usmart_dev usmart_dev=
{
	usmart_nametab,
	usmart_init,
	usmart_cmd_rec,
	usmart_exe,
	usmart_scan,
	sizeof(usmart_nametab)/sizeof(struct _m_usmart_nametab),//函数数量
	0,	  	//参数数量
	0,	 	//函数ID
	1,		//参数显示类型,0,10进制;1,16进制
	0,		//参数类型.bitx:,0,数字;1,字符串	    
	0,	  	//每个参数的长度暂存表,需要MAX_PARM个0初始化
	0,		//函数的参数,需要PARM_LEN个0初始化
};   



















