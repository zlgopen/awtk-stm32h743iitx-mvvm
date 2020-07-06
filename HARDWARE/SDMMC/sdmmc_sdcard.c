#include "sdmmc_sdcard.h"
#include "string.h"  
#include "usart.h"	 
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32H7开发板
//SDMMC 驱动代码	(仅提供查询模式驱动代码)
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2018/7/31
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved		 
//********************************************************************************
//升级说明
//无
////////////////////////////////////////////////////////////////////////////////// 	 
static u8 CardType=STD_CAPACITY_SD_CARD_V1_1;			//SD卡类型（默认为1.x卡）
static u32 CSD_Tab[4],CID_Tab[4],RCA=0;					//SD卡CSD,CID以及相对地址(RCA)数据
SD_CardInfo SDCardInfo;									//SD卡信息

//SD_ReadDisk/SD_WriteDisk函数专用buf,当这两个函数的数据缓存区地址不是4字节对齐的时候,
//需要用到该数组,确保数据缓存区地址是4字节对齐的.
__align(4) u8 SDMMC_DATA_BUFFER[512];						  
 

//初始化SD卡
//返回值:错误代码;(0,无错误)
SD_Error SD_Init(void)
{
	SD_Error errorstatus=SD_OK;	  
	u8 clkdiv=0;
	//SDMMC1 IO口初始化   	 
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_SDMMC1_CLK_ENABLE();  //使能SDMMC1时钟
    __HAL_RCC_GPIOC_CLK_ENABLE();   //使能GPIOC时钟
    __HAL_RCC_GPIOD_CLK_ENABLE();   //使能GPIOD时钟
    
    //PC8,9,10,11,12
    GPIO_Initure.Pin=GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12;
    GPIO_Initure.Mode=GPIO_MODE_AF_PP;      //推挽复用
    GPIO_Initure.Pull=GPIO_NOPULL;          //无上下拉
    GPIO_Initure.Speed=GPIO_SPEED_FREQ_VERY_HIGH;     //高速
    GPIO_Initure.Alternate=GPIO_AF12_SDIO1; //复用为SDIO
    HAL_GPIO_Init(GPIOC,&GPIO_Initure);     //初始化
    
    //PD2
    GPIO_Initure.Pin=GPIO_PIN_2;            
    HAL_GPIO_Init(GPIOD,&GPIO_Initure);     //初始化
    
 	//SDMMC外设寄存器设置为默认值 			   
	SDMMC1->POWER=0x00000000;
	SDMMC1->CLKCR=0x00000000;
	SDMMC1->ARG=0x00000000;
	SDMMC1->CMD=0x00000000;
	SDMMC1->DTIMER=0x00000000;
	SDMMC1->DLEN=0x00000000;
	SDMMC1->DCTRL=0x00000000;
	SDMMC1->ICR=0X1FE00FFF;
	SDMMC1->MASK=0x00000000;	  

    HAL_NVIC_SetPriority(SDMMC1_IRQn,2,0);  //配置SDMMC1中断，抢占优先级2，子优先级0
    HAL_NVIC_EnableIRQ(SDMMC1_IRQn);        //使能SDMMC1中断
    
   	errorstatus=SD_PowerON();			//SD卡上电
 	if(errorstatus==SD_OK)errorstatus=SD_InitializeCards();			//初始化SD卡														  
  	if(errorstatus==SD_OK)errorstatus=SD_GetCardInfo(&SDCardInfo);	//获取卡信息
 	if(errorstatus==SD_OK)errorstatus=SD_SelectDeselect((u32)(SDCardInfo.RCA<<16));//选中SD卡   
   	if(errorstatus==SD_OK)errorstatus=SD_EnableWideBusOperation(1);	//4位宽度,如果是MMC卡,则不能用4位模式 
  	if((errorstatus==SD_OK)||(MULTIMEDIA_CARD==CardType))
	{  		    
		if(SDCardInfo.CardType==STD_CAPACITY_SD_CARD_V1_1||SDCardInfo.CardType==STD_CAPACITY_SD_CARD_V2_0)
		{
			clkdiv=SDMMC_TRANSFER_CLK_DIV+2;	//V1.1/V2.0卡，设置最高48/4=12Mhz
		}else clkdiv=SDMMC_TRANSFER_CLK_DIV;	//SDHC等其他卡，设置最高48/2=24Mhz
		SDMMC_Clock_Set(clkdiv);	//设置时钟频率,SDMMC时钟计算公式:SDMMC_CK时钟=SDMMCCLK/[clkdiv+2];其中,SDMMCCLK固定为48Mhz 
  	}
	return errorstatus;		 
}
//SDMMC时钟初始化设置
//clkdiv:时钟分频系数
//CK时钟=sdmmc_ker_ck/[2*clkdiv];(sdmmc_ker_ck钟固定为400Mhz)
void SDMMC_Clock_Set(u16 clkdiv)
{
	u32 tmpreg=SDMMC1->CLKCR; 
  	tmpreg&=0XFFFFFC00; 
 	tmpreg|=clkdiv;   
	SDMMC1->CLKCR=tmpreg;
} 
//SDMMC发送命令函数
//cmdindex:命令索引,低六位有效
//waitrsp:期待的相应.00/10,无响应;01,短响应;11,长响应
//arg:参数
void SDMMC_Send_Cmd(u8 cmdindex,u8 waitrsp,u32 arg)
{			
	u32 tmpreg=0;
	SDMMC1->ARG=arg;  
	tmpreg|=cmdindex&0X3F;	//设置新的index			 
	tmpreg|=(u32)waitrsp<<8;//设置新的wait rsp 
	tmpreg|=0<<10;			//无等待
  	tmpreg|=1<<12;			//命令通道状态机使能
	SDMMC1->CMD=tmpreg;
}
//SDMMC发送数据配置函数
//datatimeout:超时时间设置
//datalen:传输数据长度,低25位有效,必须为块大小的整数倍
//blksize:块大小.实际大小为:2^blksize字节
//dir:数据传输方向:0,控制器到卡;1,卡到控制器;
void SDMMC_Send_Data_Cfg(u32 datatimeout,u32 datalen,u8 blksize,u8 dir)
{
	u32 tmpreg;
	SDMMC1->DTIMER=datatimeout;
  	SDMMC1->DLEN=datalen&0X1FFFFFF;	//低25位有效
	tmpreg=SDMMC1->DCTRL; 
	tmpreg&=0xFFFFFF00;		//清除之前的设置.
	tmpreg|=blksize<<4;		//设置块大小
	tmpreg|=0<<2;			//块数据传输
	tmpreg|=(dir&0X01)<<1;	//方向控制
	tmpreg|=1<<0;			//数据传输使能,DPSM状态机
	SDMMC1->DCTRL=tmpreg;		
}  

