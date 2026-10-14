#include "key.h"

#define KEY_DEBOUNCE_MS  10      /* 连续稳定 10ms 才算有效状态 */
#define KEY_EVENT_BUF    4       /* 事件队列长度 */

static volatile uint8_t s_state    = 0;   /* 消抖后的状态 */
static volatile uint8_t s_last_raw = 0;
static volatile uint8_t s_stable   = 0;   /* 当前原始值已稳定多少 ms */

/* 事件队列：中断里入队、主循环出队。
   主循环要刷一大屏 OLED（几十 ms），如果只比较"上一次状态"，
   短按很可能整个被漏掉，所以这里把按下沿排队存起来。 */
static volatile uint8_t s_ev[KEY_EVENT_BUF];
static volatile uint8_t s_ev_head = 0;
static volatile uint8_t s_ev_tail = 0;

static uint8_t Key_ReadRaw(void)
{
    uint8_t k1 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1);
    uint8_t k2 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_2);
    uint8_t k3 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3);

    if (k1 == 0 && k2 == 1 && k3 == 1) return 1;
    if (k2 == 0 && k1 == 1 && k3 == 1) return 2;
    if (k3 == 0)                       return 3;
    return 0;
}

static void Key_PushEvent(uint8_t key)
{
    uint8_t next = (uint8_t)((s_ev_head + 1U) % KEY_EVENT_BUF);

    if (next != s_ev_tail) {        /* 队列满就丢弃，绝不覆盖未处理的事件 */
        s_ev[s_ev_head] = key;
        s_ev_head = next;
    }
}

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    s_state    = 0;
    s_last_raw = Key_ReadRaw();
    s_stable   = 0;
    s_ev_head  = 0;
    s_ev_tail  = 0;
}

/* 1ms 调一次：状态连续稳定 KEY_DEBOUNCE_MS 毫秒才认账 */
void Key_Scan(void)
{
    uint8_t raw = Key_ReadRaw();

    if (raw != s_last_raw) {        /* 还在抖动：重新计时 */
        s_last_raw = raw;
        s_stable   = 0;
        return;
    }

    if (s_stable < KEY_DEBOUNCE_MS) {
        s_stable++;
    }

    if (s_stable == KEY_DEBOUNCE_MS && raw != s_state) {
        s_state = raw;
        if (raw != 0) {
            Key_PushEvent(raw);     /* 只在按下沿产生一次事件 */
        }
    }
}

uint8_t Key_GetState(void)
{
    return s_state;
}

uint8_t Key_GetEvent(void)
{
    uint8_t key = 0;

    if (s_ev_head != s_ev_tail) {
        key = s_ev[s_ev_tail];
        s_ev_tail = (uint8_t)((s_ev_tail + 1U) % KEY_EVENT_BUF);
    }
    return key;
}
