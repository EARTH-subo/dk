/**
 * 呼吸灯 —— 阻塞版（A0 单灯）
 *
 * 功能：A0（PA0）上一颗共阳 LED 由暗→亮→暗，像一盏呼吸灯；
 *       呼吸周期约 3.8s（128 档 × 30ms），亮度按 raised-cosine 曲线变化；
 *       PC13 心跳灯每 500ms 翻转一次，用来确认程序确实在跑。
 * 接线：LED 长脚（正极）接面包板正极(3.3V)，短脚（负极）接 A0；
 *       共阳接法，低电平点亮，所以亮度要反相（见 breath.h）。
 *
 * 思路：用 HAL_Delay(BREATH_STEP_MS) 死等 30ms 推进呼吸相位 phase；
 *       心跳用 HAL_GetTick() 时间戳判断 500ms。
 *
 * 模块划分与关联：
 *   1) SysTick 中断 → HAL_IncTick() 维护毫秒时基，HAL_Delay() 靠它计时；
 *   2) Breath_Init() 把 PA0 配成 TIM2_CH1 硬件 PWM（1kHz、1000 档亮度）；
 *   3) breath_lut[128] 是预计算的呼吸亮度表（暗→亮→暗）；
 *   4) Breath_SetBrightness(0, 亮度) 把亮度写进 CCR 寄存器，占空比实时变化；
 *   5) phase 每 30ms +1，128 步走完一个周期回到起点。
 */
#include "breath.h"   // 呼吸灯共享模块：PWM 初始化 + 亮度设置 + 呼吸表

/**
 * SysTick 中断服务函数（每 1ms 触发一次）
 * 调用 HAL_IncTick() 让 HAL 库的毫秒计数器 +1，HAL_Delay() 依赖它计时。
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

int main(void)
{
    // 1. 初始化 HAL 库（配置 SysTick 等），必须最先调用
    HAL_Init();

    // 2. 初始化 TIM2 PWM：把 PA0 配成硬件 PWM 输出，初始全灭
    Breath_Init();

    // 3. PC13 心跳灯：推挽输出，共阳低电平点亮，先给高电平=灭
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;   // 推挽输出，能主动输出高低电平
    gpio.Pull  = GPIO_NOPULL;           // 无需上下拉
    gpio.Speed = GPIO_SPEED_FREQ_LOW;   // 低速即可（点灯）
    gpio.Pin   = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOC, &gpio);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

    // 4. 呼吸相关变量
    uint16_t phase       = SIN_N / 2;   // 从最亮(表下标 64)开始，先亮后暗
    uint32_t last_heart  = HAL_GetTick(); // 上次心跳翻转时间戳

    // 5. 主循环：每轮死等 30ms 推进一档呼吸，再判断心跳
    while (1)
    {
        // 只控制 A0 这一路（TIM2_CH1 → PA0）的亮度
        Breath_SetBrightness(0, breath_lut[phase]);

        HAL_Delay(BREATH_STEP_MS);      // 阻塞等 30ms（期间 CPU 空转）
        phase = (phase + 1) % SIN_N;    // 前进一档，128 档走完回 0

        // 心跳：每 500ms 翻转一次 PC13（程序在跑的标志）
        if (HAL_GetTick() - last_heart >= 500)
        {
            last_heart = HAL_GetTick();
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
    }
}
