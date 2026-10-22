/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//															//																		//
// 工程：	MQTT_JX											//	注：在《esp_mqtt_proj》例程上修改									//
//															//																		//
// 平台：	【技新电子】物联网开发板 ESP8266 V1.0			//	①：添加【详实的注释】唉，不说了，说多了都是泪！！！				//
//															//																		//
// 功能：	①：设置MQTT相关参数							//	②：修改【MQTT参数数组】config.h -> device_id/mqtt_host/mqtt_pass	//
//															//																		//
//			②：与MQTT服务端，建立网络连接(TCP)				//	③：修改【MQTT_CLIENT_ID宏定义】mqtt_config.h -> MQTT_CLIENT_ID		//
//															//																		//
//			③：配置/发送【CONNECT】报文，连接MQTT服务端	//	④：修改【PROTOCOL_NAMEv31宏】mqtt_config.h -> PROTOCOL_NAMEv311	//
//															//																		//
//			④：订阅主题"SW_LED"							//	⑤：修改【心跳报文的发送间隔】mqtt.c ->	[mqtt_timer]函数			//
//															//																		//
//			⑤：向主题"SW_LED"发布"ESP8266_Online"			//	⑥：修改【SNTP服务器设置】user_main.c -> [sntpfn]函数				//
//															//																		//
//			⑥：根据接收到"SW_LED"主题的消息，控制LED亮灭	//	⑦：注释【遗嘱设置】user_main.c -> [user_init]函数					//
//															//																		//
//			⑦：每隔一分钟，向MQTT服务端发送【心跳】		//	⑧：添加【MQTT消息控制LED亮/灭】user_main.c -> [mqttDataCb]函数		//
//															//																		//
//	版本：	V1.1											//																		//
//															//																		//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// 头文件
//==============================
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "pwm.h"
#include "sntp.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"

//==============================

// 类型定义
//=================================
typedef unsigned long 		u32_t;
//=================================


// 全局变量
//============================================================================
MQTT_Client mqttClient;			// MQTT客户端_结构体【此变量非常重要】

static ETSTimer sntp_timer;		// SNTP定时器
uint16 light_adc,light_check=200,time;
char buf[100];
int day=1,night=0;
uint32	io_info[][3] = {{PWM_0_OUT_IO_MUX,PWM_0_OUT_IO_FUNC,PWM_0_OUT_IO_NUM},};
//============================================================================


// SNTP定时函数：获取当前网络时间
//============================================================================
void sntpfn()
{
    u32_t ts = 0;

    ts = sntp_get_current_timestamp();		// 获取当前的偏移时间

    os_printf("current time : %s\n", sntp_get_real_time(ts));	// 获取真实时间

    if (ts == 0)		// 网络时间获取失败
    {
        os_printf("did not get a valid time from sntp server\n");
    }
    else //(ts != 0)	// 网络时间获取成功
    {
            os_timer_disarm(&sntp_timer);	// 关闭SNTP定时器

            MQTT_Connect(&mqttClient);		// 开始MQTT连接
    }
}
//============================================================================

void light_on(void)
{
	pwm_set_duty(34000,0);
	pwm_start();
	os_delay_us(150000);
	pwm_set_duty(42000,0);
	pwm_start();
}

void light_off(void)
{
	pwm_set_duty(50000,0);
	pwm_start();
	os_delay_us(150000);
	pwm_set_duty(42000,0);
	pwm_start();
}
// WIFI连接状态改变：参数 = wifiStatus
//============================================================================
void wifiConnectCb(uint8_t status)
{
	// 成功获取到IP地址
	//---------------------------------------------------------------------
    if(status == STATION_GOT_IP)
    {
    	ip_addr_t * addr = (ip_addr_t *)os_zalloc(sizeof(ip_addr_t));

    	// 在官方例程的基础上，增加2个备用服务器
    	//---------------------------------------------------------------
    	sntp_setservername(0, "us.pool.ntp.org");	// 服务器_0【域名】
    	sntp_setservername(1, "ntp1.aliyun.com");	// 服务器_1【域名】

    	ipaddr_aton("210.72.145.44", addr);	// 点分十进制 => 32位二进制
    	sntp_setserver(2, addr);					// 服务器_2【IP地址】
    	os_free(addr);								// 释放addr

    	sntp_init();	// SNTP初始化


        // 设置SNTP定时器[sntp_timer]
        //-----------------------------------------------------------
        os_timer_disarm(&sntp_timer);
        os_timer_setfn(&sntp_timer, (os_timer_func_t *)sntpfn, NULL);
        os_timer_arm(&sntp_timer, 1000, 1);		// 1s定时
    }

    // IP地址获取失败
	//----------------------------------------------------------------
    else
    {
          MQTT_Disconnect(&mqttClient);	// WIFI连接出错，TCP断开连接
    }
}
//============================================================================


