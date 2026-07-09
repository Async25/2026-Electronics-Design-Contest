#ifndef __BSP_OLED_H
#define __BSP_OLED_H 
/***********************************************************************************************************************************
 ** 【代码编写】  魔女开发板团队
 ** 【代码更新】
 ** 【淘    宝】  魔女开发板      https://demoboard.taobao.com
 ***********************************************************************************************************************************
 ** 【文件名称】  bsp_OLED.h
 **
 ** 【适用平台】  STM32F407 + KEIL5.31 + 0.96寸4线LOED
 **
 ** 【移植说明】  1- 本驱动(bsp_OLED.c、bsp_OLED.h，同时适用于标准库、HAL库工程.
 **               2- 复制本工程的OLED文件夹，到目标工程文件夹中;
 **               3- 把bsp_OLED.c文件添加到工程管理器;
 **               4- 添加bsp_OLED.h文件的头文件路径;
 **               5- 在需要串口功能的文件中，#include "bsp_OLED.h＂；
 **
 ** 【硬件重点】  VCC --- 3.3V , 建议不要用5.0V。
 **               GND --- GND
 **               SCL --- PB8   (可修改)
 **               SDA --- PB9   (可修改)
 **
 ** 【软件重点】  本代码使用模拟I2C，所用引脚无需带I2C功能。
 **               OLED的显存, 存放格式如下.
 **               [0]0 1 2 3 ... 127
 **               [1]0 1 2 3 ... 127
 **               [2]0 1 2 3 ... 127
 **               [3]0 1 2 3 ... 127
 **               [4]0 1 2 3 ... 127
 **               [5]0 1 2 3 ... 127
 **               [6]0 1 2 3 ... 127
 **               [7]0 1 2 3 ... 127
 **
 ** 【更新记录】  
 **               2023-01-10   更新至F407
 **
 ** 【备注说明】  版权归魔女科技所有，限于学习测试，切勿商用，谢谢！
 **               https://demoboard.taobao.com
************************************************************************************************************************************/
#ifdef USE_STDPERIPH_DRIVER                 // 标准库
    #include "stm32f4xx.h"                 // 标准库的寄存器定义 
    #include "bsp_W25Q128.h"               // 外部Flash存储，存储有字库数据
#endif

#ifdef USE_HAL_DRIVER                      // 使用HAL库
    #include "stm32f4xx_hal.h"             // HAL库的寄存器定义 
    #include "bsp_W25Q128.h"               // 外部Flash存储，存储有字库数据
#endif





/*****************************************************************************
 ** 引脚定义
 ** 本代码使用模拟I2C，可以使用普通引脚，而非必须有I2C功能的引脚。
****************************************************************************/





/*****************************************************************************
 ** 声明全局函数
****************************************************************************/
void OLED_Init(void);                                                   // 初始化
void OLED_Refresh(void);                                                // 更新显存到OLED
void OLED_DisplayTurn(uint8_t i);                                       // 显示方向水平反转
void OLED_Display(uint8_t sw);                                          // 显示电源开关
void OLED_Clear(void);                                                  // 清屏
void OLED_Line(uint16_t  x1, uint16_t  y1, uint16_t  x2, uint16_t  y2); // 画线
void OLED_Circle(uint16_t Xpos, uint16_t Ypos, uint16_t Radius);        // 画圆
void OLED_String(uint16_t x, uint16_t y, char *pFont, uint8_t size);    // 显示中英文字符号
void OLED_ShowChinese(uint8_t x,uint8_t y,uint8_t num,uint8_t size1);   // 显示汉字(字模数据在font.h)
void OLED_fill(int16_t x0, int16_t y0, int16_t w, int16_t h);
void OLED_DrawBMP(int16_t x0, int16_t y0,uint8_t width, uint8_t height,float scale,const uint8_t *data);
void OLED_Num(uint16_t x, uint16_t y, int num, uint8_t width, uint8_t size);
void OLED_Float(uint16_t x, uint16_t y, float f, uint8_t decimals, uint8_t size);

#endif

