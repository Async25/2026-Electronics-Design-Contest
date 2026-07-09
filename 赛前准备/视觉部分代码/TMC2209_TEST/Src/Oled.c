#include "Oled.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "font.h"
#include "main.h"



#define SCL_0    OLED_SCL_GPIO_Port->BSRR = OLED_SCL_Pin << 16
#define SCL_1    OLED_SCL_GPIO_Port->BSRR = OLED_SCL_Pin
#define SDA_0    OLED_SDA_GPIO_Port->BSRR = OLED_SDA_Pin << 16
#define SDA_1    OLED_SDA_GPIO_Port->BSRR = OLED_SDA_Pin



/*****************************************************************************
 ** È«¾Ö±äÁ¿
****************************************************************************/
typedef struct                  // OLED ÖØÒª²ÎÊı¼¯
{
    uint16_t width;			    // OLED ¿í¶È
    uint16_t height;		    // OLED ¸ß¶È
    uint8_t  FlagInit;          // ³õÊ¼»¯Íê³É±êÖ¾
} xOLED_TypeDef;
xOLED_TypeDef xLCD;	            // ¹ÜÀíLCDÖØÒª²ÎÊı



uint8_t OLED_GRAM[128][8];



/******************************************************************************
 * º¯  Êı£º delay_ms
 * ¹¦  ÄÜ£º ms ÑÓÊ±º¯Êı
 * ±¸  ×¢£º 1¡¢ÏµÍ³Ê±ÖÓ168MHz
 *          2¡¢´ò¹´£ºOptions/ c++ / One ELF Section per Function
            3¡¢±àÒëÓÅ»¯¼¶±ğ£ºLevel 3(-O3)
 * ²Î  Êı£º uint32_t  ms  ºÁÃëÖµ
 * ·µ»ØÖµ£º ÎŞ
 ******************************************************************************/
static volatile uint32_t ulTimesMS;    // Ê¹ÓÃvolatileÉùÃ÷£¬·ÀÖ¹±äÁ¿±»±àÒëÆ÷ÓÅ»¯
static void delay_ms(uint16_t ms)
{
    ulTimesMS = ms * 16500;
    while (ulTimesMS)
        ulTimesMS--;                   // ²Ù×÷Íâ²¿±äÁ¿£¬·ÀÖ¹¿ÕÑ­»·±»±àÒëÆ÷ÓÅ»¯µô
}



//ÑÓÊ±
static volatile uint32_t ulTimesUS;    // Ê¹ÓÃvolatileÉùÃ÷£¬·ÀÖ¹±äÁ¿±»±àÒëÆ÷ÓÅ»¯
static void delay(void)
{
    ulTimesUS = 15;
    while (ulTimesUS--);
}



//ÆğÊ¼ĞÅºÅ
static void start(void)
{
    SDA_1;
    SCL_1;
    delay();
    SDA_0;
    delay();
    SCL_0;
    delay();
}

//½áÊøĞÅºÅ
static void stop(void)
{
    SDA_0;
    SCL_1;
    delay();
    SDA_1;
}

//µÈ´ıĞÅºÅÏìÓ¦
static void waitACK(void)
{
    SDA_1;
    delay();
    SCL_1;
    delay();
    SCL_0;
    delay();
}

//Ğ´ÈëÒ»¸ö×Ö½Ú
static void sendByte(uint8_t dat)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (dat & 0x80)
        {
            SDA_1;
        }
        else
        {
            SDA_0;
        }
        delay();
        SCL_1;
        delay();
        SCL_0;
        dat <<= 1;
    }
}

// ·¢ËÍÃüÁî
static void writeCMD(uint8_t cmd)
{
    start();
    sendByte(0x78);  // µØÖ·
    waitACK();
    sendByte(0x00);  // ÃüÁî£º0x0
    waitACK();
    sendByte(cmd);   // ÃüÁîÖµ
    waitACK();
    stop();
}

// ·¢ËÍÊı¾İ
//static void writeData(uint8_t data)
//{
//    start();
//    sendByte(0x78);  // µØÖ·
//    waitACK();
//    sendByte(0x40);  // ÃüÁî£º0x1
//    waitACK();
//    sendByte(data);  // Êı¾İÖµ
//    waitACK();
//    stop();
//}



