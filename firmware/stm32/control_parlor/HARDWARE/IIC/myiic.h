#ifndef __BSP_I2C_H
#define __BSP_I2C_H

#include "sys.h"
#include "delay.h"
#include "usart.h"
//  PB6,PB7
 
#define SDA_IN()  {GPIOB->CRH&=0XFFFF0FFF;GPIOB->CRH|=(u32)8<<12;}
#define SDA_OUT() {GPIOB->CRH&=0XFFFF0FFF;GPIOB->CRH|=(u32)3<<12;}
 
//IO2ù×÷oˉêy	 
#define IIC_SCL    PBout(10) //SCL
#define IIC_SDA    PBout(11) //SDA	 
#define READ_SDA   PBin(11)


//IIC?ùóD2ù×÷oˉêy
void IIC_Init(void);              		 
void IIC_Start(void);				
void IIC_Stop(void);	  		
void IIC_Send_Byte(u8 txd);			
u8 IIC_Read_Byte(unsigned char ack);
u8 IIC_Wait_Ack(void); 				
void IIC_Ack(void);					
void IIC_NAck(void);				
 
void IIC_WriteByte(uint16_t addr,uint8_t data,uint8_t device_addr);
uint16_t IIC_ReadByte(uint16_t addr,uint8_t device_addr,uint8_t ByteNumToRead);


void  read_AHT20_once(void);
void  reset_AHT20(void);
void  init_AHT20(void);	
void  startMeasure_AHT20(void);
void  read_AHT20(void);
uint8_t  Receive_ACK(void);
void  Send_ACK(void);
void  SendNot_Ack(void);
void I2C_WriteByte(uint8_t  input);
uint8_t I2C_ReadByte(void);	
void  set_AHT20sendOutData(void);
void  I2C_Start(void);
void  I2C_Stop(void);
#endif


