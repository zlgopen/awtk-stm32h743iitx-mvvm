#include "sys.h"
#include "ov5640.h"
#include "ov5640cfg.h"
#include "ov5640af.h"
#include "timer.h"	  
#include "delay.h"
#include "usart.h"			 
#include "sccb.h"	
#include "pcf8574.h"  
#include "ltdc.h"  
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32开发板
//OV5640 驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2016/1/6
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 
 

//OV5640写寄存器
//返回值:0,成功;1,失败.
u8 OV5640_WR_Reg(u16 reg,u8 data)
{
	u8 res=0;
	SCCB_Start(); 					//启动SCCB传输
	if(SCCB_WR_Byte(OV5640_ADDR))res=1;	//写器件ID	  
   	if(SCCB_WR_Byte(reg>>8))res=1;	//写寄存器高8位地址
   	if(SCCB_WR_Byte(reg))res=1;		//写寄存器低8位地址		  
   	if(SCCB_WR_Byte(data))res=1; 	//写数据	 
  	SCCB_Stop();	  
  	return	res;
}
//OV5640读寄存器
//返回值:读到的寄存器值
u8 OV5640_RD_Reg(u16 reg)
{
	u8 val=0;
	SCCB_Start(); 				//启动SCCB传输
	SCCB_WR_Byte(OV5640_ADDR);	//写器件ID
   	SCCB_WR_Byte(reg>>8);	    //写寄存器高8位地址   
  	SCCB_WR_Byte(reg);			//写寄存器低8位地址	  
 	SCCB_Stop();   
 	//设置寄存器地址后，才是读
	SCCB_Start();
	SCCB_WR_Byte(OV5640_ADDR|0X01);//发送读命令	  
   	val=SCCB_RD_Byte();		 	//读取数据
  	SCCB_No_Ack();
  	SCCB_Stop();
  	return val;
}
 
