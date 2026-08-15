#include "lzport/pwm.h"

#include "ch32h417_rcc.h"
#include "ch32h417_tim.h"

typedef struct
{
    TIM_TypeDef *reg;
    uint32_t clock;
    uint8_t hb2;
    uint8_t bits32;
    uint8_t advanced;
} pwm_hw;

typedef struct
{
    uint32_t period;
    uint32_t frequency_hz;
    uint8_t configured;
    uint8_t active;
} pwm_state;

static const pwm_hw g_hw[LZPORT_PWM_TIMER_COUNT] = {
    {TIM1, RCC_HB2Periph_TIM1, 1U, 0U, 1U},
    {TIM2, RCC_HB1Periph_TIM2, 0U, 0U, 0U},
    {TIM3, RCC_HB1Periph_TIM3, 0U, 0U, 0U},
    {TIM4, RCC_HB1Periph_TIM4, 0U, 0U, 0U},
    {TIM5, RCC_HB1Periph_TIM5, 0U, 0U, 0U},
    {TIM8, RCC_HB2Periph_TIM8, 1U, 0U, 1U},
    {TIM9, RCC_HB2Periph_TIM9, 1U, 1U, 0U},
    {TIM10, RCC_HB2Periph_TIM10, 1U, 1U, 0U},
    {TIM11, RCC_HB2Periph_TIM11, 1U, 1U, 0U},
    {TIM12, RCC_HB2Periph_TIM12, 1U, 1U, 0U},
};

static const uint16_t g_channel[LZPORT_PWM_CHANNEL_COUNT] = {
    TIM_Channel_1, TIM_Channel_2, TIM_Channel_3, TIM_Channel_4,
};

static pwm_state g_state[LZPORT_PWM_TIMER_COUNT];

static int timebase_calculate(uint32_t frequency_hz, uint8_t bits32,
                              uint16_t *prescaler, uint32_t *period)
{
    RCC_ClocksTypeDef clocks;
    uint64_t max_counts = (bits32 != 0U) ? 0xFFFFFFFFULL : 65535ULL;
    uint64_t divider;
    uint64_t counts;

    RCC_GetClocksFreq(&clocks);
    if ((frequency_hz == 0U) || (frequency_hz > clocks.HCLK_Frequency)) {
        return 0;
    }
    divider = ((uint64_t)clocks.HCLK_Frequency +
               ((uint64_t)frequency_hz * max_counts) - 1U) /
              ((uint64_t)frequency_hz * max_counts);
    if ((divider == 0U) || (divider > 65536U)) {
        return 0;
    }
    counts = ((uint64_t)clocks.HCLK_Frequency +
              ((uint64_t)frequency_hz * divider / 2U)) /
             ((uint64_t)frequency_hz * divider);
    if ((counts == 0U) || (counts > max_counts)) {
        return 0;
    }
    *prescaler = (uint16_t)(divider - 1U);
    *period = (uint32_t)(counts - 1U);
    return 1;
}

static void channel_cmd(TIM_TypeDef *reg, lzport_pwm_channel channel,
                        FunctionalState state)
{
    TIM_CCxCmd(reg, g_channel[channel], state);
}

static void compare_set(const pwm_hw *hw, lzport_pwm_channel channel,
                        uint32_t value)
{
    if (hw->bits32 != 0U) {
        static void (*const set32[])(TIM_TypeDef *, uint32_t) = {
            TIM9_12_SetCompare1, TIM9_12_SetCompare2,
            TIM9_12_SetCompare3, TIM9_12_SetCompare4,
        };
        set32[channel](hw->reg, value);
    } else {
        static void (*const set16[])(TIM_TypeDef *, uint16_t) = {
            TIM_SetCompare1, TIM_SetCompare2, TIM_SetCompare3, TIM_SetCompare4,
        };
        set16[channel](hw->reg, (uint16_t)value);
    }
}

