/**
 * 流水灯 —— 阻塞版
 *
 * 功能：红=A0、黄=A1、蓝=A2、绿=A3 四个 LED 单灯循环流动；
 *       走到 A3（绿）后停 1 秒，再回到 A0（红）重新开始；
 *       PC13 心跳灯每 500ms 翻转一次。
 * 接线：共阳 LED，低电平点亮（点亮=RESET，熄灭=SET）。
 *
 * 思路：用 HAL_Delay(100) 每次死等 100ms，再用计数变量 tick
 *       按"节拍"推算 200ms / 500ms / 1000ms，逻辑简单直接。
 *
 * 模块划分与关联：
 *   1) SysTick 中断 → HAL_IncTick() 维护毫秒时基；
 *   2) HAL_Delay() 内部靠这个时基"死等"指定毫秒数，期间 CPU 空转；
 *   3) main 主循环每轮 HAL_Delay(100) 后 tick++，所以 tick 每 +1 代表过了 100ms；
 *   4) 用 tick 的倍数/差值推算出 200ms 走灯、500ms 心跳、1000ms 停顿。
 */
#include "stm32f1xx_hal.h"   // HAL 库总头文件：提供 GPIO/延时等 API

// 四个 LED 引脚合并成一个掩码，方便一次"全灭"（写高电平）
#define LED_ALL    (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)

/**
 * SysTick 中断服务函数（每 1ms 触发一次）
 * 调用 HAL_IncTick() 让 HAL 库毫秒计数器 +1，HAL_Delay() 依赖它计时。
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
    int      idx        = 0;    // 当前点亮的灯下标（0~3）
    uint32_t tick       = 0;    // 主循环节拍计数：每 100ms 加 1
    uint8_t  pause      = 0;    // 是否处于停顿状态(1=停顿中)
    uint32_t pause_tick = 0;    // 进入停顿那一刻的 tick 值

    // 6. 初始状态：四个灯全灭，再单独点亮红(A0)
    HAL_GPIO_WritePin(GPIOA, LED_ALL, GPIO_PIN_SET);    // 全灭(高电平)
    HAL_GPIO_WritePin(GPIOA, leds[0], GPIO_PIN_RESET);  // 点亮红(低电平)

    // 7. 主循环：每轮死等 100ms 后推进一次节拍，再判断该做什么
    while (1)
    {
        HAL_Delay(100);   // 阻塞等 100ms（期间 CPU 空转）
        tick++;           // 节拍 +1，代表又过了 100ms

        // —— 流水灯部分 ——
        if (pause)
        {
            // 停顿中：停满 1 秒(10 个 100ms 节拍)就回到 A0
            if (tick - pause_tick >= 10)
            {
                pause = 0;                                // 退出停顿
                idx = 0;                                  // 回到第一个灯
                HAL_GPIO_WritePin(GPIOA, LED_ALL, GPIO_PIN_SET);    // 全灭
                HAL_GPIO_WritePin(GPIOA, leds[0], GPIO_PIN_RESET);  // 点亮红
            }
        }
        else
        {
            // 正常流动：每 2 个节拍(200ms)往后走一个灯
            if (tick % 2 == 0)
            {
                if (idx == 3)
                {
                    // 已经到 A3(绿)，进入停顿，绿灯保持亮
                    pause = 1;
                    pause_tick = tick;
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

        // —— 心跳灯部分（每 5 个节拍=500ms 翻转一次）——
        if (tick % 5 == 0)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);   // 翻转 PC13 电平
        }
    }
}
