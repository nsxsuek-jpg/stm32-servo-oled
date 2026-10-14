#include "oled.h"
#include "oled_font.h"
#include "delay.h"        /* OLED_Init() 里的上电延时用 Delay_ms()（需先 Delay_Init()）*/

#define OLED_SCL_PIN   GPIO_Pin_7
#define OLED_SDA_PIN   GPIO_Pin_6
#define OLED_PORT      GPIOA

/* 每个 us 的空转次数：这个循环一次约 6 条指令，
   72MHz 下取 24 大约 2us，SCL 约 50kHz。
   刻意留慢一点：杜邦线长/模块没焊上拉时也不容易出错，
   全屏 1KB 刷新仍只要 10ms 出头。 */
#define OLED_US_LOOPS  24

/* ---------------- 显示方向 ----------------
   SSD1306 的两个方向控制命令：
     段重映射 SEG : 0xA1 = 左右镜像, 0xA0 = 不镜像
     扫描方向 COM : 0xC8 = 上下翻转, 0xC0 = 不翻转

   出厂默认（江协科技那套驱动）是 SEG=1, COM=1。
   这里改成 0/0，等于把画面整体旋转 180 度，
   对应模块倒着装（接头从上面变成下面）的情况。

   烧进去以后如果还不对，按现象改下面两个宏：
     现象                          怎么改
     --------------------------    ------------------------
     显示正常了                    就这样，不用动
     左右反了（字从右往左读）       OLED_SEG_REMAP 改回 1
     上下还是反的                   OLED_COM_REMAP 改回 1
     想回到出厂默认方向            两个都改回 1
   ------------------------------------------ */
#define OLED_SEG_REMAP  0
#define OLED_COM_REMAP  0

#define OLED_CMD_SEG_REMAP  (OLED_SEG_REMAP ? 0xA1 : 0xA0)
#define OLED_CMD_COM_REMAP  (OLED_COM_REMAP ? 0xC8 : 0xC0)

static void OLED_DelayUs(uint32_t us)
{
    while (us--) {
        for (volatile uint8_t i = 0; i < OLED_US_LOOPS; i++);
    }
}

static void OLED_I2C_Start(void)
{
    GPIO_SetBits(OLED_PORT, OLED_SDA_PIN);
    GPIO_SetBits(OLED_PORT, OLED_SCL_PIN);
    OLED_DelayUs(2);
    GPIO_ResetBits(OLED_PORT, OLED_SDA_PIN);
    OLED_DelayUs(2);
    GPIO_ResetBits(OLED_PORT, OLED_SCL_PIN);
    OLED_DelayUs(2);
}

static void OLED_I2C_Stop(void)
{
    GPIO_ResetBits(OLED_PORT, OLED_SCL_PIN);
    GPIO_ResetBits(OLED_PORT, OLED_SDA_PIN);
    OLED_DelayUs(2);
    GPIO_SetBits(OLED_PORT, OLED_SCL_PIN);
    OLED_DelayUs(2);
    GPIO_SetBits(OLED_PORT, OLED_SDA_PIN);
    OLED_DelayUs(2);
}

static void OLED_I2C_SendByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        GPIO_WriteBit(OLED_PORT, OLED_SDA_PIN,
                      (byte & (0x80 >> i)) ? Bit_SET : Bit_RESET);
        OLED_DelayUs(1);
        GPIO_SetBits(OLED_PORT, OLED_SCL_PIN);
        OLED_DelayUs(2);
        GPIO_ResetBits(OLED_PORT, OLED_SCL_PIN);
        OLED_DelayUs(1);
    }
    GPIO_SetBits(OLED_PORT, OLED_SCL_PIN);
    OLED_DelayUs(2);
    GPIO_ResetBits(OLED_PORT, OLED_SCL_PIN);
}

static void OLED_WriteCmd(uint8_t cmd)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);
    OLED_I2C_SendByte(0x00);
    OLED_I2C_SendByte(cmd);
    OLED_I2C_Stop();
}

static void OLED_WriteData(uint8_t data)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);
    OLED_I2C_SendByte(0x40);
    OLED_I2C_SendByte(data);
    OLED_I2C_Stop();
}

static void OLED_SetCursor(uint8_t Page, uint8_t X)
{
    OLED_WriteCmd(0xB0 | Page);
    OLED_WriteCmd(0x10 | ((X & 0xF0) >> 4));
    OLED_WriteCmd(0x00 | (X & 0x0F));
}

void OLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(OLED_PORT, &GPIO_InitStructure);

    GPIO_SetBits(OLED_PORT, OLED_SCL_PIN | OLED_SDA_PIN);

    /* SSD1306 上电后要等电源稳定 + 内部复位，datasheet 要求 >=1ms。
       原来是忙等 "100us"，8MHz 时约 2ms 勉强够，主频提到 72MHz 后只剩 0.23ms，
       初始化就会失败（花屏/黑屏）。这里改成实打实的 100ms。 */
    Delay_ms(100);

    OLED_WriteCmd(0xAE);
    OLED_WriteCmd(0x20); OLED_WriteCmd(0x10);
    OLED_WriteCmd(0xB0);
    OLED_WriteCmd(OLED_CMD_COM_REMAP);
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x10);
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0x81); OLED_WriteCmd(0xFF);
    OLED_WriteCmd(OLED_CMD_SEG_REMAP);
    OLED_WriteCmd(0xA6);
    OLED_WriteCmd(0xA8); OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xA4);
    OLED_WriteCmd(0xD3); OLED_WriteCmd(0x00);
    OLED_WriteCmd(0xD5); OLED_WriteCmd(0xF0);
    OLED_WriteCmd(0xD9); OLED_WriteCmd(0x22);
    OLED_WriteCmd(0xDA); OLED_WriteCmd(0x12);
    OLED_WriteCmd(0xDB); OLED_WriteCmd(0x20);
    OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14);
    OLED_WriteCmd(0xAF);

    OLED_Clear();
}

void OLED_Clear(void)
{
    for (uint8_t p = 0; p < 8; p++) {
        OLED_SetCursor(p, 0);
        for (uint8_t x = 0; x < 128; x++) OLED_WriteData(0x00);
    }
}

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t idx;

    /* 越界保护：字库只有 0x20~0x7F 共 96 个字符，屏幕是 4 行 x 16 列 */
    if (Line < 1 || Line > 4 || Column < 1 || Column > 16) return;
    if ((uint8_t)Char < ' ' || (uint8_t)Char > '~') Char = ' ';
    idx = (uint8_t)(Char - ' ');
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (uint8_t i = 0; i < 8; i++)
        OLED_WriteData(OLED_F8x16[idx][i]);
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (uint8_t i = 0; i < 8; i++)
        OLED_WriteData(OLED_F8x16[idx][i + 8]);
}

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    for (uint8_t i = 0; String[i] != '\0'; i++)
        OLED_ShowChar(Line, Column + i, String[i]);
}

static uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t r = 1;
    while (Y--) r *= X;
    return r;
}

void OLED_ShowNum(uint8_t Line, uint8_t Column,
                  uint32_t Number, uint8_t Length)
{
    for (uint8_t i = 0; i < Length; i++)
        OLED_ShowChar(Line, Column + i,
                      Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
}

void OLED_ShowFloat(uint8_t Line, uint8_t Column,
                    float Number, uint8_t IntLen, uint8_t DecLen)
{
    int32_t  intPart;

    if (Number < 0.0f) Number = 0.0f;   /* 不会出现负电压，钳位避免 OLED_ShowNum 收到补码大数 */
    intPart = (int32_t)Number;
    uint32_t decPart = (uint32_t)((Number - intPart) * OLED_Pow(10, DecLen));
    if (decPart >= OLED_Pow(10, DecLen)) decPart = OLED_Pow(10, DecLen) - 1;

    OLED_ShowNum(Line, Column, intPart, IntLen);
    OLED_ShowChar(Line, Column + IntLen, '.');
    OLED_ShowNum(Line, Column + IntLen + 1, decPart, DecLen);
}

static uint8_t oled_gram[8][128];

/* ---- 显存操作（A 页波形用）---- */

void OLED_ClearGRAM(void)
{
    for (uint8_t p = 0; p < 8; p++)
        for (uint8_t x = 0; x < 128; x++)
            oled_gram[p][x] = 0x00;
}

void OLED_DrawPoint(uint8_t x, uint8_t y)
{
    if (x >= 128 || y >= 64) return;
    oled_gram[y / 8][x] |= (1 << (y % 8));
}

void OLED_Refresh(void)
{
    for (uint8_t p = 0; p < 8; p++) {
        OLED_SetCursor(p, 0);
        for (uint8_t x = 0; x < 128; x++)
            OLED_WriteData(oled_gram[p][x]);
    }
}