static void output_init(const pwm_hw *hw, lzport_pwm_channel channel,
                        uint32_t pulse)
{
    if (hw->bits32 != 0U) {
        TIM9_12_OCInitTypeDef init = {0};
        static void (*const init_fn[])(TIM_TypeDef *, TIM9_12_OCInitTypeDef *) = {
            TIM9_12_OC1Init, TIM9_12_OC2Init,
            TIM9_12_OC3Init, TIM9_12_OC4Init,
        };

        init.TIM_OCMode = TIM_OCMode_PWM1;
        init.TIM_OutputState = TIM_OutputState_Enable;
        init.TIM_Pulse = pulse;
        init.TIM_OCPolarity = TIM_OCPolarity_High;
        init_fn[channel](hw->reg, &init);
    } else {
        TIM_OCInitTypeDef init = {0};
        static void (*const init_fn[])(TIM_TypeDef *, TIM_OCInitTypeDef *) = {
            TIM_OC1Init, TIM_OC2Init, TIM_OC3Init, TIM_OC4Init,
        };

        init.TIM_OCMode = TIM_OCMode_PWM1;
        init.TIM_OutputState = TIM_OutputState_Enable;
        init.TIM_Pulse = (uint16_t)pulse;
        init.TIM_OCPolarity = TIM_OCPolarity_High;
        init_fn[channel](hw->reg, &init);
    }
}

lzport_status lzport_pwm_init(lzport_pwm_timer timer,
                              lzport_pwm_channel channel,
                              const lzport_pwm_config *cfg)
{
    const pwm_hw *hw;
    uint16_t prescaler;
    uint32_t period;
    uint32_t pulse;

    if (((uint32_t)timer >= LZPORT_PWM_TIMER_COUNT) ||
        ((uint32_t)channel >= LZPORT_PWM_CHANNEL_COUNT) || (cfg == 0) ||
        (cfg->duty_permille > 1000U) ||
        ((uint32_t)cfg->port >= LZPORT_GPIO_PORT_COUNT) ||
        ((uint32_t)cfg->pin >= LZPORT_GPIO_PIN_COUNT) ||
        ((uint32_t)cfg->af >= LZPORT_GPIO_AF_COUNT)) {
        return LZPORT_EINVAL;
    }
    hw = &g_hw[timer];
    if (!timebase_calculate(cfg->frequency_hz, hw->bits32,
                            &prescaler, &period)) {
        return LZPORT_EINVAL;
    }
    if ((g_state[timer].configured != 0U) &&
        (g_state[timer].frequency_hz != cfg->frequency_hz)) {
        return LZPORT_EBUSY;
    }

    if (hw->hb2 != 0U) {
        RCC_HB2PeriphClockCmd(hw->clock, ENABLE);
    } else {
        RCC_HB1PeriphClockCmd(hw->clock, ENABLE);
    }
    lzport_gpio_mode_af_output(cfg->port, cfg->pin, cfg->af,
                               LZPORT_GPIO_SPEED_VERY_HIGH,
                               LZPORT_GPIO_PUSH_PULL);
    if (g_state[timer].configured == 0U) {
        TIM_DeInit(hw->reg);
        if (hw->bits32 != 0U) {
            TIM9_12_TimeBaseInitTypeDef init = {0};
            init.TIM_Prescaler = prescaler;
            init.TIM_CounterMode = TIM_CounterMode_Up;
            init.TIM_Period = period;
            init.TIM_ClockDivision = TIM_CKD_DIV1;
            TIM9_12_TimeBaseInit(hw->reg, &init);
        } else {
            TIM_TimeBaseInitTypeDef init = {0};
            init.TIM_Prescaler = prescaler;
            init.TIM_CounterMode = TIM_CounterMode_Up;
            init.TIM_Period = (uint16_t)period;
            init.TIM_ClockDivision = TIM_CKD_DIV1;
            TIM_TimeBaseInit(hw->reg, &init);
        }
        TIM_ARRPreloadConfig(hw->reg, ENABLE);
        g_state[timer].period = period;
        g_state[timer].frequency_hz = cfg->frequency_hz;
    }
    pulse = (uint32_t)(((uint64_t)(period + 1U) * cfg->duty_permille) /
                       1000U);
    output_init(hw, channel, pulse);
    channel_cmd(hw->reg, channel, DISABLE);
    g_state[timer].configured |= (uint8_t)(1U << channel);
    return LZPORT_OK;
}