//»­µã£¬x:0~127£¬y:0~63£¬t:1 Ìî³ä 0,Çå¿Õ
static void drawPoint(uint8_t x, uint8_t y, uint8_t t)
{
    uint8_t i, m, n;
    i = y / 8;
    m = y % 8;
    n = 1 << m;
    if (t)
    {
        OLED_GRAM[x][i] |= n;
    }
    else
    {
        OLED_GRAM[x][i] = ~OLED_GRAM[x][i];
        OLED_GRAM[x][i] |= n;
        OLED_GRAM[x][i] = ~OLED_GRAM[x][i];
    }
}



/******************************************************************
 * º¯ÊıÃû£º OLED_Init
 * ¹¦  ÄÜ£º OLEDµÄ³õÊ¼»¯,
 * ²Î  Êı£º ÎŞ
 * ·µ  »Ø:  ÎŞ
 *****************************************************************/
void OLED_Init(void)
{
		GPIO_InitTypeDef    GPIO_InitStruct = {0}; 
    // Ê¹ÄÜSCLÒı½Å¶Ë¿ÚÊ±ÖÓ
    if (OLED_SCL_GPIO_Port == GPIOA)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN ;
    if (OLED_SCL_GPIO_Port == GPIOB)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN ;
    if (OLED_SCL_GPIO_Port == GPIOC)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN ;
    if (OLED_SCL_GPIO_Port == GPIOD)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN ;
    if (OLED_SCL_GPIO_Port == GPIOE)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN ;
    if (OLED_SCL_GPIO_Port == GPIOF)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN ;
    if (OLED_SCL_GPIO_Port == GPIOG)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN ;
    // Ê¹ÄÜSDAÒı½Å¶Ë¿ÚÊ±ÖÓ
    if (OLED_SDA_GPIO_Port == GPIOA)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN ;
    if (OLED_SDA_GPIO_Port == GPIOB)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN ;
    if (OLED_SDA_GPIO_Port == GPIOC)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN ;
    if (OLED_SDA_GPIO_Port == GPIOD)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN ;
    if (OLED_SDA_GPIO_Port == GPIOE)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN ;
    if (OLED_SDA_GPIO_Port == GPIOF)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN ;
    if (OLED_SDA_GPIO_Port == GPIOG)      RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN ;

		// ³õÊ¼»¯SCLÒı½Å
    GPIO_InitStruct.Pin   = OLED_SCL_Pin ;                      // Òı½Å±àºÅ
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;                // ¹¤×÷Ä£Ê½
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                        // ÉÏÏÂÀ­
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;          // Òı½ÅËÙÂÊ
    HAL_GPIO_Init(OLED_SCL_GPIO_Port, &GPIO_InitStruct);             // ³õÊ¼»¯
    // ³õÊ¼»¯SDAÒı½Å
    GPIO_InitStruct.Pin   = OLED_SDA_Pin;                       // Òı½Å±àºÅ
    HAL_GPIO_Init(OLED_SDA_GPIO_Port, &GPIO_InitStruct);             // ³õÊ¼»¯
	
    delay_ms(100);

    writeCMD(0xAE); //--turn off oled panel
    writeCMD(0x00); //---set low column address
    writeCMD(0x10); //---set high column address
    writeCMD(0x40); //--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
    writeCMD(0x81); //--set contrast control register
    writeCMD(0xCF); // Set SEG Output Current Brightness
    writeCMD(0xA1); //--Set SEG/Column Mapping     0xa0×óÓÒ·´ÖÃ 0xa1Õı³£
    writeCMD(0xC8); //Set COM/Row Scan Direction   0xc0ÉÏÏÂ·´ÖÃ 0xc8Õı³£
    writeCMD(0xA6); //--set normal display
    writeCMD(0xA8); //--set multiplex ratio(1 to 64)
    writeCMD(0x3f); //--1/64 duty
    writeCMD(0xD3); //-set display offset	Shift Mapping RAM Counter (0x00~0x3F)
    writeCMD(0x00); //-not offset
    writeCMD(0xd5); //--set display clock divide ratio/oscillator frequency
    writeCMD(0x80); //--set divide ratio, Set Clock as 100 Frames/Sec
    writeCMD(0xD9); //--set pre-charge period
    writeCMD(0xF1); //Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
    writeCMD(0xDA); //--set com pins hardware configuration
    writeCMD(0x12);
    writeCMD(0xDB); //--set vcomh
    writeCMD(0x40); //Set VCOM Deselect Level
    writeCMD(0x20); //-Set Page Addressing Mode (0x00/0x01/0x02)
    writeCMD(0x02); //
    writeCMD(0x8D); //--set Charge Pump enable/disable
    writeCMD(0x14); //--set(0x10) disable
    writeCMD(0xA4); // Disable Entire Display On (0xa4/0xa5)
    writeCMD(0xA6); // Disable Inverse Display On (0xa6/a7)
    OLED_Clear();
    writeCMD(0xAF);

    xLCD.FlagInit = 1;
    xLCD.width = 128;
    xLCD.height = 64;
}