//卡上电
//查询所有SDMMC接口上的卡设备,并查询其电压和配置时钟
//返回值:错误代码;(0,无错误)
SD_Error SD_PowerON(void)
{
 	u8 i=0;
	u32 tempreg=0;
	SD_Error errorstatus=SD_OK;
	u32 response=0,count=0,validvoltage=0;
	u32 SDType=SD_STD_CAPACITY;
	//配置CLKCR寄存器  
	tempreg|=0<<12;				//PWRSAV=0,非省电模式 
	tempreg|=0<<14;				//WIDBUS[1:0]=0,1位数据宽度
	tempreg|=0<<16;				//NEGEDGE=0,SDMMCCK下降沿更改命令和数据
	tempreg|=0<<17;				//HWFC_EN=0,关闭硬件流控制    
	SDMMC1->CLKCR=tempreg; 
	SDMMC_Clock_Set(SDMMC_INIT_CLK_DIV);//设置时钟频率(初始化的时候,不能超过400Khz)			 
 	SDMMC1->POWER=0X03;			//上电状态,开启卡时钟     
   	for(i=0;i<74;i++)
	{
		SDMMC_Send_Cmd(SD_CMD_GO_IDLE_STATE,0,0);//发送CMD0进入IDLE STAGE模式命令.												  
		errorstatus=CmdError();
		if(errorstatus==SD_OK)break;
 	}
 	if(errorstatus)return errorstatus;//返回错误状态
	SDMMC_Send_Cmd(SD_SDMMC_SEND_IF_COND,1,SD_CHECK_PATTERN);//发送CMD8,短响应,检查SD卡接口特性.
 														//arg[11:8]:01,支持电压范围,2.7~3.6V
														//arg[7:0]:默认0XAA
														//返回响应7
  	errorstatus=CmdResp7Error();						//等待R7响应
 	if(errorstatus==SD_OK) 								//R7响应正常
	{
		CardType=STD_CAPACITY_SD_CARD_V2_0;				//SD 2.0卡
		SDType=SD_HIGH_CAPACITY;			   			//高容量卡
	}
	SDMMC_Send_Cmd(SD_CMD_APP_CMD,1,0);					//发送CMD55,短响应	 
	errorstatus=CmdResp1Error(SD_CMD_APP_CMD); 		 	//等待R1响应   
	if(errorstatus==SD_OK)//SD2.0/SD 1.1,否则为MMC卡
	{																  
		//SD卡,发送ACMD41 SD_APP_OP_COND,参数为:0x80100000 
		while((!validvoltage)&&(count<SD_MAX_VOLT_TRIAL))
		{	   										   
			SDMMC_Send_Cmd(SD_CMD_APP_CMD,1,0);				//发送CMD55,短响应	 
			errorstatus=CmdResp1Error(SD_CMD_APP_CMD); 	 	//等待R1响应   
 			if(errorstatus!=SD_OK)return errorstatus;   	//响应错误
			SDMMC_Send_Cmd(SD_CMD_SD_APP_OP_COND,1,SD_VOLTAGE_WINDOW_SD|SDType);//发送ACMD41,短响应	 
			errorstatus=CmdResp3Error(); 					//等待R3响应   
 			if(errorstatus!=SD_OK)return errorstatus;   	//响应错误  
			response=SDMMC1->RESP1;;			   				//得到响应
			validvoltage=(((response>>31)==1)?1:0);			//判断SD卡上电是否完成
			count++;
		}
		if(count>=SD_MAX_VOLT_TRIAL)
		{
			errorstatus=SD_INVALID_VOLTRANGE;
			return errorstatus;
		}	 
		if(response&=SD_HIGH_CAPACITY)
		{
			CardType=HIGH_CAPACITY_SD_CARD;
		}
 	}else//MMC卡
	{
		//MMC卡,发送CMD1 SDMMC_SEND_OP_COND,参数为:0x80FF8000 
		while((!validvoltage)&&(count<SD_MAX_VOLT_TRIAL))
		{	   										   				   
			SDMMC_Send_Cmd(SD_CMD_SEND_OP_COND,1,SD_VOLTAGE_WINDOW_MMC);//发送CMD1,短响应	 
			errorstatus=CmdResp3Error(); 					//等待R3响应   
 			if(errorstatus!=SD_OK)return errorstatus;   	//响应错误  
			response=SDMMC1->RESP1;;			   				//得到响应
			validvoltage=(((response>>31)==1)?1:0);
			count++;
		}
		if(count>=SD_MAX_VOLT_TRIAL)
		{
			errorstatus=SD_INVALID_VOLTRANGE;
			return errorstatus;
		}	 			    
		CardType=MULTIMEDIA_CARD;	  
  	}  
  	return(errorstatus);		
}
//SD卡 Power OFF
//返回值:错误代码;(0,无错误)
SD_Error SD_PowerOFF(void)
{
  	SDMMC1->POWER&=~(3<<0);//SDMMC电源关闭,时钟停止	
	return SD_OK;		  
}   
//初始化所有的卡,并让卡进入就绪状态
//返回值:错误代码
SD_Error SD_InitializeCards(void)
{
 	SD_Error errorstatus=SD_OK;
	u16 rca = 0x01;
 	if((SDMMC1->POWER&0X03)==0)return SD_REQUEST_NOT_APPLICABLE;//检查电源状态,确保为上电状态
 	if(SECURE_DIGITAL_IO_CARD!=CardType)				//非SECURE_DIGITAL_IO_CARD
	{
		SDMMC_Send_Cmd(SD_CMD_ALL_SEND_CID,3,0);		//发送CMD2,取得CID,长响应	 
		errorstatus=CmdResp2Error(); 					//等待R2响应   
		if(errorstatus!=SD_OK)return errorstatus;   	//响应错误		    
 		CID_Tab[0]=SDMMC1->RESP1;
		CID_Tab[1]=SDMMC1->RESP2;
		CID_Tab[2]=SDMMC1->RESP3;
		CID_Tab[3]=SDMMC1->RESP4;
	}
	if((STD_CAPACITY_SD_CARD_V1_1==CardType)||(STD_CAPACITY_SD_CARD_V2_0==CardType)||(SECURE_DIGITAL_IO_COMBO_CARD==CardType)||(HIGH_CAPACITY_SD_CARD==CardType))//判断卡类型
	{
		SDMMC_Send_Cmd(SD_CMD_SET_REL_ADDR,1,0);			//发送CMD3,短响应 
		errorstatus=CmdResp6Error(SD_CMD_SET_REL_ADDR,&rca);//等待R6响应 
		if(errorstatus!=SD_OK)return errorstatus;   	//响应错误		    
	}   
    if (MULTIMEDIA_CARD==CardType)
    {
 		SDMMC_Send_Cmd(SD_CMD_SET_REL_ADDR,1,(u32)(rca<<16));//发送CMD3,短响应 	   
		errorstatus=CmdResp2Error(); 					//等待R2响应   
		if(errorstatus!=SD_OK)return errorstatus;   	//响应错误	 
    }
	if (SECURE_DIGITAL_IO_CARD!=CardType)			//非SECURE_DIGITAL_IO_CARD
	{
		RCA = rca;
		SDMMC_Send_Cmd(SD_CMD_SEND_CSD,3,(u32)(rca<<16));//发送CMD9+卡RCA,取得CSD,长响应 	   
		errorstatus=CmdResp2Error(); 					//等待R2响应   
		if(errorstatus!=SD_OK)return errorstatus;   	//响应错误		    
  		CSD_Tab[0]=SDMMC1->RESP1;
		CSD_Tab[1]=SDMMC1->RESP2;
		CSD_Tab[2]=SDMMC1->RESP3;						
		CSD_Tab[3]=SDMMC1->RESP4;					    
	}
	return SD_OK;//卡初始化成功
} 
//得到卡信息
//cardinfo:卡信息存储区
//返回值:错误状态
SD_Error SD_GetCardInfo(SD_CardInfo *cardinfo)
{
 	SD_Error errorstatus=SD_OK;
	u8 tmp=0;	   
	cardinfo->CardType=(u8)CardType; 				//卡类型
	cardinfo->RCA=(u16)RCA;							//卡RCA值
	tmp=(u8)((CSD_Tab[0]&0xFF000000)>>24);
	cardinfo->SD_csd.CSDStruct=(tmp&0xC0)>>6;		//CSD结构
	cardinfo->SD_csd.SysSpecVersion=(tmp&0x3C)>>2;	//2.0协议还没定义这部分(为保留),应该是后续协议定义的
	cardinfo->SD_csd.Reserved1=tmp&0x03;			//2个保留位  
	tmp=(u8)((CSD_Tab[0]&0x00FF0000)>>16);			//第1个字节
	cardinfo->SD_csd.TAAC=tmp;				   		//数据读时间1
	tmp=(u8)((CSD_Tab[0]&0x0000FF00)>>8);	  		//第2个字节
	cardinfo->SD_csd.NSAC=tmp;		  				//数据读时间2
	tmp=(u8)(CSD_Tab[0]&0x000000FF);				//第3个字节
	cardinfo->SD_csd.MaxBusClkFrec=tmp;		  		//传输速度	   
	tmp=(u8)((CSD_Tab[1]&0xFF000000)>>24);			//第4个字节
	cardinfo->SD_csd.CardComdClasses=tmp<<4;    	//卡指令类高四位
	tmp=(u8)((CSD_Tab[1]&0x00FF0000)>>16);	 		//第5个字节
	cardinfo->SD_csd.CardComdClasses|=(tmp&0xF0)>>4;//卡指令类低四位
	cardinfo->SD_csd.RdBlockLen=tmp&0x0F;	    	//最大读取数据长度
	tmp=(u8)((CSD_Tab[1]&0x0000FF00)>>8);			//第6个字节
	cardinfo->SD_csd.PartBlockRead=(tmp&0x80)>>7;	//允许分块读
	cardinfo->SD_csd.WrBlockMisalign=(tmp&0x40)>>6;	//写块错位
	cardinfo->SD_csd.RdBlockMisalign=(tmp&0x20)>>5;	//读块错位
	cardinfo->SD_csd.DSRImpl=(tmp&0x10)>>4;
	cardinfo->SD_csd.Reserved2=0; 					//保留
 	if((CardType==STD_CAPACITY_SD_CARD_V1_1)||(CardType==STD_CAPACITY_SD_CARD_V2_0)||(MULTIMEDIA_CARD==CardType))//标准1.1/2.0卡/MMC卡
	{
		cardinfo->SD_csd.DeviceSize=(tmp&0x03)<<10;	//C_SIZE(12位)
	 	tmp=(u8)(CSD_Tab[1]&0x000000FF); 			//第7个字节	
		cardinfo->SD_csd.DeviceSize|=(tmp)<<2;
 		tmp=(u8)((CSD_Tab[2]&0xFF000000)>>24);		//第8个字节	
		cardinfo->SD_csd.DeviceSize|=(tmp&0xC0)>>6;
 		cardinfo->SD_csd.MaxRdCurrentVDDMin=(tmp&0x38)>>3;
		cardinfo->SD_csd.MaxRdCurrentVDDMax=(tmp&0x07);
 		tmp=(u8)((CSD_Tab[2]&0x00FF0000)>>16);		//第9个字节	
		cardinfo->SD_csd.MaxWrCurrentVDDMin=(tmp&0xE0)>>5;
		cardinfo->SD_csd.MaxWrCurrentVDDMax=(tmp&0x1C)>>2;
		cardinfo->SD_csd.DeviceSizeMul=(tmp&0x03)<<1;//C_SIZE_MULT
 		tmp=(u8)((CSD_Tab[2]&0x0000FF00)>>8);	  	//第10个字节	
		cardinfo->SD_csd.DeviceSizeMul|=(tmp&0x80)>>7;
 		cardinfo->CardCapacity=(cardinfo->SD_csd.DeviceSize+1);//计算卡容量
		cardinfo->CardCapacity*=(1<<(cardinfo->SD_csd.DeviceSizeMul+2));
		cardinfo->CardBlockSize=1<<(cardinfo->SD_csd.RdBlockLen);//块大小
		cardinfo->CardCapacity*=cardinfo->CardBlockSize;
	}else if(CardType==HIGH_CAPACITY_SD_CARD)	//高容量卡
	{
 		tmp=(u8)(CSD_Tab[1]&0x000000FF); 		//第7个字节	
		cardinfo->SD_csd.DeviceSize=(tmp&0x3F)<<16;//C_SIZE
 		tmp=(u8)((CSD_Tab[2]&0xFF000000)>>24); 	//第8个字节	
 		cardinfo->SD_csd.DeviceSize|=(tmp<<8);
 		tmp=(u8)((CSD_Tab[2]&0x00FF0000)>>16);	//第9个字节	
 		cardinfo->SD_csd.DeviceSize|=(tmp);
 		tmp=(u8)((CSD_Tab[2]&0x0000FF00)>>8); 	//第10个字节	
 		cardinfo->CardCapacity=(long long)(cardinfo->SD_csd.DeviceSize+1)*512*1024;//计算卡容量
		cardinfo->CardBlockSize=512; 			//块大小固定为512字节
	}	  
	cardinfo->SD_csd.EraseGrSize=(tmp&0x40)>>6;
	cardinfo->SD_csd.EraseGrMul=(tmp&0x3F)<<1;	   
	tmp=(u8)(CSD_Tab[2]&0x000000FF);			//第11个字节	
	cardinfo->SD_csd.EraseGrMul|=(tmp&0x80)>>7;
	cardinfo->SD_csd.WrProtectGrSize=(tmp&0x7F);
 	tmp=(u8)((CSD_Tab[3]&0xFF000000)>>24);		//第12个字节	
	cardinfo->SD_csd.WrProtectGrEnable=(tmp&0x80)>>7;
	cardinfo->SD_csd.ManDeflECC=(tmp&0x60)>>5;
	cardinfo->SD_csd.WrSpeedFact=(tmp&0x1C)>>2;
	cardinfo->SD_csd.MaxWrBlockLen=(tmp&0x03)<<2;	 
	tmp=(u8)((CSD_Tab[3]&0x00FF0000)>>16);		//第13个字节
	cardinfo->SD_csd.MaxWrBlockLen|=(tmp&0xC0)>>6;
	cardinfo->SD_csd.WriteBlockPaPartial=(tmp&0x20)>>5;
	cardinfo->SD_csd.Reserved3=0;
	cardinfo->SD_csd.ContentProtectAppli=(tmp&0x01);  
	tmp=(u8)((CSD_Tab[3]&0x0000FF00)>>8);		//第14个字节
	cardinfo->SD_csd.FileFormatGrouop=(tmp&0x80)>>7;
	cardinfo->SD_csd.CopyFlag=(tmp&0x40)>>6;
	cardinfo->SD_csd.PermWrProtect=(tmp&0x20)>>5;
	cardinfo->SD_csd.TempWrProtect=(tmp&0x10)>>4;
	cardinfo->SD_csd.FileFormat=(tmp&0x0C)>>2;
	cardinfo->SD_csd.ECC=(tmp&0x03);  
	tmp=(u8)(CSD_Tab[3]&0x000000FF);			//第15个字节
	cardinfo->SD_csd.CSD_CRC=(tmp&0xFE)>>1;
	cardinfo->SD_csd.Reserved4=1;		 
	tmp=(u8)((CID_Tab[0]&0xFF000000)>>24);		//第0个字节
	cardinfo->SD_cid.ManufacturerID=tmp;		    
	tmp=(u8)((CID_Tab[0]&0x00FF0000)>>16);		//第1个字节
	cardinfo->SD_cid.OEM_AppliID=tmp<<8;	  
	tmp=(u8)((CID_Tab[0]&0x000000FF00)>>8);		//第2个字节
	cardinfo->SD_cid.OEM_AppliID|=tmp;	    
	tmp=(u8)(CID_Tab[0]&0x000000FF);			//第3个字节	
	cardinfo->SD_cid.ProdName1=tmp<<24;				  
	tmp=(u8)((CID_Tab[1]&0xFF000000)>>24); 		//第4个字节
	cardinfo->SD_cid.ProdName1|=tmp<<16;	  
	tmp=(u8)((CID_Tab[1]&0x00FF0000)>>16);	   	//第5个字节
	cardinfo->SD_cid.ProdName1|=tmp<<8;		 
	tmp=(u8)((CID_Tab[1]&0x0000FF00)>>8);		//第6个字节
	cardinfo->SD_cid.ProdName1|=tmp;		   
	tmp=(u8)(CID_Tab[1]&0x000000FF);	  		//第7个字节
	cardinfo->SD_cid.ProdName2=tmp;			  
	tmp=(u8)((CID_Tab[2]&0xFF000000)>>24); 		//第8个字节
	cardinfo->SD_cid.ProdRev=tmp;		 
	tmp=(u8)((CID_Tab[2]&0x00FF0000)>>16);		//第9个字节
	cardinfo->SD_cid.ProdSN=tmp<<24;	   
	tmp=(u8)((CID_Tab[2]&0x0000FF00)>>8); 		//第10个字节
	cardinfo->SD_cid.ProdSN|=tmp<<16;	   
	tmp=(u8)(CID_Tab[2]&0x000000FF);   			//第11个字节
	cardinfo->SD_cid.ProdSN|=tmp<<8;		   
	tmp=(u8)((CID_Tab[3]&0xFF000000)>>24); 		//第12个字节
	cardinfo->SD_cid.ProdSN|=tmp;			     
	tmp=(u8)((CID_Tab[3]&0x00FF0000)>>16);	 	//第13个字节
	cardinfo->SD_cid.Reserved1|=(tmp&0xF0)>>4;
	cardinfo->SD_cid.ManufactDate=(tmp&0x0F)<<8;    
	tmp=(u8)((CID_Tab[3]&0x0000FF00)>>8);		//第14个字节
	cardinfo->SD_cid.ManufactDate|=tmp;		 	  
	tmp=(u8)(CID_Tab[3]&0x000000FF);			//第15个字节
	cardinfo->SD_cid.CID_CRC=(tmp&0xFE)>>1;
	cardinfo->SD_cid.Reserved2=1;	 
	return errorstatus;
}
//设置SDMMC总线宽度(MMC卡不支持4bit模式)
//wmode:位宽模式.0,1位数据宽度;1,4位数据宽度;2,8位数据宽度
//返回值:SD卡错误状态
SD_Error SD_EnableWideBusOperation(u32 wmode)
{
  	SD_Error errorstatus=SD_OK;
	u16 clkcr=0;
  	if(MULTIMEDIA_CARD==CardType)return SD_UNSUPPORTED_FEATURE;//MMC卡不支持
 	else if((STD_CAPACITY_SD_CARD_V1_1==CardType)||(STD_CAPACITY_SD_CARD_V2_0==CardType)||(HIGH_CAPACITY_SD_CARD==CardType))
	{
		if(wmode>=2)return SD_UNSUPPORTED_FEATURE;//不支持8位模式
 		else   
		{
			errorstatus=SDEnWideBus(wmode);
 			if(SD_OK==errorstatus)
			{
				clkcr=SDMMC1->CLKCR;	//读取CLKCR的值
				clkcr&=~(3<<14);		//清除之前的位宽设置    
				clkcr|=(u32)wmode<<14;	//1位/4位总线宽度 
				clkcr|=0<<17;			//不开启硬件流控制
				SDMMC1->CLKCR=clkcr;	//重新设置CLKCR值 
			}
		}  
	} 
	return errorstatus; 
} 
//选卡
//发送CMD7,选择相对地址(rca)为addr的卡,取消其他卡.如果为0,则都不选择.
//addr:卡的RCA地址
SD_Error SD_SelectDeselect(u32 addr)
{
 	SDMMC_Send_Cmd(SD_CMD_SEL_DESEL_CARD,1,addr);	//发送CMD7,选择卡,短响应	 	   
   	return CmdResp1Error(SD_CMD_SEL_DESEL_CARD);	  
}  
//SD卡读取单个/多个块 
//buf:读数据缓存区
//addr:读取地址
//blksize:块大小
//nblks:要读取的块数,1,表示读取单个块
//返回值:错误状态 
SD_Error SD_ReadBlocks(u8 *buf,long long addr,u16 blksize,u32 nblks)
{
  	SD_Error errorstatus=SD_OK; 
   	u32 count=0;
	u32 timeout=SDMMC_DATATIMEOUT;  
	u32 *tempbuff=(u32*)buf;	//转换为u32指针 
    SDMMC1->DCTRL=0x0;			//数据控制寄存器清零(关DMA)   
	if(CardType==HIGH_CAPACITY_SD_CARD)//大容量卡
	{
		blksize=512;
		addr>>=9;
	}     
	SDMMC_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,blksize);			//发送CMD16+设置数据长度为blksize,短响应 	   
	errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);			//等待R1响应   
	if(errorstatus!=SD_OK)
    {
        printf("SDMMC_Send_Cmd=%d\r\n",errorstatus);
        return errorstatus;   			//响应错误
    }
	SDMMC_Send_Data_Cfg(SD_DATATIMEOUT,nblks*blksize,9,1);	//nblks*blksize,块大小恒为512,卡到控制器	 
	SDMMC1->CMD|=1<<6;										//CMDTRANS=1,产生一个数据传输命令
	if(nblks>1)												//多块读  
	{									    
		SDMMC_Send_Cmd(SD_CMD_READ_MULT_BLOCK,1,addr);		//发送CMD18+从addr地址出读取数据,短响应 	   
		errorstatus=CmdResp1Error(SD_CMD_READ_MULT_BLOCK);	//等待R1响应   
		if(errorstatus!=SD_OK)
		{	
			printf("SD_CMD_READ_MULT_BLOCK Error\r\n");
			return errorstatus;   		//响应错误	 
		}
	}else													//单块读
	{ 
		SDMMC_Send_Cmd(SD_CMD_READ_SINGLE_BLOCK,1,addr);		//发送CMD17+从addr地址出读取数据,短响应 	   
		errorstatus=CmdResp1Error(SD_CMD_READ_SINGLE_BLOCK);//等待R1响应   
		if(errorstatus!=SD_OK)return errorstatus;   		//响应错误	 
	} 
	INTX_DISABLE();//关闭总中断(POLLING模式,严禁中断打断SDMMC读写操作!!!)
	while(!(SDMMC1->STA&((1<<5)|(1<<1)|(1<<3)|(1<<8))))//无上溢/CRC/超时/完成(标志)
	{
		if(SDMMC1->STA&(1<<15))						//接收区半满,表示至少存了8个字
		{
			for(count=0;count<8;count++)			//循环读取数据
			{
				*(tempbuff+count)=SDMMC1->FIFO;
			}
			tempbuff+=8;	 
			timeout=0X7FFFFF; 	//读数据溢出时间
		}else 	//处理超时
		{
			if(timeout==0)return SD_DATA_TIMEOUT;
			timeout--;
		}
	} 
	SDMMC1->CMD&=~(1<<6);		//CMDTRANS=0,结束数据传输
	INTX_ENABLE();				//开启总中断
	if(SDMMC1->STA&(1<<3))		//数据超时错误
	{										   
		SDMMC1->ICR|=1<<3; 		//清错误标志
		return SD_DATA_TIMEOUT;
	}else if(SDMMC1->STA&(1<<1))//数据块CRC错误
	{
		SDMMC1->ICR|=1<<1; 		//清错误标志
		if(nblks>1)				//针对可能出现的CRC错误,如果是多块读取,必须发送结束传输命令!
		{
			SDMMC_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);		//发送CMD12+结束传输 	   
			errorstatus=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);//等待R1响应   
		}				
		return SD_DATA_CRC_FAIL;		   
	}else if(SDMMC1->STA&(1<<5))//接收fifo上溢错误
	{
		SDMMC1->ICR|=1<<5; 		//清错误标志
		return SD_RX_OVERRUN;		 
	}  
	if((SDMMC1->STA&(1<<8))&&(nblks>1))//多块接收结束,发送结束指令
	{
		if((STD_CAPACITY_SD_CARD_V1_1==CardType)||(STD_CAPACITY_SD_CARD_V2_0==CardType)||(HIGH_CAPACITY_SD_CARD==CardType))
		{
			SDMMC_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);		//发送CMD12+结束传输 	   
			errorstatus=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);//等待R1响应   
			if(errorstatus!=SD_OK)return errorstatus;	 
		}
	}
	SDMMC1->ICR=0X1FE00FFF;	 		//清除所有标记 
	return errorstatus;
}	
 		    																  