//设置摄像头模块PWDN脚的状态
//sta:0,PWDN=0,上电.
//    1,PWDN=1,掉电
void OV5640_PWDN_Set(u8 sta)
{
	PCF8574_WriteBit(DCMI_PWDN_IO,sta);
}
//初始化OV5640 
//配置完以后,默认输出是1600*1200尺寸的图片!! 
//返回值:0,成功
//    其他,错误代码
u8 OV5640_Init(void)
{ 
	u16 i=0;
	u16 reg;
	//设置IO     	
     GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_GPIOA_CLK_ENABLE();			//开启GPIOA时钟
	
    GPIO_Initure.Pin=GPIO_PIN_15;           //PA15
    GPIO_Initure.Mode=GPIO_MODE_OUTPUT_PP;  //推挽输出
    GPIO_Initure.Pull=GPIO_PULLUP;          //上拉
    GPIO_Initure.Speed=GPIO_SPEED_FREQ_VERY_HIGH;     //高速
    HAL_GPIO_Init(GPIOA,&GPIO_Initure);     //初始化
    
	PCF8574_Init();			//初始化PCF8574
	OV5640_RST(0);			//必须先拉低OV5640的RST脚,再上电
	delay_ms(20); 
	OV5640_PWDN_Set(0);		//POWER ON
	delay_ms(5);  
	OV5640_RST(1);			//结束复位 
	delay_ms(20);      
  	SCCB_Init();			//初始化SCCB 的IO口 
	delay_ms(5); 
	reg=OV5640_RD_Reg(OV5640_CHIPIDH);	//读取ID 高八位
	reg<<=8;
	reg|=OV5640_RD_Reg(OV5640_CHIPIDL);	//读取ID 低八位
	if(reg!=OV5640_ID)
	{
		printf("ID:%d\r\n",reg);
		return 1;
	}  
	OV5640_WR_Reg(0x3103,0X11);	//system clock from pad, bit[1]
	OV5640_WR_Reg(0X3008,0X82);	//软复位
	delay_ms(10);
 	//初始化 OV5640,采用SXGA分辨率(1600*1200)  
	for(i=0;i<sizeof(ov5640_uxga_init_reg_tbl)/4;i++)
	{
	   	OV5640_WR_Reg(ov5640_uxga_init_reg_tbl[i][0],ov5640_uxga_init_reg_tbl[i][1]);
 	}   
	//检查闪光灯是否正常
	OV5640_Flash_Ctrl(1);//打开闪光灯
	delay_ms(50);
	OV5640_Flash_Ctrl(0);//关闭闪光灯
  	return 0x00; 	//ok
} 
//OV5640切换为JPEG模式
void OV5640_JPEG_Mode(void) 
{
	u16 i=0; 
	//设置:输出JPEG数据
	for(i=0;i<(sizeof(OV5640_jpeg_reg_tbl)/4);i++)
	{
		OV5640_WR_Reg(OV5640_jpeg_reg_tbl[i][0],OV5640_jpeg_reg_tbl[i][1]);  
	}  
}
//OV5640切换为RGB565模式
void OV5640_RGB565_Mode(void) 
{
	u16 i=0;
	//设置:RGB565输出
	for(i=0;i<(sizeof(ov5640_rgb565_reg_tbl)/4);i++)
	{
		OV5640_WR_Reg(ov5640_rgb565_reg_tbl[i][0],ov5640_rgb565_reg_tbl[i][1]); 
	} 
	if(lcdltdc.pwidth!=0)OV5640_WR_Reg(0X3821,0X06);//RGB屏,水平镜像
} 
//灯光模式参数表,支持5个模式
const static u8 OV5640_LIGHTMODE_TBL[5][7]=
{ 
	0x04,0X00,0X04,0X00,0X04,0X00,0X00,//Auto,自动 
	0x06,0X1C,0X04,0X00,0X04,0XF3,0X01,//Sunny,日光
	0x05,0X48,0X04,0X00,0X07,0XCF,0X01,//Office,办公室
	0x06,0X48,0X04,0X00,0X04,0XD3,0X01,//Cloudy,阴天 
	0x04,0X10,0X04,0X00,0X08,0X40,0X01,//Home,室内
}; 
//白平衡设置
//0:自动
//1:日光sunny
//2,办公室office
//3,阴天cloudy
//4,家里home
void OV5640_Light_Mode(u8 mode)
{
	u8 i;
	OV5640_WR_Reg(0x3212,0x03);	//start group 3
	for(i=0;i<7;i++)OV5640_WR_Reg(0x3400+i,OV5640_LIGHTMODE_TBL[mode][i]);//设置饱和度 
	OV5640_WR_Reg(0x3212,0x13); //end group 3
	OV5640_WR_Reg(0x3212,0xa3); //launch group 3	
}
//色彩饱和度设置参数表,支持7个等级
const static u8 OV5640_SATURATION_TBL[7][6]=
{ 
	0X0C,0x30,0X3D,0X3E,0X3D,0X01,//-3 
	0X10,0x3D,0X4D,0X4E,0X4D,0X01,//-2	
	0X15,0x52,0X66,0X68,0X66,0X02,//-1	
	0X1A,0x66,0X80,0X82,0X80,0X02,//+0	
	0X1F,0x7A,0X9A,0X9C,0X9A,0X02,//+1	
	0X24,0x8F,0XB3,0XB6,0XB3,0X03,//+2
 	0X2B,0xAB,0XD6,0XDA,0XD6,0X04,//+3
}; 
//色度设置
//sat:0~6,代表饱和度-3~3. 
void OV5640_Color_Saturation(u8 sat)
{ 
	u8 i;
	OV5640_WR_Reg(0x3212,0x03);	//start group 3
	OV5640_WR_Reg(0x5381,0x1c);
	OV5640_WR_Reg(0x5382,0x5a);
	OV5640_WR_Reg(0x5383,0x06);
	for(i=0;i<6;i++)OV5640_WR_Reg(0x5384+i,OV5640_SATURATION_TBL[sat][i]);//设置饱和度 
	OV5640_WR_Reg(0x538b, 0x98);
	OV5640_WR_Reg(0x538a, 0x01);
	OV5640_WR_Reg(0x3212, 0x13); //end group 3
	OV5640_WR_Reg(0x3212, 0xa3); //launch group 3	
}
//亮度设置
//bright:0~8,代表亮度-4~4.
void OV5640_Brightness(u8 bright)
{
	u8 brtval;
	if(bright<4)brtval=4-bright;
	else brtval=bright-4;
	OV5640_WR_Reg(0x3212,0x03);	//start group 3
	OV5640_WR_Reg(0x5587,brtval<<4);
	if(bright<4)OV5640_WR_Reg(0x5588,0x09);
	else OV5640_WR_Reg(0x5588,0x01);
	OV5640_WR_Reg(0x3212,0x13); //end group 3
	OV5640_WR_Reg(0x3212,0xa3); //launch group 3
}
//对比度设置
//contrast:0~6,代表亮度-3~3.
void OV5640_Contrast(u8 contrast)
{
	u8 reg0val=0X00;//contrast=3,默认对比度
	u8 reg1val=0X20;
  	switch(contrast)
	{
		case 0://-3
			reg1val=reg0val=0X14;	 	 
			break;	
		case 1://-2
			reg1val=reg0val=0X18; 	 
			break;	
		case 2://-1
			reg1val=reg0val=0X1C;	 
			break;	
		case 4://1
			reg0val=0X10;	 	 
			reg1val=0X24;	 	 
			break;	
		case 5://2
			reg0val=0X18;	 	 
			reg1val=0X28;	 	 
			break;	
		case 6://3
			reg0val=0X1C;	 	 
			reg1val=0X2C;	 	 
			break;	
	} 
	OV5640_WR_Reg(0x3212,0x03); //start group 3
	OV5640_WR_Reg(0x5585,reg0val);
	OV5640_WR_Reg(0x5586,reg1val); 
	OV5640_WR_Reg(0x3212,0x13); //end group 3
	OV5640_WR_Reg(0x3212,0xa3); //launch group 3
}
//锐度设置
//sharp:0~33,0,关闭;33,auto;其他值,锐度范围.
void OV5640_Sharpness(u8 sharp)
{
	if(sharp<33)//设置锐度值
	{
		OV5640_WR_Reg(0x5308,0x65);
		OV5640_WR_Reg(0x5302,sharp);
	}else	//自动锐度
	{
		OV5640_WR_Reg(0x5308,0x25);
		OV5640_WR_Reg(0x5300,0x08);
		OV5640_WR_Reg(0x5301,0x30);
		OV5640_WR_Reg(0x5302,0x10);
		OV5640_WR_Reg(0x5303,0x00);
		OV5640_WR_Reg(0x5309,0x08);
		OV5640_WR_Reg(0x530a,0x30);
		OV5640_WR_Reg(0x530b,0x04);
		OV5640_WR_Reg(0x530c,0x06);
	}
	
}
//特效设置参数表,支持7个特效
const static u8 OV5640_EFFECTS_TBL[7][3]=
{ 
	0X06,0x40,0X10,//正常 
	0X1E,0xA0,0X40,//冷色
	0X1E,0x80,0XC0,//暖色
	0X1E,0x80,0X80,//黑白
	0X1E,0x40,0XA0,//泛黄 
	0X40,0x40,0X10,//反色
	0X1E,0x60,0X60,//偏绿
}; 
//特效设置
//0:正常    
//1,冷色
//2,暖色   
//3,黑白
//4,偏黄
//5,反色
//6,偏绿	    
void OV5640_Special_Effects(u8 eft)
{ 
	OV5640_WR_Reg(0x3212,0x03); //start group 3
	OV5640_WR_Reg(0x5580,OV5640_EFFECTS_TBL[eft][0]);
	OV5640_WR_Reg(0x5583,OV5640_EFFECTS_TBL[eft][1]);// sat U
	OV5640_WR_Reg(0x5584,OV5640_EFFECTS_TBL[eft][2]);// sat V
	OV5640_WR_Reg(0x5003,0x08);
	OV5640_WR_Reg(0x3212,0x13); //end group 3
	OV5640_WR_Reg(0x3212,0xa3); //launch group 3
}
//测试序列
//mode:0,关闭
//     1,彩条 
//     2,色块
void OV5640_Test_Pattern(u8 mode)
{
	if(mode==0)OV5640_WR_Reg(0X503D,0X00);
	else if(mode==1)OV5640_WR_Reg(0X503D,0X80);
	else if(mode==2)OV5640_WR_Reg(0X503D,0X82);
} 
//闪光灯控制
//mode:0,关闭
//     1,打开 
void OV5640_Flash_Ctrl(u8 sw)
{
	OV5640_WR_Reg(0x3016,0X02);
	OV5640_WR_Reg(0x301C,0X02); 
	if(sw)OV5640_WR_Reg(0X3019,0X02); 
	else OV5640_WR_Reg(0X3019,0X00);
} 
//设置图像输出大小
//OV5640输出图像的大小(分辨率),完全由该函数确定
//offx,offy,为输出图像在OV5640_ImageWin_Set设定窗口(假设长宽为xsize和ysize)上的偏移
//由于开启了scale功能,用于输出的图像窗口为:xsize-2*offx,ysize-2*offy
//width,height:实际输出图像的宽度和高度
//实际输出(width,height),是在xsize-2*offx,ysize-2*offy的基础上进行缩放处理.
//一般设置offx和offy的值为16和4,更小也是可以,不过默认是16和4 
//返回值:0,设置成功
//    其他,设置失败
u8 OV5640_OutSize_Set(u16 offx,u16 offy,u16 width,u16 height)
{ 
    OV5640_WR_Reg(0X3212,0X03);  	//开始组3
    //以下设置决定实际输出尺寸(带缩放)
    OV5640_WR_Reg(0x3808,width>>8);	//设置实际输出宽度高字节
    OV5640_WR_Reg(0x3809,width&0xff);//设置实际输出宽度低字节  
    OV5640_WR_Reg(0x380a,height>>8);//设置实际输出高度高字节
    OV5640_WR_Reg(0x380b,height&0xff);//设置实际输出高度低字节
	//以下设置决定输出尺寸在ISP上面的取图范围
	//范围:xsize-2*offx,ysize-2*offy
    OV5640_WR_Reg(0x3810,offx>>8);	//设置X offset高字节
    OV5640_WR_Reg(0x3811,offx&0xff);//设置X offset低字节
	
    OV5640_WR_Reg(0x3812,offy>>8);	//设置Y offset高字节
    OV5640_WR_Reg(0x3813,offy&0xff);//设置Y offset低字节
	
    OV5640_WR_Reg(0X3212,0X13);		//结束组3
    OV5640_WR_Reg(0X3212,0Xa3);		//启用组3设置
	return 0; 
}

