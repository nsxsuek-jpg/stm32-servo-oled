#include "stm32f10x.h"
#include "oled.h"
#include "servo.h"
#include "adc.h"
#include "key.h"
#include "buzzer.h"
#include "led.h"
#include "delay.h"

#define LOOP_MS         20      /* 主循环基础节拍 (ms) */
#define ANGLE_STEP       5.0f   /* 轻点一下的角度步长 (度) */
#define WAVE_PERIOD_MS   100    /* 波形采样周期：10 次/秒，一屏 128 点约 12.8 秒 */
#define WAVE_LEN         128    /* 波形缓冲区长度 */

/* ============================================================
 * 系统时钟
 *   1) 首选 HSE 8MHz * 9 = 72MHz
 *   2) HSE 起不来（没焊晶振 / 晶振坏 / 起振太慢）就退回 HSI/2 * 16 = 64MHz
 *   3) 两条都失败就还是 HSI 8MHz（系统仍能跑）
 * 每一步都有超时，任何失败都不会卡死。
 * 实际跑在多少 MHz 显示在第 3 页 "CLK:" 那一行。
 *
 * 注意：framework = cmsis 时框架给的 SystemInit() 是空函数（反汇编只有 bx lr），
 *       不会配置 PLL，必须在这里自己配。
 * ============================================================ */
static volatile uint8_t g_clock_hse = 0;   /* 1 = 跑在 HSE 的 PLL 上 */
static uint8_t g_pb6_pad_ok = 0;              /* PB6 引脚自检结果 */

/* 这个阶段还没有 SysTick，只能用最原始的忙等 */
static void Clock_RoughDelay(volatile uint32_t loops)
{
    while (loops--) {
        __NOP();
    }
}

/* 等 HSE 起振。
   晶振从上电到稳定通常要 1~2ms，而 SPL 的 RCC_WaitForHSEStartUp()
   只等约 0.8ms，一次就成功很靠运气，所以失败后等一会儿重试。 */
static uint8_t Clock_StartHSE(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        RCC_HSEConfig(RCC_HSE_ON);
        if (RCC_WaitForHSEStartUp() == SUCCESS) {
            return 1;
        }
        RCC_HSEConfig(RCC_HSE_OFF);
        Clock_RoughDelay(40000);      /* HSI 8MHz 下约 10ms */
    }
    return 0;
}

/* 打开 PLL 并把 SYSCLK 切过去。全程带超时；失败时系统仍在 HSI 上。 */
static uint8_t Clock_TryPLL(uint32_t pll_source, uint32_t pll_mul)
{
    uint32_t timeout;

    RCC_HCLKConfig(RCC_SYSCLK_Div1);    /* AHB  = SYSCLK */
    RCC_PCLK2Config(RCC_HCLK_Div1);     /* APB2 = SYSCLK */
    RCC_PCLK1Config(RCC_HCLK_Div2);     /* APB1 = SYSCLK/2 */

    RCC_PLLConfig(pll_source, pll_mul);
    RCC_PLLCmd(ENABLE);

    timeout = 2000000UL;
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET && --timeout) {
    }
    if (timeout == 0UL) {
        RCC_PLLCmd(DISABLE);
        return 0;
    }

    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
    timeout = 2000000UL;
    while (RCC_GetSYSCLKSource() != 0x08 && --timeout) {
    }
    if (timeout == 0UL) {
        return 0;                       /* 没切过去，仍在 HSI 上 */
    }
    return 1;
}

