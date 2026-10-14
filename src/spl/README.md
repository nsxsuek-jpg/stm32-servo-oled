# src/spl —— 从 lib/ 复制过来的标准外设库(SPL)源文件

PlatformIO 的库依赖扫描(LDF)不会编译 `lib/` 下的这两个"库"：
`platformio.ini` 的 `-I lib/...` 已经把它们的头文件路径直接加进了编译参数，
LDF 因此认为工程"没有依赖库"，既不生成 `.a` 也不会编译里面的任何 `.c`。
所以**真正参与链接的 SPL 源文件必须是这里的副本**，`lib/` 只当"头文件仓库"用。

当前需要的副本（对应 `lib/STM32F10x_StdPeriph_Driver/src/` 与 `lib/CMSIS/CM3/CoreSupport/`）：

| 文件 | 提供 |
| --- | --- |
| `core_cm3.c`       | CMSIS 内核函数 |
| `misc.c`           | NVIC 中断优先级等 |
| `stm32f10x_rcc.c`  | 时钟/复位（`SystemClock_Config()` 依赖它） |
| `stm32f10x_gpio.c` | GPIO |
| `stm32f10x_tim.c`  | TIM（舵机 PWM） |
| `stm32f10x_adc.c`  | ADC |

以后要用 USART / SPI / I2C / EXTI / FLASH 等，把对应的
`lib/STM32F10x_StdPeriph_Driver/src/stm32f10x_xxx.c` 再复制到本目录即可，
同时记得在 `include/stm32f10x_conf.h` 里加上对应的 `#include`。

> 注意：**不要**复制 `lib/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.c`，
> 它和框架提供的 `system_stm32f1xx.c` 都会定义 `SystemInit()`，会重复定义链接失败。
> 系统时钟统一在 `src/main.c` 的 `SystemClock_Config()` 里配置。