//设置图像开窗大小(ISP大小),非必要,一般无需调用此函数
//在整个传感器上面开窗(最大2592*1944),用于OV5640_OutSize_Set的输出
//注意:本函数的宽度和高度,必须大于等于OV5640_OutSize_Set函数的宽度和高度
//     OV5640_OutSize_Set设置的宽度和高度,根据本函数设置的宽度和高度,由DSP
//     自动计算缩放比例,输出给外部设备.
//width,height:宽度(对应:horizontal)和高度(对应:vertical)  
//返回值:0,设置成功
//    其他,设置失败
u8 OV5640_ImageWin_Set(u16 offx,u16 offy,u16 width,u16 height)
{
	u16 xst,yst,xend,yend;
	xst=offx;
	yst=offy;
	xend=offx+width-1;
	yend=offy+height-1;  
    OV5640_WR_Reg(0X3212,0X03);		//开始组3
	OV5640_WR_Reg(0X3800,xst>>8);	
	OV5640_WR_Reg(0X3801,xst&0XFF);	
	OV5640_WR_Reg(0X3802,yst>>8);	
	OV5640_WR_Reg(0X3803,yst&0XFF);	
	OV5640_WR_Reg(0X3804,xend>>8);	
	OV5640_WR_Reg(0X3805,xend&0XFF);
	OV5640_WR_Reg(0X3806,yend>>8);	
	OV5640_WR_Reg(0X3807,yend&0XFF);
    OV5640_WR_Reg(0X3212,0X13);		//结束组3
    OV5640_WR_Reg(0X3212,0Xa3);		//启用组3设置	 
	return 0;
}   
//初始化自动对焦
//返回值:0,成功;1,失败.
u8 OV5640_Focus_Init(void)
{ 
	u16 i; 
	u16 addr=0x8000;
	u8 state=0x8F;
	OV5640_WR_Reg(0x3000, 0x20);			//reset MCU	 
	for(i=0;i<sizeof(OV5640_AF_Config);i++) //发送配置数组
	{
		OV5640_WR_Reg(addr,OV5640_AF_Config[i]);
		addr++;
	}  
	OV5640_WR_Reg(0x3022,0x00);
	OV5640_WR_Reg(0x3023,0x00);
	OV5640_WR_Reg(0x3024,0x00);
	OV5640_WR_Reg(0x3025,0x00);
	OV5640_WR_Reg(0x3026,0x00);
	OV5640_WR_Reg(0x3027,0x00);
	OV5640_WR_Reg(0x3028,0x00);
	OV5640_WR_Reg(0x3029,0x7f);
	OV5640_WR_Reg(0x3000,0x00); 
	i=0;
	do
	{
		state=OV5640_RD_Reg(0x3029);	
		delay_ms(5);
		i++;
		if(i>1000)return 1;
	}while(state!=0x70); 
	return 0;    
}  
//执行一次自动对焦
//返回值:0,成功;1,失败.
u8 OV5640_Focus_Single(void)
{
	u8 temp; 
	u16 retry=0; 
	OV5640_WR_Reg(0x3022,0x03);		//触发一次自动对焦 
	while(1)
	{
		retry++;
		temp=OV5640_RD_Reg(0x3029);	//检查对焦完成状态
		if(temp==0x10)break;		// focus completed
		delay_ms(5);
		if(retry>1000)return 1;
	}
	return 0;	 		
}
//持续自动对焦,当失焦后,会自动继续对焦
//返回值:0,成功;其他,失败.
u8 OV5640_Focus_Constant(void)
{
	u8 temp=0;   
	u16 retry=0; 
	OV5640_WR_Reg(0x3023,0x01);
	OV5640_WR_Reg(0x3022,0x08);//发送IDLE指令 
	do 
	{
		temp=OV5640_RD_Reg(0x3023); 
		retry++;
		if(retry>1000)return 2;
		delay_ms(5);
	} while(temp!=0x00);   
	OV5640_WR_Reg(0x3023,0x01);
	OV5640_WR_Reg(0x3022,0x04);//发送持续对焦指令 
	retry=0;
	do 
	{
		temp=OV5640_RD_Reg(0x3023); 
		retry++;
		if(retry>1000)return 2;
		delay_ms(5);
	}while(temp!=0x00);//0,对焦完成;1:正在对焦
	return 0;
} 