/******************************************************************
 * º¯ÊıÃû£º OLED_ColorTurn
 * ¹¦  ÄÜ£º ·´ÏÔº¯Êı
 * ²Î  Êı£º uint8_t i   0_Õı³£ÏÔÊ¾£¬1_·´É«ÏÔÊ¾
 * ·µ  »Ø£º ÎŞ
 *****************************************************************/
void OLED_ColorTurn(uint8_t i)
{
    if (i == 0)
    {
        writeCMD(0xA6);   // Õı³£ÏÔÊ¾
    }
    if (i == 1)
    {
        writeCMD(0xA7);   // ·´É«ÏÔÊ¾
    }
}



/******************************************************************
 * º¯ÊıÃû£º OLED_DisplayTurn
 * ¹¦  ÄÜ£º ÆÁÄ»Ğı×ª180¶È
 * ²Î  Êı£º uint8_t i   0_Õı³£ÏÔÊ¾£¬1_Ë®Æ½·´×ªÏÔÊ¾
 * ·µ  »Ø£º ÎŞ
 *****************************************************************/
void OLED_DisplayTurn(uint8_t i)
{
    if (i == 0)
    {
        writeCMD(0xC8); //Õı³£ÏÔÊ¾
        writeCMD(0xA1);
    }
    if (i == 1)
    {
        writeCMD(0xC0); //·´×ªÏÔÊ¾
        writeCMD(0xA0);
    }
}


/******************************************************************
 * º¯ÊıÃû£º OLED_Display
 * ¹¦  ÄÜ£º ¿ªÆôOLEDÏÔÊ¾
 * ²Î  Êı£º uint8_t   sw   0_¹Ø±ÕÏÔÊ¾£¬1_´ò¿ªÏÔÊ¾
 * ·µ  »Ø£º ÎŞ
 *****************************************************************/
void OLED_Display(uint8_t sw)
{
    writeCMD(0x8D);       // µçºÉ±ÃÊ¹ÄÜ
    if (sw)
    {
        writeCMD(0x14);   // ¿ªÆôµçºÉ±Ã
        writeCMD(0xAF);   // µãÁÁÆÁÄ»
    }
    else
    {
        writeCMD(0x10);   // ¹Ø±ÕµçºÉ±Ã
        writeCMD(0xAE);   // ¹Ø±ÕÆÁÄ»
    }
}



/******************************************************************
 * º¯ÊıÃû£º OLED_Refresh
 * ¹¦  ÄÜ£º ¸üĞÂÏÔ´æµ½OLED
 * ²Î  Êı£º ÎŞ
 * ·µ  »Ø£º ÎŞ
 *****************************************************************/
void OLED_Refresh(void)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        writeCMD(0xb0 + i); // ÉèÖÃĞĞÆğÊ¼µØÖ·
        writeCMD(0x00);     // ÉèÖÃµÍÁĞÆğÊ¼µØÖ·
        writeCMD(0x10);     // ÉèÖÃ¸ßÁĞÆğÊ¼µØÖ·
        start();
        sendByte(0x78);
        waitACK();
        sendByte(0x40);
        waitACK();
        for (uint8_t n = 0; n < 128; n++)
        {
            sendByte(OLED_GRAM[n][i]);
            waitACK();
        }
        stop();
    }
}



