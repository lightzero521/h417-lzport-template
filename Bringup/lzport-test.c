#include "lzport/serial.h"
#include "lzport/gpio.h"
#include "lzport/rtc.h"

#include "debug.h"

#define TEST_SERIAL_PORT       1U
#define TEST_RX_START          0U
#define TEST_RX_BUSY           1U
#define TEST_TX_START          2U
#define TEST_TX_BUSY           3U
#define TEST_STOPPED           4U

static uint8_t g_echo_buffer[256];
static volatile uint32_t g_echo_length;
static volatile uint8_t g_echo_state = TEST_STOPPED;
static uint8_t g_rtc_ready;
static uint8_t g_rtc_second = 0xFFU;

static void rx_done(uint8_t port, lzport_status status, uint32_t actual, void *user)
{
    (void)port;
    (void)user;
    g_echo_length = actual;
    g_echo_state = ((status == LZPORT_OK) && (actual != 0U)) ? TEST_TX_START : TEST_RX_START;
}

static void tx_done(uint8_t port, lzport_status status, uint32_t actual, void *user)
{
    (void)port;
    (void)status;
    (void)actual;
    (void)user;
    g_echo_state = TEST_RX_START;
}

void lzport_test_user(void)
{
    static const uint8_t message[] =
        "\r\nLZPort USART2 bringup, 460800 8N1.\r\n"
        "DMA echo ready.\r\n";
    const lzport_serial_config cfg = {
        .baud = 460800U,
        .data_bits = LZPORT_SERIAL_DATA_BITS_8,
        .parity = LZPORT_SERIAL_PARITY_NONE,
        .stop_bits = LZPORT_SERIAL_STOP_BITS_1,
        .tx = {LZPORT_GPIO_D, LZPORT_GPIO_PIN_5, LZPORT_GPIO_AF_7},
        .rx = {LZPORT_GPIO_D, LZPORT_GPIO_PIN_6, LZPORT_GPIO_AF_7},
    };
    lzport_status rtc_status;

    printf("lzport-test-user: gpio\r\n");
    lzport_gpio_mode_output(LZPORT_GPIO_C, LZPORT_GPIO_PIN_2,
                            LZPORT_GPIO_SPEED_LOW, LZPORT_GPIO_OPEN_DRAIN);
    lzport_gpio_mode_output(LZPORT_GPIO_C, LZPORT_GPIO_PIN_3,
                            LZPORT_GPIO_SPEED_LOW, LZPORT_GPIO_OPEN_DRAIN);
    lzport_gpio_write(LZPORT_GPIO_C, LZPORT_GPIO_PIN_2, LZPORT_GPIO_HIGH);
    lzport_gpio_write(LZPORT_GPIO_C, LZPORT_GPIO_PIN_3, LZPORT_GPIO_HIGH);
    lzport_gpio_toggle(LZPORT_GPIO_C, LZPORT_GPIO_PIN_2);
    Delay_Ms(300);
    lzport_gpio_toggle(LZPORT_GPIO_C, LZPORT_GPIO_PIN_2);
    lzport_gpio_toggle(LZPORT_GPIO_C, LZPORT_GPIO_PIN_3);
    Delay_Ms(300);
    lzport_gpio_toggle(LZPORT_GPIO_C, LZPORT_GPIO_PIN_3);

    rtc_status = lzport_rtc_init();
    g_rtc_ready = (rtc_status == LZPORT_OK) ? 1U : 0U;
    printf("lzport-test-user: rtc %s (%d)\r\n",
           g_rtc_ready ? "ok" : "failed", (int)rtc_status);
    if (g_rtc_ready == 0U) {
        printf("PWR_CTLR=%08x BDCTLR=%08x RSTSCKR=%08x RTC_CTLRL=%04x\r\n",
               (unsigned)PWR->CTLR, (unsigned)RCC->BDCTLR,
               (unsigned)RCC->RSTSCKR, (unsigned)RTC->CTLRL);
    }

    g_echo_state = TEST_STOPPED;
    if (lzport_serial_init(TEST_SERIAL_PORT, &cfg) != LZPORT_OK) {
        return;
    }
    (void)lzport_serial_write(TEST_SERIAL_PORT, message, sizeof(message) - 1U);
    if ((lzport_serial_dma_bind_tx(TEST_SERIAL_PORT, 0U, 0U, 87U) != LZPORT_OK) ||
        (lzport_serial_dma_bind_rx(TEST_SERIAL_PORT, 0U, 1U, 88U) != LZPORT_OK)) {
        return;
    }
    g_echo_state = TEST_RX_START;
}

void lzport_test_runner(void)
{
    lzport_rtc_datetime now;

    if ((g_rtc_ready != 0U) && (lzport_rtc_get(&now) == LZPORT_OK) &&
        (now.second != g_rtc_second)) {
        g_rtc_second = now.second;
        printf("rtc: %04u-%02u-%02u %02u:%02u:%02u.%03u\r\n",
               (unsigned)now.year, (unsigned)now.month, (unsigned)now.day,
               (unsigned)now.hour, (unsigned)now.minute,
               (unsigned)now.second, (unsigned)now.millisecond);
    }

    if (g_echo_state == TEST_RX_START) {
        g_echo_state = TEST_RX_BUSY;
        if (lzport_serial_read_async(TEST_SERIAL_PORT, g_echo_buffer,
                                     sizeof(g_echo_buffer), rx_done, 0) != LZPORT_OK) {
            g_echo_state = TEST_RX_START;
        }
    } else if (g_echo_state == TEST_TX_START) {
        g_echo_state = TEST_TX_BUSY;
        if (lzport_serial_write_async(TEST_SERIAL_PORT, g_echo_buffer,
                                      g_echo_length, tx_done, 0) != LZPORT_OK) {
            g_echo_state = TEST_RX_START;
        }
    }
}