static void SystemClock_Config(void)
{
    uint32_t timeout;

    /* 先把 flash 等待周期和预取设好：后面无论跑多快都安全 */
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    if (Clock_StartHSE()) {
        /* HSE 8MHz x 9 = 72MHz */
        if (Clock_TryPLL(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9)) {
            g_clock_hse = 1;
        }
    }

    if (!g_clock_hse) {
        /* 没有可用晶振：先确保回到 HSI、PLL 关掉，再用 HSI/2 x 16 = 64MHz。
           这条路不需要外部晶振，舵机照样是 50Hz。 */
        RCC_SYSCLKConfig(RCC_SYSCLKSource_HSI);
        timeout = 2000000UL;
        while (RCC_GetSYSCLKSource() != 0x00 && --timeout) {
        }
        RCC_PLLCmd(DISABLE);
        RCC_HSEConfig(RCC_HSE_OFF);

        Clock_TryPLL(RCC_PLLSource_HSI_Div2, RCC_PLLMul_16);
    }

    SystemCoreClockUpdate();    /* Delay_Init() 要用 SystemCoreClock */
}

/* ============================================================
 * 1ms 系统节拍
 * ============================================================ */
void SysTick_Handler(void)
{
    Delay_TickInc();
    Key_Scan();
    Buzzer_Scan();
    Servo_Update();      /* 每 1ms 平滑推进舵机角度 */
}

/* ============ 波形缓冲区 ============ */
static uint8_t wave_buf[WAVE_LEN];
static uint8_t wave_idx = 0;

static void Wave_Push(uint8_t angle)
{
    wave_buf[wave_idx++] = angle;
    if (wave_idx >= WAVE_LEN) wave_idx = 0;
}

/* 开机时把整条缓冲区填成当前角度。
   否则一上来整屏都是 0，波形会是一条躺在最底下的直线，看着像坏了。 */
static void Wave_Reset(uint8_t angle)
{
    for (uint8_t i = 0; i < WAVE_LEN; i++) {
        wave_buf[i] = angle;
    }
    wave_idx = 0;
}

static void Wave_Draw(void)
{
    OLED_ClearGRAM();

    /* 底部基线 */
    for (uint8_t x = 0; x < 128; x++)
        OLED_DrawPoint(x, 63);

    /* 逐列画波形：最老的数据在最左边 */
    for (uint8_t x = 0; x < 128; x++) {
        uint8_t ang = wave_buf[(wave_idx + x) % WAVE_LEN];
        uint8_t y   = (uint8_t)(62 - (uint8_t)(ang * 62 / 180));
        OLED_DrawPoint(x, y);
        OLED_DrawPoint(x, (uint8_t)(y + 1));
    }

    OLED_Refresh();
}

/* ============ 三个页面 ============ */

/* 只重画变化的部分。
   整页重画一次要走 780 多个字节的软件 I2C（60ms 以上），主循环被它拖住，
   按键和舵机就会"慢半拍"。标号只在切页/第一帧画一次。 */
static void Page1_Show(float angle, uint16_t volt_mV, uint16_t pulse, uint8_t force)
{
    static float    last_angle = -1.0f;
    static uint16_t last_volt  = 0xFFFFu;
    static uint16_t last_pulse = 0xFFFFu;

    if (force) {
        OLED_ShowString(1, 1, "Servo Control");
        OLED_ShowString(2, 1, "Angle:");
        OLED_ShowString(2, 10, "deg");
        OLED_ShowString(3, 1, "Volt: ");
        OLED_ShowString(3, 12, "V");
        OLED_ShowString(4, 1, "Pulse:");
        OLED_ShowString(4, 11, "us");
        last_angle = -1.0f;
        last_volt  = 0xFFFFu;
        last_pulse = 0xFFFFu;
    }

    if (angle != last_angle) {
        OLED_ShowNum(2, 7, (uint32_t)angle, 3);
        last_angle = angle;
    }
    if (volt_mV != last_volt) {
        OLED_ShowFloat(3, 7, (float)volt_mV / 1000.0f, 1, 3);
        last_volt = volt_mV;
    }
    if (pulse != last_pulse) {
        OLED_ShowNum(4, 7, pulse, 4);
        last_pulse = pulse;
    }
}