/******************************************************************
 * º¯ÊıÃû£º OLED_Clear
 * ¹¦  ÄÜ£º ÇåÆÁº¯Êı
 * ²Î  Êı£º ÎŞ
 * ·µ  »Ø£º ÎŞ
 *****************************************************************/
void OLED_Clear(void)
{
    memset(OLED_GRAM, 0, 8 * 128); // Çå³ı»º´æËùÓĞÊı¾İ
    OLED_Refresh();                // ¸üĞÂÏÔÊ¾
}



/******************************************************************
 * º¯ÊıÃû£º OLED_Line
 * ¹¦  ÄÜ£º »­Ö±Ïß
 * ²Î  Êı£º uint16_t x1     ÆğµãX×ø±ê
 *          uint16_t y1     ÆğµãY×ø±ê
 *          uint16_t x2     ÖÕµãX×ø±ê
 *          uint16_t y2     ÖÕµãY×ø±ê
 *          uint16_t color  ÑÕÉ«Öµ
 * ±¸  ×¢£º
 *****************************************************************/
void OLED_Line(uint16_t  x1, uint16_t  y1, uint16_t  x2, uint16_t  y2)
{
    uint16_t  t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    delta_x = x2 - x1;                        // ¼ÆËã×ø±êÔöÁ¿
    delta_y = y2 - y1;
    uRow = x1;
    uCol = y1;
    if (delta_x > 0)incx = 1;                 // ÉèÖÃµ¥²½·½Ïò
    else if (delta_x == 0)incx = 0;           // ´¹Ö±Ïß
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }
    if (delta_y > 0)incy = 1;
    else if (delta_y == 0)incy = 0;           // Ë®Æ½Ïß
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }
    if (delta_x > delta_y)distance = delta_x; // Ñ¡È¡»ù±¾ÔöÁ¿×ø±êÖá
    else distance = delta_y;
    for (t = 0; t <= distance + 1; t++)       // »­ÏßÊä³ö
    {
        drawPoint(uRow, uCol, 1);             // »­µã
        xerr += delta_x ;
        yerr += delta_y ;
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
    OLED_Refresh();  // ¸üĞÂÏÔ´æµ½OLED
}



/******************************************************************
 * º¯ÊıÃû£º OLED_Circle
 * ¹¦  ÄÜ£º ÔÚÖ¸¶¨Î»ÖÃ»­Ô²
 * ²Î  Êı£º uint16_t Xpos     X×ø±ê
 *          uint16_t Ypos     ÆğµãY×ø±ê
 *          uint16_t Radius   °ë¾¶
 * ±¸  ×¢£º
 *****************************************************************/