//SD卡写单个/多个块 
//buf:数据缓存区
//addr:写地址
//blksize:块大小
//nblks:要读取的块数,1,表示读取单个块
//返回值:错误状态												   
SD_Error SD_WriteBlocks(u8 *buf,long long addr,u16 blksize,u32 nblks)
{
	SD_Error errorstatus = SD_OK;
	u8  cardstate=0;
	u32 timeout=0,bytestransferred=0;
	u32 cardstatus=0,count=0,restwords=0;
	u32 tlen=nblks*blksize;						//总长度(字节)
	u32*tempbuff=(u32*)buf;								 
 	if(buf==NULL)return SD_INVALID_PARAMETER;	//参数错误   
  	SDMMC1->DCTRL=0x0;							//数据控制寄存器清零(关DMA)    
 	if(CardType==HIGH_CAPACITY_SD_CARD)			//大容量卡
	{
		blksize=512;
		addr>>=9;
	}     
	SDMMC_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,blksize);			//发送CMD16+设置数据长度为blksize,短响应 	   
	errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);			//等待R1响应   
	if(errorstatus!=SD_OK)return errorstatus;   			//响应错误  
	if(nblks>1)												//多块写
	{									     
		if(nblks*blksize>SD_MAX_DATA_LENGTH)return SD_INVALID_PARAMETER;   
     	if((STD_CAPACITY_SD_CARD_V1_1==CardType)||(STD_CAPACITY_SD_CARD_V2_0==CardType)||(HIGH_CAPACITY_SD_CARD==CardType))
    	{
			//提高性能
	 	   	SDMMC_Send_Cmd(SD_CMD_APP_CMD,1,(u32)RCA<<16);	//发送ACMD55,短响应 	   
			errorstatus=CmdResp1Error(SD_CMD_APP_CMD);		//等待R1响应   		   
			if(errorstatus!=SD_OK)return errorstatus;				    
	 	   	SDMMC_Send_Cmd(SD_CMD_SET_BLOCK_COUNT,1,nblks);	//发送CMD23,设置块数量,短响应 	   
			errorstatus=CmdResp1Error(SD_CMD_SET_BLOCK_COUNT);//等待R1响应   		   
			if(errorstatus!=SD_OK)return errorstatus;				    
		} 
		SDMMC_Send_Cmd(SD_CMD_WRITE_MULT_BLOCK,1,addr);		//发送CMD25,多块写指令,短响应 	   
		errorstatus=CmdResp1Error(SD_CMD_WRITE_MULT_BLOCK);	//等待R1响应    
	}else													//单块写		
	{ 
		SDMMC_Send_Cmd(SD_CMD_SEND_STATUS,1,(u32)RCA<<16);	//发送CMD13,查询卡的状态,短响应 	   
		errorstatus=CmdResp1Error(SD_CMD_SEND_STATUS);		//等待R1响应   		   
		if(errorstatus!=SD_OK)return errorstatus;
		cardstatus=SDMMC1->RESP1;													  
		timeout=SD_DATATIMEOUT;
		while(((cardstatus&0x00000100)==0)&&(timeout>0)) 	//检查READY_FOR_DATA位是否置位
		{
			timeout--;
			SDMMC_Send_Cmd(SD_CMD_SEND_STATUS,1,(u32)RCA<<16);//发送CMD13,查询卡的状态,短响应 	   
			errorstatus=CmdResp1Error(SD_CMD_SEND_STATUS);	//等待R1响应   		   
			if(errorstatus!=SD_OK)return errorstatus;				    
			cardstatus=SDMMC1->RESP1;													  
		}
		if(timeout==0)return SD_ERROR;  
		SDMMC_Send_Cmd(SD_CMD_WRITE_SINGLE_BLOCK,1,addr);	//发送CMD24,写单块指令,短响应 	   
		errorstatus=CmdResp1Error(SD_CMD_WRITE_SINGLE_BLOCK);//等待R1响应   	
	}	   
	if(errorstatus!=SD_OK)return errorstatus;   	   
 	SDMMC_Send_Data_Cfg(SD_DATATIMEOUT,nblks*blksize,9,0);	//blksize,块大小恒为512字节,控制器到卡	  
	SDMMC1->CMD|=1<<6;										//CMDTRANS=1,产生一个数据传输命令
	timeout=SDMMC_DATATIMEOUT; 
	INTX_DISABLE();//关闭总中断(POLLING模式,严禁中断打断SDMMC读写操作!!!)
	while(!(SDMMC1->STA&((1<<4)|(1<<1)|(1<<8)|(1<<3))))//下溢/CRC/数据结束/超时
	{
		if(SDMMC1->STA&(1<<14))							//发送区半空,表示至少存了8字(32字节)
		{	  
			if((tlen-bytestransferred)<SD_HALFFIFOBYTES)//不够32字节了
			{
				restwords=((tlen-bytestransferred)%4==0)?((tlen-bytestransferred)/4):((tlen-bytestransferred)/4+1);
				for(count=0;count<restwords;count++,tempbuff++,bytestransferred+=4)
				{
					SDMMC1->FIFO=*tempbuff;
				}
			}else 										//发送区半空,可以发送至少8字(32字节)数据
			{
				for(count=0;count<SD_HALFFIFO;count++)
				{
					SDMMC1->FIFO=*(tempbuff+count);
				}
				tempbuff+=SD_HALFFIFO;
				bytestransferred+=SD_HALFFIFOBYTES;
			}
			timeout=0X3FFFFFFF;		//写数据溢出时间
		}else
		{
			if(timeout==0)return SD_DATA_TIMEOUT; 
			timeout--;
		}
	} 
	SDMMC1->CMD&=~(1<<6);			//CMDTRANS=0,结束数据传输
	INTX_ENABLE();					//开启总中断
	if(SDMMC1->STA&(1<<3))			//数据超时错误
	{										   
		SDMMC1->ICR|=1<<3; 			//清错误标志
		return SD_DATA_TIMEOUT;
	}else if(SDMMC1->STA&(1<<1))	//数据块CRC错误
	{
		SDMMC1->ICR|=1<<1; 			//清错误标志
		if(nblks>1)					//针对可能出现的CRC错误,如果是多块读取,必须发送结束传输命令!
		{
			SDMMC_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);		//发送CMD12+结束传输 	   
			errorstatus=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);//等待R1响应   
		}	
		return SD_DATA_CRC_FAIL;		   
	}else if(SDMMC1->STA&(1<<4)) 	//接收fifo下溢错误
	{
		SDMMC1->ICR|=1<<4; 			//清错误标志
		return SD_TX_UNDERRUN;		 
	}
	if((SDMMC1->STA&(1<<8))&&(nblks>1))//多块发送结束,发送结束指令
	{
		if((STD_CAPACITY_SD_CARD_V1_1==CardType)||(STD_CAPACITY_SD_CARD_V2_0==CardType)||(HIGH_CAPACITY_SD_CARD==CardType))
		{
			SDMMC_Send_Cmd(SD_CMD_STOP_TRANSMISSION,1,0);		//发送CMD12+结束传输 	   
			errorstatus=CmdResp1Error(SD_CMD_STOP_TRANSMISSION);//等待R1响应   
			if(errorstatus!=SD_OK)return errorstatus;	 
		}
	}
	SDMMC1->ICR=0X1FE00FFF;		//清除所有标记	  
 	errorstatus=IsCardProgramming(&cardstate);
 	while((errorstatus==SD_OK)&&((cardstate==SD_CARD_PROGRAMMING)||(cardstate==SD_CARD_RECEIVING)))
	{
		errorstatus=IsCardProgramming(&cardstate);
	}   
	return errorstatus;
}  
//检查CMD0的执行状态
//返回值:sd卡错误码
SD_Error CmdError(void)
{
	SD_Error errorstatus = SD_OK;
	u32 timeout=SDMMC_CMD0TIMEOUT;	   
	while(timeout--)
	{
		if(SDMMC1->STA&(1<<7))break;//命令已发送(无需响应)	 
	}	    
	if(timeout==0)return SD_CMD_RSP_TIMEOUT;  
	SDMMC1->ICR=0X1FE00FFF;			//清除标记
	return errorstatus;
}	 
//检查R7响应的错误状态
//返回值:sd卡错误码
SD_Error CmdResp7Error(void)
{
	SD_Error errorstatus=SD_OK;
	u32 status;
	u32 timeout=SDMMC_CMD0TIMEOUT;
 	while(timeout--)
	{
		status=SDMMC1->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;//CRC错误/命令响应超时/已经收到响应(CRC校验成功)	
	}
 	if((timeout==0)||(status&(1<<2)))	//响应超时
	{																				    
		errorstatus=SD_CMD_RSP_TIMEOUT;	//当前卡不是2.0兼容卡,或者不支持设定的电压范围
		SDMMC1->ICR|=1<<2;				//清除命令响应超时标志
		return errorstatus;
	}	 
	if(status&1<<6)						//成功接收到响应
	{								   
		errorstatus=SD_OK;
		SDMMC1->ICR|=1<<6;				//清除响应标志
 	}
	return errorstatus;
}	   
//检查R1响应的错误状态
//cmd:当前命令
//返回值:sd卡错误码
SD_Error CmdResp1Error(u8 cmd)
{	  
   	u32 status; 
	while(1)
	{
		status=SDMMC1->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;//CRC错误/命令响应超时/已经收到响应(CRC校验成功)
	} 
	if(status&(1<<2))					//响应超时
	{																				    
 		SDMMC1->ICR=1<<2;				//清除命令响应超时标志
		SDMMC1->ICR=0X1FE00FFF;				//清除标记
		return SD_CMD_RSP_TIMEOUT;
	}	
 	if(status&(1<<0))					//CRC错误
	{																				    
 		SDMMC1->ICR=1<<0;				//清除标志
		return SD_CMD_CRC_FAIL;
	}		
	if(SDMMC1->RESPCMD!=cmd)return SD_ILLEGAL_CMD;//命令不匹配 
  	SDMMC1->ICR=0X1FE00FFF;				//清除标记
	return (SD_Error)(SDMMC1->RESP1&SD_OCR_ERRORBITS);//返回卡响应
}
//检查R3响应的错误状态
//返回值:错误状态
SD_Error CmdResp3Error(void)
{
	u32 status;						 
 	while(1)
	{
		status=SDMMC1->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;//CRC错误/命令响应超时/已经收到响应(CRC校验成功)	
	}
 	if(status&(1<<2))					//响应超时
	{											 
		SDMMC1->ICR|=1<<2;				//清除命令响应超时标志
		return SD_CMD_RSP_TIMEOUT;
	}	 
   	SDMMC1->ICR=0X1FE00FFF;				//清除标记
 	return SD_OK;								  
}
//检查R2响应的错误状态
//返回值:错误状态
SD_Error CmdResp2Error(void)
{
	SD_Error errorstatus=SD_OK;
	u32 status;
	u32 timeout=SDMMC_CMD0TIMEOUT;
 	while(timeout--)
	{
		status=SDMMC1->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;//CRC错误/命令响应超时/已经收到响应(CRC校验成功)	
	}
  	if((timeout==0)||(status&(1<<2)))	//响应超时
	{																				    
		errorstatus=SD_CMD_RSP_TIMEOUT; 
		SDMMC1->ICR|=1<<2;				//清除命令响应超时标志
		return errorstatus;
	}	 
	if(status&1<<0)						//CRC错误
	{								   
		errorstatus=SD_CMD_CRC_FAIL;
		SDMMC1->ICR|=1<<0;				//清除响应标志
 	}
	SDMMC1->ICR=0X1FE00FFF;				//清除标记
 	return errorstatus;								    		 
} 
//检查R6响应的错误状态
//cmd:之前发送的命令
//prca:卡返回的RCA地址
//返回值:错误状态
SD_Error CmdResp6Error(u8 cmd,u16*prca)
{
	SD_Error errorstatus=SD_OK;
	u32 status;					    
	u32 rspr1;
 	while(1)
	{
		status=SDMMC1->STA;
		if(status&((1<<0)|(1<<2)|(1<<6)))break;//CRC错误/命令响应超时/已经收到响应(CRC校验成功)	
	}
	if(status&(1<<2))					//响应超时
	{																				    
 		SDMMC1->ICR|=1<<2;				//清除命令响应超时标志
		return SD_CMD_RSP_TIMEOUT;
	}	 	 
	if(status&1<<0)						//CRC错误
	{								   
		SDMMC1->ICR|=1<<0;				//清除响应标志
 		return SD_CMD_CRC_FAIL;
	}
	if(SDMMC1->RESPCMD!=cmd)			//判断是否响应cmd命令
	{
 		return SD_ILLEGAL_CMD; 		
	}	    
	SDMMC1->ICR=0X1FE00FFF;				//清除所有标记
	rspr1=SDMMC1->RESP1;				//得到响应 	 
	if(SD_ALLZERO==(rspr1&(SD_R6_GENERAL_UNKNOWN_ERROR|SD_R6_ILLEGAL_CMD|SD_R6_COM_CRC_FAILED)))
	{
		*prca=(u16)(rspr1>>16);			//右移16位得到,rca
		return errorstatus;
	}
   	if(rspr1&SD_R6_GENERAL_UNKNOWN_ERROR)return SD_GENERAL_UNKNOWN_ERROR;
   	if(rspr1&SD_R6_ILLEGAL_CMD)return SD_ILLEGAL_CMD;
   	if(rspr1&SD_R6_COM_CRC_FAILED)return SD_COM_CRC_FAILED;
	return errorstatus;
}

