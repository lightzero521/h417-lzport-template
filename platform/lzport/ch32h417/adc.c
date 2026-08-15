#include "lzport/adc.h"

#include "ch32h417_adc.h"
#include "ch32h417_rcc.h"

#define ADC_CALIBRATION_TIMEOUT 100000U

static ADC_TypeDef *const g_adc[LZPORT_ADC_COUNT] = {ADC1, ADC2};
static const uint32_t g_clock[LZPORT_ADC_COUNT] = {
    RCC_HB2Periph_ADC1, RCC_HB2Periph_ADC2,
};
static uint8_t g_initialized;
static uint8_t g_internal;

lzport_status lzport_adc_init(lzport_adc adc, const lzport_adc_config *cfg)
{
    ADC_InitTypeDef init = {0};
    uint32_t timeout;

    if (((uint32_t)adc >= LZPORT_ADC_COUNT) || (cfg == 0) ||
        (cfg->channel > 17U) ||
        ((uint32_t)cfg->sample_time >= LZPORT_ADC_SAMPLE_COUNT) ||
        ((cfg->channel < 16U) &&
         (((uint32_t)cfg->port >= LZPORT_GPIO_PORT_COUNT) ||
          ((uint32_t)cfg->pin >= LZPORT_GPIO_PIN_COUNT)))) {
        return LZPORT_EINVAL;
    }
    if ((g_initialized & (1U << adc)) != 0U) {
        lzport_adc_deinit(adc);
    }

    RCC_HB2PeriphClockCmd(g_clock[adc], ENABLE);
    RCC_ADCCLKConfig(RCC_ADCCLKSource_HCLK);
    RCC->CFGR0 = (RCC->CFGR0 & ~RCC_ADCPRE) |
                 ((uint32_t)RCC_HCLK_ADCPRE_DIV8 << 14);
    if (cfg->channel < 16U) {
        lzport_gpio_mode_analog(cfg->port, cfg->pin);
    } else {
        ADC_TempSensorVrefintCmd(ENABLE);
    }

    ADC_DeInit(g_adc[adc]);
    init.ADC_Mode = ADC_Mode_Independent;
    init.ADC_ScanConvMode = DISABLE;
    init.ADC_ContinuousConvMode = DISABLE;
    init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    init.ADC_DataAlign = ADC_DataAlign_Right;
    init.ADC_NbrOfChannel = 1U;
    init.ADC_OutputBuffer = ADC_OutputBuffer_Disable;
    init.ADC_Pga = ADC_Pga_1;
    ADC_Init(g_adc[adc], &init);
    ADC_LowPowerModeCmd(g_adc[adc], DISABLE);
    ADC_SMP_ModeConfig(g_adc[adc], cfg->channel, ADC_SMP_CFG_MODE1);
    ADC_Cmd(g_adc[adc], ENABLE);
    ADC_BufferCmd(g_adc[adc], DISABLE);

    ADC_ResetCalibration(g_adc[adc]);
    timeout = ADC_CALIBRATION_TIMEOUT;
    while ((ADC_GetResetCalibrationStatus(g_adc[adc]) != RESET) &&
           (timeout-- != 0U)) {
    }
    if (ADC_GetResetCalibrationStatus(g_adc[adc]) != RESET) {
        ADC_Cmd(g_adc[adc], DISABLE);
        RCC_HB2PeriphClockCmd(g_clock[adc], DISABLE);
        if ((cfg->channel >= 16U) && (g_internal == 0U)) {
            ADC_TempSensorVrefintCmd(DISABLE);
        }
        return LZPORT_ETIMEOUT;
    }
    ADC_StartCalibration(g_adc[adc]);
    timeout = ADC_CALIBRATION_TIMEOUT;
    while ((ADC_GetCalibrationStatus(g_adc[adc]) != RESET) &&
           (timeout-- != 0U)) {
    }
    if (ADC_GetCalibrationStatus(g_adc[adc]) != RESET) {
        ADC_Cmd(g_adc[adc], DISABLE);
        RCC_HB2PeriphClockCmd(g_clock[adc], DISABLE);
        if ((cfg->channel >= 16U) && (g_internal == 0U)) {
            ADC_TempSensorVrefintCmd(DISABLE);
        }
        return LZPORT_ETIMEOUT;
    }
    ADC_RegularChannelConfig(g_adc[adc], cfg->channel, 1U,
                             (uint8_t)cfg->sample_time);
    g_initialized |= (uint8_t)(1U << adc);
    if (cfg->channel >= 16U) {
        g_internal |= (uint8_t)(1U << adc);
    } else {
        g_internal &= (uint8_t)~(1U << adc);
    }
    return LZPORT_OK;
}

lzport_status lzport_adc_deinit(lzport_adc adc)
{
    if (((uint32_t)adc >= LZPORT_ADC_COUNT) ||
        ((g_initialized & (1U << adc)) == 0U)) {
        return LZPORT_EINVAL;
    }
    ADC_Cmd(g_adc[adc], DISABLE);
    ADC_DeInit(g_adc[adc]);
    RCC_HB2PeriphClockCmd(g_clock[adc], DISABLE);
    g_initialized &= (uint8_t)~(1U << adc);
    g_internal &= (uint8_t)~(1U << adc);
    if (g_internal == 0U) {
        ADC_TempSensorVrefintCmd(DISABLE);
    }
    return LZPORT_OK;
}

lzport_status lzport_adc_read(lzport_adc adc, uint16_t *value,
                              uint32_t timeout_cycles)
{
    if (((uint32_t)adc >= LZPORT_ADC_COUNT) ||
        ((g_initialized & (1U << adc)) == 0U) ||
        (value == 0) || (timeout_cycles == 0U)) {
        return LZPORT_EINVAL;
    }
    ADC_ClearFlag(g_adc[adc], ADC_FLAG_EOC);
    ADC_SoftwareStartConvCmd(g_adc[adc], ENABLE);
    while (ADC_GetFlagStatus(g_adc[adc], ADC_FLAG_EOC) == RESET) {
        if (timeout_cycles-- == 0U) {
            return LZPORT_ETIMEOUT;
        }
    }
    *value = ADC_GetConversionValue(g_adc[adc]);
    return LZPORT_OK;
}