void OLED_Circle(uint16_t Xpos, uint16_t Ypos, uint16_t Radius)
{
    int16_t mx = Xpos, my = Ypos, x = 0, y = Radius;
    int16_t d = 1 - Radius;
    while (y > x)
    {
        drawPoint(x + mx, y + my, 1);
        drawPoint(-x + mx, y + my, 1);
        drawPoint(-x + mx, -y + my, 1);
        drawPoint(x + mx, -y + my, 1);
        drawPoint(y + mx, x + my, 1);
        drawPoint(-y + mx, x + my, 1);
        drawPoint(y + mx, -x + my, 1);
        drawPoint(-y + mx, -x + my, 1);
        if (d < 0)
        {
            d += 2 * x + 3;
        }
        else
        {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
    OLED_Refresh();  // ¸üĞÂÏÔ´æµ½OLED
}



/******************************************************************
 * º¯ÊıÃû£º OLED_ShowChinese
 * ¹¦  ÄÜ£º ÏÔÊ¾×ÔĞĞÈ¡Ä£µÄºº×Ö,
 *          ×Ö¿âÊı¾İÔÚfontÎÄ¼şÖĞ£¬Ö»ÊÊºÏÉÙÁ¿ºº×Ö¹Ì¶¨Êä³ö
 *          PCtoLCD2018È¡Ä££ºÒõÂë+ÁĞĞĞÊ½+ÄæÏò+C51¸ñÊ½
 * ²Î  Êı£º uint16_t  x         ×ø±êx
 *          uint16_t  y         ×ø±êy
 *          uint8_t   index     ×ÖÄ£Êı¾İÔÚÊı×éÖĞµÄĞòºÅ
 * ·µ  »Ø:  ÎŞ
 *****************************************************************/
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t num, uint8_t size1)
{
    uint8_t m, temp;
		uint8_t x0 = x;
		uint8_t y0 = y;
    uint16_t size3 = (size1 / 8 + ((size1 % 8) ? 1 : 0)) * size1; // ?õ?????h??????????????????????

    for (uint16_t i = 0; i < size3; i++)
    {
        if (size1 == 16)
        {
            temp = newChinese[num][i];   // ????16*16????
        }
        else if(size1 == 12)
        {
            temp = bluemoonChinese[num][i];   // ??????????S??????12*12????
        }
        for (m = 0; m < 8; m++)
        {
            if (temp & 0x01)
                drawPoint(x, y, 1);
            else
                drawPoint(x, y, 0);
            temp >>= 1;
            y++;
        }
        x++;
        if ((x - x0) == size1)
        {
            x = x0;
            y0 = y0 + 8;
        }
        y = y0;
    }
    OLED_Refresh();                          // ???????OLED
}



/******************************************************************
 * º¯ÊıÃû£º drawAscii
 * ¹¦  ÄÜ£º ÔÚÖ¸¶¨Î»ÖÃÏÔÊ¾Ò»¸ö×Ö·û
 * ²Î  Êı£º uint16_t x,y     ÆğÊ¼×ø±ê
 *          uint8_t  num     ÒªÏÔÊ¾µÄ×Ö·û:" "--->"~"
 *          uint8_t  size    ×ÖÌå´óĞ¡ 12/16/24/32
 *****************************************************************/
static void drawAscii(uint16_t x, uint16_t y, uint8_t num, uint8_t size)
{
    static uint8_t temp;
    static uint8_t csize;
    static uint16_t y0;

    y0 = y;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);   // µÃµ½×ÖÌåÒ»¸ö×Ö·û¶ÔÓ¦µãÕó¼¯ËùÕ¼µÄ×Ö½ÚÊı
    num = num - ' ';                                          // µÃµ½Æ«ÒÆºóµÄÖµ£¨ASCII×Ö¿âÊÇ´Ó¿Õ¸ñ¿ªÊ¼È¡Ä££¬ËùÒÔ-' '¾ÍÊÇ¶ÔÓ¦×Ö·ûµÄ×Ö¿â£©
    for (uint8_t t = 0; t < csize; t++)
    {
        if (size == 12)         temp = aFontASCII12[num][t];  // µ÷ÓÃ1206×ÖÌå
        else if (size == 16)    temp = aFontASCII16[num][t];  // µ÷ÓÃ1608×ÖÌå
        else if (size == 24)    temp = aFontASCII24[num][t];  // µ÷ÓÃ2412×ÖÌå
        else if (size == 32)    temp = aFontASCII32[num][t];  // µ÷ÓÃ3216×ÖÌå
        else return;                                          // Ã»ÓĞµÄ×Ö¿â

        for (uint8_t t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80) drawPoint(x, y,  1);             // ×ÖÌå »­µã
            else             drawPoint(x, y,  0);             // ±³¾° »­µã

            temp <<= 1;
            y++;
            if (y >= xLCD.height)    return;                  // ³¬³öÆÁÄ»¸ß¶È(µ×)
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                if (x >= xLCD.width) return;                  // ³¬³öÆÁÄ»¿í¶È(¿í)
                break;
            }
        }
    }
}



/******************************************************************
 * º¯ÊıÃû£º drawGBK
 * ¹¦  ÄÜ£º ÔÚÖ¸¶¨Î»ÖÃÏÔÊ¾Ò»¸ö×Ö·û
 * ²Î  Êı£º uint16_t x,y     ÆğÊ¼×ø±ê
 *          uint8_t  num     ÒªÏÔÊ¾µÄ×Ö·û:" "--->"~"
 *          uint8_t  size    ×ÖÌå´óĞ¡ 12/16/24/32
 *****************************************************************/
