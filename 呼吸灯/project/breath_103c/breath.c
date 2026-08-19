/**
 * 呼吸灯共享模块 —— 实现
 *
 * 关键点：
 *   1) PA0~PA3 正好是 TIM2 的 CH1~CH4，无需 AFIO 重映射即可输出 PWM；
 *   2) PWM 频率 = 72MHz / (PSC+1) / (ARR+1) = 1kHz，亮度分辨率 1000 档；
 *   3) 共阳 LED 低电平点亮，所以亮度要反相：CCR = ARR - 亮度。
 *
 * 亮度原理（PWM1 模式，CNT < CCR 时输出有效电平）：
 *   - 亮度 999 → CCR=0   → 几乎全低电平 → 共阳 LED 最亮；
 *   - 亮度 0   → CCR=999 → 几乎全高电平 → 共阳 LED 熄灭。
 */
#include "breath.h"

// 一个完整呼吸周期（暗→亮→暗）的亮度表
// breath_lut[k] = round((1 - cos(2*pi*k/128)) / 2 * 999)
const uint16_t breath_lut[SIN_N] = {
    0,   1,   2,   5,  10,  15,  22,  29,  38,  48,  59,  71,  84,  98, 113, 129,
  146, 164, 183, 202, 222, 243, 264, 286, 308, 331, 355, 378, 402, 426, 451, 475,
  499, 524, 548, 573, 597, 621, 644, 668, 691, 713, 735, 756, 777, 797, 816, 835,
  853, 870, 886, 901, 915, 928, 940, 951, 961, 970, 977, 984, 989, 994, 997, 998,
  999, 998, 997, 994, 989, 984, 977, 970, 961, 951, 940, 928, 915, 901, 886, 870,
  853, 835, 816, 797, 777, 756, 735, 713, 691, 668, 644, 621, 597, 573, 548, 524,
  500, 475, 451, 426, 402, 378, 355, 331, 308, 286, 264, 243, 222, 202, 183, 164,
  146, 129, 113,  98,  84,  71,  59,  48,  38,  29,  22,  15,  10,   5,   2,   1,
};

// 四路 PWM 通道，对应 A0~A3
const uint32_t breath_ch[4] = {
    TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4,
};

TIM_HandleTypeDef htim2;

/**
 * TIM2 的底层初始化回调（HAL_TIM_PWM_Init 内部会调用）：
 * 开时钟 + 把 PA0~PA3 配成复用推挽输出。
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitTypeDef gpio = {0};
        gpio.Mode  = GPIO_MODE_AF_PP;      // 复用推挽，输出 PWM 波形
        gpio.Pull  = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        gpio.Pin   = LED_ALL;
        HAL_GPIO_Init(GPIOA, &gpio);
    }
}

/**
 * 初始化 TIM2 四路 PWM 并启动，初始状态全灭（共阳高电平）。
 */
void Breath_Init(void)
{
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = BREATH_PSC;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = BREATH_ARR;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&htim2);

    TIM_OC_InitTypeDef sConfig = {0};
    sConfig.OCMode     = TIM_OCMODE_PWM1;
    sConfig.Pulse      = BREATH_ARR;      // CCR=999 → 全高 → 共阳全灭
    sConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfig.OCFastMode = TIM_OCFAST_DISABLE;

    for (int i = 0; i < 4; i++)
    {
        HAL_TIM_PWM_ConfigChannel(&htim2, &sConfig, breath_ch[i]);
        HAL_TIM_PWM_Start(&htim2, breath_ch[i]);
    }
}

/**
 * 设置某一路亮度（0=灭，999=最亮）。
 * 共阳：亮度越高 → CCR 越小 → 低电平时间越长 → 越亮。
 */
void Breath_SetBrightness(uint8_t led, uint16_t brightness)
{
    __HAL_TIM_SET_COMPARE(&htim2, breath_ch[led], BREATH_ARR - brightness);
}
