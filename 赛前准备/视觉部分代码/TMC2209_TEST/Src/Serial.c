#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdarg.h>

extern UART_HandleTypeDef huart5;

uint8_t Serial_RxData;
uint8_t Serial_RxFlag;

/* ---- 帧解析状态机 (中断上下文中运行) ---- */
typedef enum { WAIT_AA, WAIT_55, WAIT_DATA } UartState_t;
static UartState_t uart_state = WAIT_AA;
static uint8_t uart_buf[6];
static uint8_t uart_idx;
static uint32_t uart_x;	
static uint32_t uart_y;
static uint8_t uart_frame_ready;

void Serial_Init(void)
{
	HAL_UART_Receive_IT(&huart5, &Serial_RxData, 1);
}

void Serial_SendByte(uint8_t Byte)
{
	HAL_UART_Transmit(&huart5, &Byte, 1, HAL_MAX_DELAY);
}

void Serial_SendArray(uint8_t *Array, uint16_t Length)
{
	uint16_t i;
	for (i = 0; i < Length; i ++)
	{
		Serial_SendByte(Array[i]);
	}
}

void Serial_SendString(char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i ++)
	{
		Serial_SendByte(String[i]);
	}
}

uint32_t Serial_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y --)
	{
		Result *= X;
	}
	return Result;
}

void Serial_SendNumber(uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i ++)
	{
		Serial_SendByte(Number / Serial_Pow(10, Length - i - 1) % 10 + '0');
	}
}

int fputc(int ch, FILE *f)
{
	Serial_SendByte(ch);
	return ch;
}

void Serial_Printf(char *format, ...)
{
	char String[100];
	va_list arg;
	va_start(arg, format);
	vsprintf(String, format, arg);
	va_end(arg);
	Serial_SendString(String);
}

uint8_t Serial_GetRxFlag(void)
{
	if (Serial_RxFlag == 1)
	{
		Serial_RxFlag = 0;
		return 1;
	}
	return 0;
}

uint8_t Serial_GetRxData(void)
{
	return Serial_RxData;
}

uint8_t Serial_IsFrameReady(void)
{
	if (uart_frame_ready)
	{
		uart_frame_ready = 0;
		return 1;
	}
	return 0;
}

uint32_t Serial_GetX(void)
{
	return uart_x;
}

uint32_t Serial_GetY(void)
{
	return uart_y;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == UART5)
	{
		uint8_t b = Serial_RxData;

		switch (uart_state)
		{
		case WAIT_AA:
			if (b == 0xAA) uart_state = WAIT_55;
			break;
		case WAIT_55:
			uart_state = (b == 0x55) ? WAIT_DATA : WAIT_AA;
			uart_idx = 0;
			break;
		case WAIT_DATA:
			uart_buf[uart_idx++] = b;
			if (uart_idx >= 6)
			{
				uart_state = WAIT_AA;
				uint32_t x = (uart_buf[0] << 8) | uart_buf[1];
				uint32_t y = (uart_buf[2] << 8) | uart_buf[3];
				uint32_t chk = (uart_buf[4] << 8) | uart_buf[5];
				if (chk == (uint32_t)(uart_buf[0] ^ uart_buf[1] ^ uart_buf[2] ^ uart_buf[3]))
				{
					uart_x = x;
					uart_y = y;
					uart_frame_ready = 1;
				}
			}
			break;
		}

		HAL_UART_Receive_IT(&huart5, &Serial_RxData, 1);
	}
}
