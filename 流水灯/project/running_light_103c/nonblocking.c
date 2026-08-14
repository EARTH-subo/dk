/**
 * 流水灯 —— 非阻塞版
 *
 * 功能：红=A0、黄=A1、蓝=A2、绿=A3 四个 LED 单灯循环流动；
 *       走到 A3（绿）后停 1 秒，再回到 A0（红）重新开始；
 *       PC13 心跳灯每 500ms 翻转一次。
 * 接线：共阳 LED，低电平点亮（点亮=RESET，熄灭=SET）。
 *
 * 思路：用 HAL_GetTick() 取系统毫秒时间，通过"当前时间 - 上次时间"
 *       判断是否到点，而不是用 HAL_Delay() 死等，所以 CPU 不阻塞。
 *
 * 模块划分与关联：
 *   1) SysTick 中断 → HAL_IncTick() 维护软件时基（毫秒计数器）；
 *   2) HAL_GetTick() 读这个时基，得到"现在几点"；
 *   3) main 用三个"上次时间戳"(last_step / last_heart / pause_t0)
 *      分别记录流水灯、心跳、停顿各自的上次动作时刻；
 *   4) 每次循环拿 now 和这些时间戳做差，差值够了就执行对应动作，
 *      三件事互相独立、互不干扰。
 */
#include "stm32f1xx_hal.h"   // HAL 库总头文件：提供 GPIO/时钟/延时等 API

// 四个 LED 引脚合并成一个掩码，方便一次"全灭"（写高电平）
#define LED_ALL    (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)

/**
 * SysTick 中断服务函数（每 1ms 触发一次）
 * 调用 HAL_IncTick() 让 HAL 库的毫秒计数器 +1，
 * 这就是 HAL_GetTick() 能读到的时间来源。
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

int main(void)
{
    // 1. 初始化 HAL 库（配置 SysTick 等），必须最先调用
    HAL_Init();

    // 2. 使能 GPIOA、GPIOC 外设时钟——STM32 外设必须先开时钟才能读写
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // 3. 配置 GPIO 为推挽输出
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;   // 推挽输出，能主动输出高低电平
    gpio.Pull  = GPIO_NOPULL;           // 无需上下拉
    gpio.Speed = GPIO_SPEED_FREQ_LOW;   // 低速即可（点灯）

    gpio.Pin = LED_ALL;                 // PA0~PA3 四个 LED 脚
    HAL_GPIO_Init(GPIOA, &gpio);        // 把配置写入 GPIOA

    gpio.Pin = GPIO_PIN_13;             // PC13 心跳灯脚
    HAL_GPIO_Init(GPIOC, &gpio);        // 把配置写入 GPIOC

    // 4. PC13 初始熄灭（共阳低电平点亮，所以先给高电平=灭）
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

    // 5. 流水灯相关变量
    const uint16_t leds[4] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3}; // 红 黄 蓝 绿
    int      idx        = 0;               // 当前点亮的灯下标（0~3）
    uint32_t now        = HAL_GetTick();   // 当前系统毫秒时间
    uint32_t last_step  = now;             // 上次"移动灯"的时间戳
    uint32_t last_heart = now;             // 上次"心跳翻转"的时间戳
    uint32_t pause_t0   = 0;               // 进入停顿的时刻
    uint8_t  pause      = 0;               // 是否处于停顿状态(1=停顿中)

    // 6. 初始状态：四个灯全灭，再单独点亮红(A0)
    HAL_GPIO_WritePin(GPIOA, LED_ALL, GPIO_PIN_SET);    // 全灭(高电平)
    HAL_GPIO_WritePin(GPIOA, leds[0], GPIO_PIN_RESET);  // 点亮红(低电平)

    // 7. 主循环：不停读时间、判断到点、执行动作，CPU 不空等
    while (1)
    {
        now = HAL_GetTick();   // 先取当前时间，后面所有判断都用它

        // —— 流水灯部分 ——
        if (pause)
        {
            // 停顿中：停满 1 秒就回到 A0 重新开始
            if (now - pause_t0 >= 1000)
            {
                pause = 0;                                // 退出停顿
                idx = 0;                                  // 回到第一个灯
                HAL_GPIO_WritePin(GPIOA, LED_ALL, GPIO_PIN_SET);    // 全灭
                HAL_GPIO_WritePin(GPIOA, leds[0], GPIO_PIN_RESET);  // 点亮红
                last_step = now;                          // 重置移动时间戳
            }
        }
        else
        {
            // 正常流动：每 200ms 往后走一个灯
            if (now - last_step >= 200)
            {
                last_step = now;                          // 记录本次移动时刻
                if (idx == 3)
                {
                    // 已经到 A3(绿)，进入停顿，绿灯保持亮
                    pause = 1;
                    pause_t0 = now;
                }
                else
                {
                    // 还没到末尾：下标+1，全灭后点亮下一个灯
                    idx++;
                    HAL_GPIO_WritePin(GPIOA, LED_ALL, GPIO_PIN_SET);     // 全灭
                    HAL_GPIO_WritePin(GPIOA, leds[idx], GPIO_PIN_RESET); // 点亮当前
                }
            }
        }

        // —— 心跳灯部分（独立判断，不受流水灯影响）——
        if (now - last_heart >= 500)
        {
            last_heart = now;                             // 记录本次翻转时刻
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);       // 翻转 PC13 电平
        }
    }
}