lzport_status lzport_pwm_deinit(lzport_pwm_timer timer,
                                lzport_pwm_channel channel)
{
    const pwm_hw *hw;

    if (((uint32_t)timer >= LZPORT_PWM_TIMER_COUNT) ||
        ((uint32_t)channel >= LZPORT_PWM_CHANNEL_COUNT) ||
        ((g_state[timer].configured & (1U << channel)) == 0U)) {
        return LZPORT_EINVAL;
    }
    hw = &g_hw[timer];
    channel_cmd(hw->reg, channel, DISABLE);
    g_state[timer].configured &= (uint8_t)~(1U << channel);
    g_state[timer].active &= (uint8_t)~(1U << channel);
    if (g_state[timer].configured == 0U) {
        TIM_Cmd(hw->reg, DISABLE);
        TIM_DeInit(hw->reg);
        if (hw->hb2 != 0U) {
            RCC_HB2PeriphClockCmd(hw->clock, DISABLE);
        } else {
            RCC_HB1PeriphClockCmd(hw->clock, DISABLE);
        }
        g_state[timer].frequency_hz = 0U;
        g_state[timer].period = 0U;
    }
    return LZPORT_OK;
}

lzport_status lzport_pwm_start(lzport_pwm_timer timer,
                               lzport_pwm_channel channel)
{
    const pwm_hw *hw;

    if (((uint32_t)timer >= LZPORT_PWM_TIMER_COUNT) ||
        ((uint32_t)channel >= LZPORT_PWM_CHANNEL_COUNT) ||
        ((g_state[timer].configured & (1U << channel)) == 0U)) {
        return LZPORT_EINVAL;
    }
    hw = &g_hw[timer];
    channel_cmd(hw->reg, channel, ENABLE);
    if (hw->advanced != 0U) {
        TIM_CtrlPWMOutputs(hw->reg, ENABLE);
    }
    g_state[timer].active |= (uint8_t)(1U << channel);
    TIM_Cmd(hw->reg, ENABLE);
    return LZPORT_OK;
}

lzport_status lzport_pwm_stop(lzport_pwm_timer timer,
                              lzport_pwm_channel channel)
{
    const pwm_hw *hw;

    if (((uint32_t)timer >= LZPORT_PWM_TIMER_COUNT) ||
        ((uint32_t)channel >= LZPORT_PWM_CHANNEL_COUNT) ||
        ((g_state[timer].configured & (1U << channel)) == 0U)) {
        return LZPORT_EINVAL;
    }
    hw = &g_hw[timer];
    channel_cmd(hw->reg, channel, DISABLE);
    g_state[timer].active &= (uint8_t)~(1U << channel);
    if (g_state[timer].active == 0U) {
        TIM_Cmd(hw->reg, DISABLE);
        if (hw->advanced != 0U) {
            TIM_CtrlPWMOutputs(hw->reg, DISABLE);
        }
    }
    return LZPORT_OK;
}

lzport_status lzport_pwm_set_duty(lzport_pwm_timer timer,
                                  lzport_pwm_channel channel,
                                  uint16_t duty_permille)
{
    uint32_t pulse;

    if (((uint32_t)timer >= LZPORT_PWM_TIMER_COUNT) ||
        ((uint32_t)channel >= LZPORT_PWM_CHANNEL_COUNT) ||
        ((g_state[timer].configured & (1U << channel)) == 0U) ||
        (duty_permille > 1000U)) {
        return LZPORT_EINVAL;
    }
    pulse = (uint32_t)(((uint64_t)(g_state[timer].period + 1U) *
                        duty_permille) /
                       1000U);
    compare_set(&g_hw[timer], channel, pulse);
    return LZPORT_OK;
}
