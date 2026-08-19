/**
 * 呼吸灯共享模块 —— 头文件
 *
 * 对外提供：
 *   - Breath_Init()            初始化 PA0(A0) 为 TIM2 硬件 PWM 并启动
 *   - Breath_SetBrightness()   设置某一路亮度（0=灭，999=最亮）
 *
 * 硬件背景：
 *   - A0=PA0 是 TIM2 的 CH1（默认映射，无需 AFIO 重映射即可输出 PWM）；
 *   - 用硬件 PWM 输出占空比可变的方波，占空比 = 亮度；
 *   - 共阳 LED 低电平点亮，所以亮度要反相：CCR = ARR - 亮度。
 */
#ifndef BREATH_H
#define BREATH_H

#include "stm32f1xx_hal.h"

// A0=PA0、A1=PA1、A2=PA2、A3=PA3，四个脚合成一个掩码（本工程只用 A0）
#define LED_ALL  (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3)

// TIM2 计数参数：PWM 频率 = 72MHz / (PSC+1) / (ARR+1) = 1kHz
#define BREATH_PSC     71    // 预分频：72MHz / 72 = 1MHz
#define BREATH_ARR     999   // 自动重载：1MHz / 1000 = 1kHz，亮度 1000 档
#define SIN_N          128   // 呼吸查找表点数（一个完整周期）
#define BREATH_STEP_MS 30    // 每步间隔：128 × 30ms ≈ 3.8s 一个呼吸周期

// 呼吸亮度表（暗→亮→暗），raised-cosine 曲线
extern const uint16_t breath_lut[SIN_N];

// PWM 通道数组，依次对应 A0~A3（TIM2_CH1~CH4，本工程只用第 0 路）
extern const uint32_t breath_ch[4];

extern TIM_HandleTypeDef htim2;

void Breath_Init(void);
void Breath_SetBrightness(uint8_t led, uint16_t brightness);

#endif /* BREATH_H */