static void Page2_Show(float angle)
{
    Wave_Draw();

    /* 波形是往显存里画的，刷新完再叠一层文字上去，
       这样一眼就能看出这一页是不是在工作（90 度时波形在中线，不会挡住左上角）*/
    OLED_ShowString(1, 1, "Angle:");
    OLED_ShowNum   (1, 7, (uint32_t)angle, 3);
}

/* 第 3 页兼作自检页：CLK 那行显示实际系统时钟和时钟源 */
/* 同样只重画变化的数字 */
static void Page3_Show(uint16_t vdd_mV, uint16_t volt_mV, uint32_t mhz, uint8_t force)
{
    static uint16_t last_vdd = 0xFFFFu;
    static uint16_t last_pot = 0xFFFFu;
    static uint32_t last_mhz = 0xFFFFFFFFu;

    if (force) {
        OLED_ShowString(1, 1, "System Info");
        OLED_ShowString(2, 1, "VDD :");
        OLED_ShowString(2, 12, "V");
        OLED_ShowString(3, 1, "POT :");
        OLED_ShowString(3, 12, "V");
        OLED_ShowString(4, 1, "CLK:");
        OLED_ShowString(4, 7, "MHz");
        if (g_clock_hse) {
            OLED_ShowString(4, 11, "HSE");
        } else {
            OLED_ShowString(4, 11, "HSI");
        }
        last_vdd = 0xFFFFu;
        last_pot = 0xFFFFu;
        last_mhz = 0xFFFFFFFFu;
    }

    if (vdd_mV != last_vdd) {
        OLED_ShowFloat(2, 7, (float)vdd_mV / 1000.0f, 1, 3);
        last_vdd = vdd_mV;
    }
    if (volt_mV != last_pot) {
        OLED_ShowFloat(3, 7, (float)volt_mV / 1000.0f, 1, 3);
        last_pot = volt_mV;
    }
    if (mhz != last_mhz) {
        OLED_ShowNum(4, 5, mhz, 2);
        last_mhz = mhz;
    }
}

/* ============================================================
 * 第 4 页：PB6 输出自检（不需要万用表）
 *
 * 做法：
 *   1) 把电位器在 PA0 上的那根线拔掉
 *   2) 用一根杜邦线把 PB6 和 PA0 连起来
 *   3) 按翻页键 3 次到这一页（三个 LED 全灭就是这一页）
 *
 * 判断：
 *   MIN≈0.00V 且 MAX≈3.30V  -> PB6 输出正常，PWM 没问题
 *   MIN=MAX≈0.00V           -> PB6 一直是低电平（没输出）
 *   MIN=MAX≈3.30V           -> PB6 一直是高电平
 * ============================================================ */
static void Page4_SelfTest(void)
{
    uint16_t vmin = 0;
    uint16_t vmax = 0;

    ADC_GetPulseRange_mV(&vmin, &vmax);

    /* 第 1 行不需要接任何线，上电时就测好了 */
    if (g_pb6_pad_ok) {
        OLED_ShowString(1, 1, "PB6 Pad: OK");
    } else {
        OLED_ShowString(1, 1, "PB6 Pad:BAD");
    }

    OLED_ShowString(2, 1, "MIN:");
    OLED_ShowFloat (2, 5, (float)vmin / 1000.0f, 1, 3);
    OLED_ShowString(2, 10, "V");

    OLED_ShowString(3, 1, "MAX:");
    OLED_ShowFloat (3, 5, (float)vmax / 1000.0f, 1, 3);
    OLED_ShowString(3, 10, "V");

    /* MIN/MAX 是 PA0 上看到的最低/最高电压。
       只有把 PB6 用跳线接到 PA0，这里才会看到 0V 和 3.3V 的跳变；
       没接跳线时 PA0 上只有电位器的电压（例如 1.574V 不动），
       那并不是说 PB6 没有 PWM —— 引脚好坏看第 1 行就够了。 */
    /* 顺便做一个蜂鸣器通断测试：1.2 秒高 + 0.3 秒低，
       用耳朵就能判断信号线到底有没有接在 B0 上 */
    Buzzer_TestPattern();

    if (!g_pb6_pad_ok) {
        OLED_ShowString(4, 1, "CHECK PIN!");
    } else if (vmax > 2000 && vmin < 600) {
        OLED_ShowString(4, 1, "PWM OK");
    } else {
        OLED_ShowString(4, 1, "CHECK JUMPER");
    }
}