static void drawGBK(uint16_t x, uint16_t y, uint8_t *font, uint8_t size)
{
    static uint8_t temp;
    static uint16_t y0;
    static uint8_t GBK[128];
    static uint8_t csize;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size);   // µÃµ½×ÖÌåÒ»¸ö×Ö·û¶ÔÓ¦µãÕó¼¯ËùÕ¼µÄ×Ö½ÚÊı
    W25Q128_ReadFontData(font, size, GBK);                // µÃµ½ÏàÓ¦´óĞ¡µÄµãÕóÊı¾İ

    y0 = y;
    for (uint8_t t = 0; t < csize; t++)
    {
        temp = GBK[t];                                    // µÃµ½GBKµãÕóÊı¾İ
        for (uint8_t t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)   drawPoint(x, y, 1);
            else               drawPoint(x, y, 0);
            temp <<= 1;
            y++;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }
}



/******************************************************************************
 * º¯  Êı£º OLED_String
 * ¹¦  ÄÜ£º ÔÚLCDÉÏÏÔÊ¾×Ö·û´®(Ö§³ÖÓ¢ÎÄ¡¢ºº×Ö)
 * Ãè  Êö£º Ó¢ÎÄ£º×ÖÄ£Êı¾İ±£´æÔÚfont.h£¬±àÒëºóºÍ´úÂëÒ»Æğ±£´æÔÚĞ¾Æ¬ÄÚ²¿Flash
 *          ºº×Ö£º×ÖÄ£±£´æÔÚÍâ²¿FlashÖĞ£¬±¾º¯Êı×Ö¿âÔÚW25Q128ÖĞ
 *                Ä§Å®¿ª·¢°åÖĞW25Q128ÒÑÉÕÂ¼ËÎÌå4ÖÖ×ÖºÅ´óĞ¡×ÖÄ£Êı¾İ
 * ²Î  Êı£º uint16_t   x      ×ÖÌå×óÉÏ½ÇX×ø±ê
 *          uint16_t   y      ×ÖÌå×óÉÏ½Çy×ø±ê
 *          char*      pFont  ÒªÏÔÊ¾µÄ×Ö·û´®Êı¾İ
 *          uint8_t    size   ×ÖºÅ´óĞ¡£º12 16 24 32
 * ·µ»ØÖµ:  ÎŞ
 ******************************************************************************/
void OLED_String(uint16_t x, uint16_t y, char *pFont, uint8_t size)
{
    if (xLCD .FlagInit == 0) return;

    uint16_t xStart = x;

    if (size != 12 && size != 16 && size != 24 && size != 32) // ×ÖÌå´óĞ¡¿ØÖÆ
        size = 24;

    while (*pFont != 0)                            // Á¬Ğø¶ÁÈ¡×Ö·û´®Êı¾İ£¬Ö±µ½'\0'Ê±Í£Ö¹
    {
        if (x > (xLCD.width - size/2))               // ĞĞÎ»ÖÃÅĞ¶Ï£¬Èç¹ûµ½ÁËĞĞÄ©£¬¾Í°Ñ¹â±ê»»ĞĞ
        {
            x = xStart;
            y = y + size;
        }
        if (y > (xLCD.height - size))              // ÁĞÎ»ÖÃÅĞ¶Ï£¬Èç¹ûµ½ÁËÁĞÄ©£¬¾Í·µ»Ø£¬²»ÔÙÊä³ö
            return;

        if (*pFont < 127)                          // ASCII×Ö·û
        {
            drawAscii(x, y, *pFont, size);
            pFont++;
            x += size / 2;
        }
        else                                       // ºº×ÖÏÔÊ¾
        {
            // ÖØÒª: Èç¹ûÓÃµÄ²»ÊÇÄ§Å®¿ª·¢°åµÄ×Ö¿â, ¾ÍÒªĞŞ¸Ä»ò×¢ÊÍÏÂÃæÕâÒ»ĞĞ, ÕâÑù¾Í²»Ó°ÏìASCIIÓ¢ÎÄ×Ö·ûµÄÊä³ö
            drawGBK(x, y, (uint8_t *)pFont, size);
            pFont = pFont + 2;                     // ÏÂÒ»¸öÒªÏÔÊ¾µÄÊı¾İÔÚÄÚ´æÖĞµÄÎ»ÖÃ
            x = x + size;                          // ÏÂÒ»¸öÒªÏÔÊ¾µÄÊı¾İÔÚÆÁÄ»ÉÏµÄXÎ»ÖÃ
        }
    }
    OLED_Refresh();                                // ¸üĞÂÏÔ´æµ½OLED
}


