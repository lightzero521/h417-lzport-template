# Changelog

## 2026-08-16

- RTC 晶振启动等待改用 100 MHz 软件延时，不再占用 SysTick。
- GPIO 外部中断改为配置即启用，移除单独的启停接口。
- 暂时移除 ADC 和 PWM 硬件实现，保留 API 占位函数。
- 移除裸 Flash API，存储操作交由 KV 或 OTA 组件管理。

## 2026-08-15

- 建立 CH32H417 V3F/V5F 双核 bring-up 工程。
- 加入 lzport GPIO、串口、SPI、I2C、RTC、定时器和 RNG 适配。
- 加入 GPIO 外部中断和 IWDG 适配。