//SDMMC使能宽总线模式
//enx:0,不使能;1,使能;
//返回值:错误状态
SD_Error SDEnWideBus(u8 enx)
{
	SD_Error errorstatus = SD_OK;
 	u32 scr[2]={0,0};
	u8 arg=0X00;
	if(enx)arg=0X02;
	else arg=0X00;
 	if(SDMMC1->RESP1&SD_CARD_LOCKED)return SD_LOCK_UNLOCK_FAILED;//SD卡处于LOCKED状态		    
 	errorstatus=FindSCR(RCA,scr);						//得到SCR寄存器数据
 	if(errorstatus!=SD_OK)return errorstatus;
	if((scr[1]&SD_WIDE_BUS_SUPPORT)!=SD_ALLZERO)		//支持宽总线
	{
	 	SDMMC_Send_Cmd(SD_CMD_APP_CMD,1,(u32)RCA<<16);	//发送CMD55+RCA,短响应											  
	 	errorstatus=CmdResp1Error(SD_CMD_APP_CMD);
	 	if(errorstatus!=SD_OK)return errorstatus; 
	 	SDMMC_Send_Cmd(SD_CMD_APP_SD_SET_BUSWIDTH,1,arg);//发送ACMD6,短响应,参数:10,4位;00,1位.											  
		errorstatus=CmdResp1Error(SD_CMD_APP_SD_SET_BUSWIDTH);
		return errorstatus;
	}else return SD_REQUEST_NOT_APPLICABLE;				//不支持宽总线设置 	 
}												   
//检查卡是否正在执行写操作
//pstatus:当前状态.
//返回值:错误代码
SD_Error IsCardProgramming(u8 *pstatus)
{
 	vu32 respR1 = 0, status = 0; 
  	SDMMC_Send_Cmd(SD_CMD_SEND_STATUS,1,(u32)RCA<<16);		//发送CMD13 	   
  	status=SDMMC1->STA;
	while(!(status&((1<<0)|(1<<6)|(1<<2))))status=SDMMC1->STA;//等待操作完成
   	if(status&(1<<0))			//CRC检测失败
	{
		SDMMC1->ICR|=1<<0;		//清除错误标记
		return SD_CMD_CRC_FAIL;
	}
   	if(status&(1<<2))			//命令超时 
	{
		SDMMC1->ICR|=1<<2;		//清除错误标记
		return SD_CMD_RSP_TIMEOUT;
	}
 	if(SDMMC1->RESPCMD!=SD_CMD_SEND_STATUS)return SD_ILLEGAL_CMD;
	SDMMC1->ICR=0X1FE00FFF;		//清除所有标记
	respR1=SDMMC1->RESP1;
	*pstatus=(u8)((respR1>>9)&0x0000000F);
	return SD_OK;
}
//读取当前卡状态
//pcardstatus:卡状态
//返回值:错误代码
SD_Error SD_SendStatus(uint32_t *pcardstatus)
{
	SD_Error errorstatus = SD_OK;
	if(pcardstatus==NULL)
	{
		errorstatus=SD_INVALID_PARAMETER;
		return errorstatus;
	}
 	SDMMC_Send_Cmd(SD_CMD_SEND_STATUS,1,RCA<<16);	//发送CMD13,短响应		 
	errorstatus=CmdResp1Error(SD_CMD_SEND_STATUS);	//查询响应状态 
	if(errorstatus!=SD_OK)return errorstatus;
	*pcardstatus=SDMMC1->RESP1;//读取响应值
	return errorstatus;
} 
//返回SD卡的状态
//返回值:SD卡状态
SDCardState SD_GetState(void)
{
	u32 resp1=0;
	if(SD_SendStatus(&resp1)!=SD_OK)return SD_CARD_ERROR;
	else return (SDCardState)((resp1>>9) & 0x0F);
}
//查找SD卡的SCR寄存器值
//rca:卡相对地址
//pscr:数据缓存区(存储SCR内容)
//返回值:错误状态		   
SD_Error FindSCR(u16 rca,u32 *pscr)
{  
	SD_Error errorstatus = SD_OK;
	u32 tempscr[2]={0,0};  
 	SDMMC_Send_Cmd(SD_CMD_SET_BLOCKLEN,1,8);			//发送CMD16,短响应,设置Block Size为8字节											  
 	errorstatus=CmdResp1Error(SD_CMD_SET_BLOCKLEN);
 	if(errorstatus!=SD_OK)return errorstatus;	    
  	SDMMC_Send_Cmd(SD_CMD_APP_CMD,1,(u32)rca<<16);	//发送CMD55,短响应 									  
 	errorstatus=CmdResp1Error(SD_CMD_APP_CMD);
 	if(errorstatus!=SD_OK)return errorstatus;
	SDMMC_Send_Data_Cfg(SD_DATATIMEOUT,8,3,1);		//8个字节长度,block为8字节,SD卡到SDMMC.
   	SDMMC_Send_Cmd(SD_CMD_SD_APP_SEND_SCR,1,0);		//发送ACMD51,短响应,参数为0											  
 	errorstatus=CmdResp1Error(SD_CMD_SD_APP_SEND_SCR);
 	if(errorstatus!=SD_OK)return errorstatus;							   
 	while(!(SDMMC1->STA&(SDMMC_STA_RXOVERR|SDMMC_STA_DCRCFAIL|SDMMC_STA_DTIMEOUT|SDMMC_STA_DBCKEND|SDMMC_STA_DATAEND)))
	{ 
		if(!(SDMMC1->STA&(1<<19)))		//接收FIFO数据可用
		{
			tempscr[0]=SDMMC1->FIFO;	//读取FIFO内容 
			tempscr[1]=SDMMC1->FIFO;	//读取FIFO内容 
			break;
		}
	}
 	if(SDMMC1->STA&(1<<3))		//接收数据超时
	{										 
 		SDMMC1->ICR|=1<<3;		//清除标记
		return SD_DATA_TIMEOUT;
	}else if(SDMMC1->STA&(1<<1))//已发送/接收的数据块CRC校验错误
	{
 		SDMMC1->ICR|=1<<1;		//清除标记
		return SD_DATA_CRC_FAIL;   
	}else if(SDMMC1->STA&(1<<5))//接收FIFO溢出
	{
 		SDMMC1->ICR|=1<<5;		//清除标记
		return SD_RX_OVERRUN;   	   
	} 
   	SDMMC1->ICR=0X1FE00FFF;		//清除标记	 
	//把数据顺序按8位为单位倒过来.   	
	*(pscr+1)=((tempscr[0]&SD_0TO7BITS)<<24)|((tempscr[0]&SD_8TO15BITS)<<8)|((tempscr[0]&SD_16TO23BITS)>>8)|((tempscr[0]&SD_24TO31BITS)>>24);
	*(pscr)=((tempscr[1]&SD_0TO7BITS)<<24)|((tempscr[1]&SD_8TO15BITS)<<8)|((tempscr[1]&SD_16TO23BITS)>>8)|((tempscr[1]&SD_24TO31BITS)>>24);
 	return errorstatus;
}  
//读SD卡
//buf:读数据缓存区
//sector:扇区地址
//cnt:扇区个数	
//返回值:错误状态;0,正常;其他,错误代码;				  				 
u8 SD_ReadDisk(u8*buf,u32 sector,u32 cnt)
{
	u8 sta=SD_OK;
	long long lsector=sector;
	u32 n;
	if(CardType!=STD_CAPACITY_SD_CARD_V1_1)lsector<<=9;
	if((u32)buf%4!=0)
	{
	 	for(n=0;n<cnt;n++)
		{
		 	sta=SD_ReadBlocks(SDMMC_DATA_BUFFER,lsector+512*n,512,1);//单个sector的读操作
			memcpy(buf,SDMMC_DATA_BUFFER,512);
			buf+=512;
		} 
	}else sta=SD_ReadBlocks(buf,lsector,512,cnt);	//单个/多个sector   
	return sta;
}
//写SD卡
//buf:写数据缓存区
//sector:扇区地址
//cnt:扇区个数	
//返回值:错误状态;0,正常;其他,错误代码;	
u8 SD_WriteDisk(u8*buf,u32 sector,u32 cnt)
{
	u8 sta=SD_OK;
	u32 n;
	long long lsector=sector;
	if(CardType!=STD_CAPACITY_SD_CARD_V1_1)lsector<<=9;
	if((u32)buf%4!=0)
	{
	 	for(n=0;n<cnt;n++)
		{
			memcpy(SDMMC_DATA_BUFFER,buf,512);
		 	sta=SD_WriteBlocks(SDMMC_DATA_BUFFER,lsector+512*n,512,1);//单个sector的写操作
			buf+=512;
		} 
	}else sta=SD_WriteBlocks(buf,lsector,512,cnt);	//单个/多个sector   
	return sta;
}







