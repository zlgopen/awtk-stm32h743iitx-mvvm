实验器材:
	阿波罗STM32H7开发板
	
实验目的:  
	学习FATFS,掌握文件系统使用(读/写/删除/创建文件等操作).
	
硬件资源:
	1,DS0(连接在PB1)  
	2,串口1(波特率:115200,PA9/PA10连接在板载USB转串口芯片CH340上面)  
	3,ALIENTEK 2.8/3.5/4.3/7寸/10.1寸LCD模块(包括MCU屏和RGB屏,都支持) 
	4,SD卡,通过SDMMC1(SDMMC_D0~D4(PC8~PC11),SDMMC_SCK(PC12),SDMMC_CMD(PD2))连接 
	5,W25Q256(SPI FLASH芯片,连接在QSPI上)
	6,NAND FLASH(MT29F4G08,512MB)
	
	
实验现象:
	本实验开机的时候先初始化SD卡，初始化成功之后，注册三个工作区（一个给SD卡用、一个给SPI FLASH
	用、一个给NAND FLASH用），然后获取SD卡的容量和剩余空间，并显示在LCD模块上，最后等待USMART输
	入指令进行各项测试。本实验通过DS0指示程序运行状态。
	
	
注意事项:
	1,4.3/7/10.1寸屏需要比较大电流,USB供电可能不足,请用外部电源适配器(推荐外接12V 1A电源).
	2,本例程在LCD_Init函数里面(在lcd.c),用到了printf,如果不初始化串口1,将导致液晶无法显示!!   
	3,本例程需自备标准SD卡一张(即大卡,也可以用TF卡+卡套).
	4,本实验通过USMART调用各种fatfs测试函数实现测试，USMART的使用，请参考实验19的相关内容。 
	 

					正点原子@ALIENTEK
					2018-8-2
					广州市星翼电子科技有限公司
					电话：020-38271790
					传真：020-36773971
					购买：http://shop62103354.taobao.com
					http://shop62057469.taobao.com
					公司网站：www.alientek.com
					技术论坛：www.openedv.com





