#include "led.h"
#include "delay.h"
#include "key.h"
#include "sys.h"
#include "usart.h"
#include "usart2.h"
#include "timer.h"
#include "AHT20-21_DEMO_V1_3.h"
//#include "myiic.h"
#include "wkup.h"
#include "adc.h"
 
/************************************************
 ALIENTEK战舰STM32开发板实验9
 PWM输出实验  
 技术支持：www.openedv.com
 淘宝店铺：http://eboard.taobao.com 
 关注微信公众平台微信号："正点原子"，免费获取STM32资料。
 广州市星翼电子科技有限公司  
 作者：正点原子 @ALIENTEK
************************************************/
/*
 int main(void)
 {		
 	//u16 led0pwmval=0;
	//u8 dir=1;	
	delay_init();	    	 //延时函数初始化	  
	WKUP_Init();
	//IIC_Init();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); 	 //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
	uart_init(115200);	 //串口初始化为115200
 	LED_Init();			     //LED端口初始化
 	TIM4_PWM_Init(1999,719);	 //不分频。PWM频率=72000000/900=80Khz
	TIM_SetCompare4(TIM4,1850);
   	while(1)
	{
		//read_AHT20_once();
		delay_ms(2000);
	}	 
 }
*/
int data_send = 0;

int main(void)
{
	
	uint32_t CT_data[2];
	volatile int  c1,t1;
	float c,t;
	u16 adcx;
	/***********************************************************************************/
	/**///上电初始化SDA，SCL的IO口
	/***********************************************************************************/
	Init_I2C_Sensor_Port();
	Adc_Init();		  		//ADC初始化
	delay_init();	    	 //延时函数初始化	  
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); 	 //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
	uart_init(115200);	 //串口初始化为115200
	uart2_init(115200);
	esp_init();
	/***********************************************************************************/
	/**///①刚上电，产品芯片内部就绪需要时间，延时100~500ms,建议500ms
	/***********************************************************************************/
	Delay_1ms(500);
	/***********************************************************************************/
	/**///②上电第一次发0x71读取状态字，判断状态字是否为0x18,如果不是0x18,进行寄存器初始化
	/***********************************************************************************/
	if((AHT20_Read_Status()&0x18)!=0x18)
	{
	AHT20_Start_Init(); //重新初始化寄存器
	Delay_1ms(10);
	}
	
	/***********************************************************************************/
	/**///③根据客户自己需求发测量命令读取温湿度数据，当前while（1）循环发测量命令读取温湿度数据，仅供参考
	/***********************************************************************************/
	printf("system start \r\n");
	while(1)
	{
		
	//下一步客户处理显示数据，
	/***********************************************************************************/
	/**///延时2s，建议间隔2s读一次
	/***********************************************************************************/
	Delay_1ms(5000);
	data_send += 1;
	if(data_send > 60)
	{
		adcx=Get_Adc_Average(ADC_Channel_1,10);
		AHT20_Read_CTdata(CT_data);       //直接读取AHT20的温度和湿度数据  
	

		c1 = CT_data[0]*100*10/1024/1024;  //计算得到湿度值c1（放大了10倍）
		t1 = CT_data[1]*200*10/1024/1024-500;//计算得到温度值t1（放大了10倍）
		c=c1/10.0;
		t=t1/10.0;	
		
		printf("AT+MQTTPUB=0,\"/YOUR_PRODUCT_KEY/control_parlor/user/change\",\"{\\\"parlor_temp\\\":\\\"%.1f\\\"}\",0,0\r\n",t);
		delay_ms(30000);
		printf("AT+MQTTPUB=0,\"/YOUR_PRODUCT_KEY/control_parlor/user/change\",\"{\\\"parlor_hum\\\":\\\"%.1f\\\"}\",0,0\r\n",c);
		delay_ms(30000);
		printf("AT+MQTTPUB=0,\"/YOUR_PRODUCT_KEY/control_parlor/user/change\",\"{\\\"parlor_gas\\\":\\\"%d\\\"}\",0,0\r\n",adcx);
		delay_ms(30000);
		printf("AT+MQTTPUB=0,\"/YOUR_PRODUCT_KEY/control_parlor/user/change\",\"{\\\"parlor_data\\\":\\\"get\\\"}\",0,0\r\n");
		delay_ms(30000);
		
		data_send = 0;
	}
	
	if(data_get == 1)
	{
		adcx=Get_Adc_Average(ADC_Channel_1,10);
		AHT20_Read_CTdata(CT_data);       //不经过CRC校验，直接读取AHT20的温度和湿度数据    推荐每隔大于1S读一次
    //AHT20_Read_CTdata_crc(CT_data);  //crc校验后，读取AHT20的温度和湿度数据 
	

		c1 = CT_data[0]*100*10/1024/1024;  //计算得到湿度值c1（放大了10倍）
		t1 = CT_data[1]*200*10/1024/1024-500;//计算得到温度值t1（放大了10倍）
		c=c1/10.0;
		t=t1/10.0;	
		
		printf("AT+MQTTPUB=0,\"/YOUR_PRODUCT_KEY/control_parlor/user/change\",\"{\\\"parlor_temp\\\":\\\"%.1f\\\"}\",0,0\r\n",t);
		delay_ms(30000);
		printf("AT+MQTTPUB=0,\"/YOUR_PRODUCT_KEY/control_parlor/user/change\",\"{\\\"parlor_hum\\\":\\\"%.1f\\\"}\",0,0\r\n",c);
		delay_ms(30000);
		printf("AT+MQTTPUB=0,\"/YOUR_PRODUCT_KEY/control_parlor/user/change\",\"{\\\"parlor_gas\\\":\\\"%d\\\"}\",0,0\r\n",adcx);
		delay_ms(30000);
		printf("AT+MQTTPUB=0,\"/YOUR_PRODUCT_KEY/control_parlor/user/change\",\"{\\\"parlor_data\\\":\\\"get\\\"}\",0,0\r\n");
		delay_ms(30000);
		
		data_get = 0;
	}
	//printf("温度：%.3f\n",t);
	//printf("湿度：%.3f\n\r",c);
	//printf("烟雾浓度：%d\r\n",adcx);
	 }

}