// MQTT已成功连接：ESP8266发送【CONNECT】，并接收到【CONNACK】
//============================================================================
void mqttConnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;	// 获取mqttClient指针

    INFO("MQTT: Connected\r\n");

    // 【参数2：主题过滤器 / 参数3：订阅Qos】
    //-----------------------------------------------------------------
	MQTT_Subscribe(client, "/YOUR_PRODUCT_KEY/switch_parlor/user/change", 0);	// 订阅主题"SW_LED"，QoS=0
//	MQTT_Subscribe(client, "SW_LED", 1);
//	MQTT_Subscribe(client, "SW_LED", 2);

	// 【参数2：主题名 / 参数3：发布消息的有效载荷 / 参数4：有效载荷长度 / 参数5：发布Qos / 参数6：Retain】
	//-----------------------------------------------------------------------------------------------------------------------------------------
	MQTT_Publish(client, "/YOUR_PRODUCT_KEY/switch_parlor/user/change", "ESP8266_Online", strlen("ESP8266_Online"), 0, 0);	// 向主题"SW_LED"发布"ESP8266_Online"，Qos=0、retain=0
//	MQTT_Publish(client, "SW_LED", "ESP8266_Online", strlen("ESP8266_Online"), 1, 0);
//	MQTT_Publish(client, "SW_LED", "ESP8266_Online", strlen("ESP8266_Online"), 2, 0);
    pwm_set_duty(42000,0);
    pwm_start();
}
//============================================================================

// MQTT成功断开连接
//============================================================================
void mqttDisconnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Disconnected\r\n");
}
//============================================================================

// MQTT成功发布消息
//============================================================================
void mqttPublishedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Published\r\n");
}
//============================================================================

// 【接收MQTT的[PUBLISH]数据】函数		【参数1：主题 / 参数2：主题长度 / 参数3：有效载荷 / 参数4：有效载荷长度】
//===============================================================================================================
void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
    char *topicBuf = (char*)os_zalloc(topic_len+1);		// 申请【主题】空间
    char *dataBuf  = (char*)os_zalloc(data_len+1);		// 申请【有效载荷】空间
    char *time;
    int i;
    u32_t ts = 0;


    MQTT_Client* client = (MQTT_Client*)args;	// 获取MQTT_Client指针


    os_memcpy(topicBuf, topic, topic_len);	// 缓存主题
    topicBuf[topic_len] = 0;				// 最后添'\0'

    os_memcpy(dataBuf, data, data_len);		// 缓存有效载荷
    dataBuf[data_len] = 0;					// 最后添'\0'

    INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);	// 串口打印【主题】【有效载荷】
    INFO("Topic_len = %d, Data_len = %d\r\n", topic_len, data_len);	// 串口打印【主题长度】【有效载荷长度】