void OLED_DrawBMP(int16_t x0, int16_t y0,uint8_t width, uint8_t height,float scale,const uint8_t *data)
{
    uint8_t x, y;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            uint8_t pixel = (data[(y / 8) * width + x] >> (y % 8)) & 0x01;

            if (pixel) // ????
            {
                // ??????
                float drawX0 = x0 + (x * scale);
                float drawY0 = y0 + (y * scale);

                if (scale >= 1.0f)
                {
                    // ??:?????
                    uint8_t s = (uint8_t)(scale + 0.5f); // ????
                    for (uint8_t dx = 0; dx < s; dx++)
                    {
                        for (uint8_t dy = 0; dy < s; dy++)
                        {
                            int16_t finalX = (int16_t)(drawX0 + dx);
                            int16_t finalY = (int16_t)(drawY0 + dy);

                            if (finalX >= 0 && finalX < 128 && finalY >= 0 && finalY < 64)
                                drawPoint(finalX, finalY, 1);
                        }
                    }
                }
                else
                {
                    // ??:???????(??)
                    int16_t finalX = (int16_t)(drawX0);
                    int16_t finalY = (int16_t)(drawY0);

                    if (finalX >= 0 && finalX < 128 && finalY >= 0 && finalY < 64)
                        drawPoint(finalX, finalY, 1);
                }
            }
        }
    }
    OLED_Refresh();
}

void OLED_fill(int16_t x0, int16_t y0, int16_t w, int16_t h)
{
    if (w <= 0 || h <= 0) return;

    int16_t x1 = x0 + w - 1;
    int16_t y1 = y0 + h - 1;

    if (x0 < 0)        x0 = 0;
    if (y0 < 0)        y0 = 0;
    if (x1 >= 128)  x1 = 127;
    if (y1 >= 64)  y1 =  63;
    if (x0 > x1 || y0 > y1) return;

    for (int16_t y = y0; y <= y1; y++)
    {
        uint8_t page = (uint8_t)(y >> 3);
        uint8_t bit  = (uint8_t)(1U << (y & 7));

        uint8_t mask = (uint8_t)~bit;
        for (int16_t x = x0; x <= x1; x++)
        {
            OLED_GRAM[x][page] &= mask;
        }
    }
}



void OLED_Num(uint16_t x, uint16_t y, int num, uint8_t width, uint8_t size)
{
    char buf[16];
    uint8_t i = 0;
    int absval = (num >= 0) ? num : -num;

    buf[i++] = (num >= 0) ? '+' : '-';

    for (int d = width; d > 0; d--)
    {
        int pow10 = 1;
        for (int k = 1; k < d; k++) pow10 *= 10;
        buf[i++] = (absval / pow10) % 10 + '0';
    }

    buf[i] = '\0';
    OLED_String(x, y, buf, size);
    OLED_Refresh();
}



void OLED_Float(uint16_t x, uint16_t y, float f, uint8_t decimals, uint8_t size)
{
    char buf[32];
    uint8_t i = 0;

    if (f >= 0) buf[i++] = '+';
    else { buf[i++] = '-'; f = -f; }

    int intPart = (int)f;
    char intbuf[16];
    int intlen = 0;
    do {
        intbuf[intlen++] = (intPart % 10) + '0';
        intPart /= 10;
    } while (intPart > 0);

    for (int j = intlen - 1; j >= 0; j--) buf[i++] = intbuf[j];

    if (decimals > 0) buf[i++] = '.';

    float frac = f - (int)f;
    for (int d = 0; d < decimals; d++)
    {
        frac *= 10;
        int digit = (int)frac;
        buf[i++] = digit + '0';
        frac -= digit;
    }

    buf[i] = '\0';
    OLED_String(x, y, buf, size);
    OLED_Refresh();
}