/* ============ 主函数 ============ */
int main(void)
{
    uint16_t Voltage_mV   = 0;
    uint16_t VDD_mV       = 0;

    uint8_t  Page         = 1;
    uint8_t  event;
    uint8_t  held;
    uint32_t wave_tick;
    uint8_t  page_dirty   = 1;   /* 需要整页重画 */


    SystemClock_Config();   /* 必须最先执行：其它初始化都依赖系统时钟 */

    /* 中断里会用到的外设先初始化，再开 1ms 节拍 */
    Key_Init();
    Buzzer_Init();
    Delay_Init();

    /* ---- 外设初始化 ---- */
    OLED_Init();            /* 内部用 Delay_ms()，必须在 Delay_Init() 之后 */
    Servo_Init();
    g_pb6_pad_ok = Servo_PinSelfTest();   /* PB6 引脚自检（不需要接线）*/
    MyADC_Init();
    LED_Init();

    Servo_SetAngle(90.0f);
    LED_SetPage(Page);

    OLED_Clear();
    Page1_Show(Servo_GetAngle(), 0, Servo_GetPulse(), 1);
    page_dirty = 0;

    Wave_Reset((uint8_t)Servo_GetAngle());   /* 波形缓冲区先铺满当前角度 */
    wave_tick = GetTick();

    while (1)
    {
        /* ---- 按键事件：切页 / 提示音 ---- */
        event = Key_GetEvent();
        if (event == 3) {
            Page = (Page >= 4) ? 1 : (uint8_t)(Page + 1);   /* 第 4 页是自检页 */
            Buzzer_Off();          /* 离开第 4 页时把蜂鸣器自检波形停掉 */
            OLED_Clear();
            page_dirty = 1;        /* 屏幕被清了，整页重画一次 */
            LED_SetPage(Page);
            Buzzer_Beep(30);
        } else if (event == 1 || event == 2) {
            /* 轻点一下走一步（只改目标角度，舵机会平滑滑过去）*/
            Servo_Jog((event == 1) ? -ANGLE_STEP : ANGLE_STEP);
            Buzzer_Beep(10);
        }

        /* ---- 按住不放：只告诉 servo 模块方向，
               具体推进由 1ms 节拍连续做，主循环慢也不会卡顿 ---- */
        held = Key_GetState();
        if (held == 1) {
            Servo_SetHold(-1);
        } else if (held == 2) {
            Servo_SetHold(1);
        } else {
            Servo_SetHold(0);
        }

        /* ---- 采集：先测一次 VDD，再用它换算电位器电压 ---- */
        VDD_mV     = ADC_GetVDD_mV();
        Voltage_mV = ADC_GetVoltage_mV();

        /* ---- 波形采样：按真实时间 1s 一次 ---- */
        if ((int32_t)(GetTick() - wave_tick) >= WAVE_PERIOD_MS) {
            wave_tick += WAVE_PERIOD_MS;
            Wave_Push((uint8_t)Servo_GetAngle());
        }

        /* ---- 绘制当前页面 ---- */
        switch (Page)
        {
            case 1:
                Page1_Show(Servo_GetAngle(), Voltage_mV, Servo_GetPulse(), page_dirty);
                break;

            case 2:
                Page2_Show(Servo_GetAngle());
                break;

            case 3:
                Page3_Show(VDD_mV, Voltage_mV, SystemCoreClock / 1000000U, page_dirty);
                break;

            case 4:
                Page4_SelfTest();
                break;
        }

        page_dirty = 0;

        Delay_ms(LOOP_MS);
    }
}