// 【技小新】添加
//########################################################################################
    // 根据接收到的主题名/有效载荷，控制LED的亮/灭
    //-----------------------------------------------------------------------------------
    if( os_strcmp(topicBuf,"/YOUR_PRODUCT_KEY/switch_parlor/user/change") == 0 )			// 主题 == "change"
    {
    	//手动设置时间
    	if( os_strcmp(dataBuf,"{\"data\":\"10\"}") == 0 )
    		time = "10";
    	if( os_strcmp(dataBuf,"{\"data\":\"22\"}") == 0 )
    	    time = "22";
    	//开关灯命令
    	if( os_strcmp(dataBuf,"{\"data\":\"parlor_light_on\"}") == 0 )
    	{
    		light_on();
    		INFO("light is on\r\n");
    	}

    	if( os_strcmp(dataBuf,"{\"data\":\"parlor_light_off\"}") == 0 )
    	{
    	    light_off();
    	    INFO("light is off\r\n");
    	}
    	if( os_strcmp(dataBuf,"{\"data\":\"parlor_light_check\"}") == 0 )
    	{
    		light_check = system_adc_read();
    	    INFO("light_check is change\r\n");
    	}
    	if( os_strcmp(dataBuf,"{\"data\":\"get\"}") == 0 )		// 有效载荷 == "LED_ON"
    	{

    		light_adc = system_adc_read();
    		ts = sntp_get_current_timestamp();		// 获取当前的偏移时间
    		time = strtok(sntp_get_real_time(ts), " ");
    		for(i=0;i<3;i++)
    		{
    			time = strtok(NULL, " ");
    		}
    		time = strtok(time, ":");
    		INFO("time get %s\r\n",time);

    		if(light_adc>light_check)
    			MQTT_Publish(client, "/YOUR_PRODUCT_KEY/switch_parlor/user/change", "{\"light_parlor\":\"off\"}", strlen("{\"light_parlor\":\"off\"}"), 0, 0);	// 向主题"SW_LED"发布"ESP8266_Online"，Qos=0、retain=0
    		else
        		MQTT_Publish(client, "/YOUR_PRODUCT_KEY/switch_parlor/user/change", "{\"light_parlor\":\"on\"}", strlen("{\"light_parlor\":\"on\"}"), 0, 0);	// 向主题"SW_LED"发布"ESP8266_Online"，Qos=0、retain=0
    	}
    	if(os_strcmp(time,"22")==0 && day == 1)
    	{
    	    INFO("night is coming\r\n");
    	    day = 0;
    	    night = 1;
    	}
    	if(os_strcmp(time,"10")==0 && night == 1)
    	{
    	    INFO("day is coming\r\n");
    	    day = 1;
    	    night = 0;
    	    if(light_adc<light_check)
    	    	light_off();
    	}

    }
//#####################################################+


    os_free(topicBuf);	// 释放【主题】空间
    os_free(dataBuf);	// 释放【有效载荷】空间
}
//===============================================================================================================

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
 *******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

// user_init：entry of user application, init user function here
//===================================================================================================================
void user_init(void)
{
    uart_init(BIT_RATE_115200, BIT_RATE_115200);	// 串口波特率设为115200
    os_delay_us(60000);
    uint32_t duty[] = {33333};
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U,FUNC_GPIO14);//设置输出模式
    gpio_output_set(0,BIT14,BIT14,0);//gpio12输出低电平
    pwm_init(43000, duty, 1, io_info);
    pwm_start();

//【技小新】添加
//###########################################################################
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U,	FUNC_GPIO4);	// GPIO4输出高	#
	GPIO_OUTPUT_SET(GPIO_ID_PIN(4),1);						// LED初始化	#
//###########################################################################


    CFG_Load();	// 加载/更新系统参数【WIFI参数、MQTT参数】


    // 网络连接参数赋值：服务端域名【mqtt_test_jx.mqtt.iot.gz.baidubce.com】、网络连接端口【1883】、安全类型【0：NO_TLS】
	//-------------------------------------------------------------------------------------------------------------------
	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);

	// MQTT连接参数赋值：客户端标识符【..】、MQTT用户名【..】、MQTT密钥【..】、保持连接时长【120s】、清除会话【1：clean_session】
	//----------------------------------------------------------------------------------------------------------------------------
	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);

	// 设置遗嘱参数(如果云端没有对应的遗嘱主题，则MQTT连接会被拒绝)
	//--------------------------------------------------------------
//	MQTT_InitLWT(&mqttClient, "Will", "ESP8266_offline", 0, 0);


	// 设置MQTT相关函数
	//--------------------------------------------------------------------------------------------------
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);			// 设置【MQTT成功连接】函数的另一种调用方式
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);	// 设置【MQTT成功断开】函数的另一种调用方式
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);			// 设置【MQTT成功发布】函数的另一种调用方式
	MQTT_OnData(&mqttClient, mqttDataCb);					// 设置【接收MQTT数据】函数的另一种调用方式


	// 连接WIFI：SSID[..]、PASSWORD[..]、WIFI连接成功函数[wifiConnectCb]
	//--------------------------------------------------------------------------
	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);


	INFO("\r\nmy System started ...\r\n");
}
//===================================================================================================================
