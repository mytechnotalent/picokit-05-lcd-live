/**
 * FILE: test_picokit_05_lcd_live_and_security.c
 *
 * DESCRIPTION:
 * Native unit tests for the RP2350 Picokit traffic light node:
 * provisioning constants, the packet artifact, CRC, the RYLR998 AT
 * interface, and the traffic light phase state machine that pairs each
 * phase with an authenticated LoRa heartbeat.
 *
 * BRIEF:
 * Native unit test runner for picokit-05-lcd-live.
 *
 * AUTHOR: Kevin Thomas
 * DATE: September 2026
 */

#include "harness.h"
#include "mock/pico/stdlib.h"
#include "mock/pico/time.h"
#include "mock/hardware/gpio.h"
#include "mock/hardware/i2c.h"
#include "mock/hardware/uart.h"
#include "picokit_05_lcd_live.h"
#include "crc.h"
#include "radio.h"
#include "monitor.h"
#include "status_led.h"
#include <string.h>

#include "../src/crc.c"
#include "../src/radio.c"
#include "../src/monitor.c"

/**
 * @brief File-scope UART transmit capture buffer.
 */
static char s_tx[2048];

/**
 * @brief File-scope decoded inbound radio report.
 */
static radio_rcv_t s_rcv;

/**
 * @brief Reset every host mock peripheral.
 *
 * @param void No parameters.
 * @return void
 */
static void reset_all(void) {
    mock_timer_reset();
    mock_gpio_reset();
    mock_uart_reset();
}

/**
 * @brief Initialize the monitor and clear its provisioning UART traffic.
 *
 * @param void No parameters.
 * @return void
 */
static void init_monitor(void) {
    reset_all();
    TEST_ASSERT_TRUE(monitor_init());
    mock_uart_reset();
}

/**
 * @brief Service one monitor tick at an absolute fake-clock time.
 *
 * @param when_us Absolute fake-clock time in microseconds.
 * @return void
 */
static void step_at(uint64_t when_us) {
    mock_timer_set_us(when_us);
    monitor_step();
}

/**
 * @brief Assert the GPIO pin and traffic provisioning constants.
 *
 * @param void No parameters.
 * @return void
 */
static void assert_pin_constants(void) {
    TEST_ASSERT_EQUAL_UINT(25u, PICOKIT_05_LCD_LIVE_LED_PIN);
    TEST_ASSERT_EQUAL_UINT(8u, PICOKIT_05_LCD_LIVE_UART_TX);
    TEST_ASSERT_EQUAL_UINT(9u, PICOKIT_05_LCD_LIVE_UART_RX);
    TEST_ASSERT_EQUAL_UINT(16u, PICOKIT_05_LCD_LIVE_RED_LED_PIN);
    TEST_ASSERT_EQUAL_UINT(17u, PICOKIT_05_LCD_LIVE_YELLOW_LED_PIN);
    TEST_ASSERT_EQUAL_UINT(18u, PICOKIT_05_LCD_LIVE_GREEN_LED_PIN);
}

/**
 * @brief Assert the UART and frame provisioning constants.
 *
 * @param void No parameters.
 * @return void
 */
static void assert_frame_constants(void) {
    TEST_ASSERT_EQUAL_UINT(115200u, PICOKIT_05_LCD_LIVE_UART_BAUD);
    TEST_ASSERT_EQUAL_UINT(48u, PICOKIT_05_LCD_LIVE_FRAME_SIZE);
    TEST_ASSERT_EQUAL_UINT(5000u, PICOKIT_05_LCD_LIVE_TX_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT(3u, STATUS_LED_STEP_COUNT);
}

/**
 * @brief Assert the packet artifact identity constants.
 *
 * @param void No parameters.
 * @return void
 */
static void assert_artifact_ids(void) {
    TEST_ASSERT_EQUAL_UINT(1u, PACKET_FRAME_VERSION);
    TEST_ASSERT_EQUAL_UINT(5u, PACKET_NODE_ID);
    TEST_ASSERT_EQUAL_HEX16(0x0001u, PACKET_HUB_ADDRESS);
    TEST_ASSERT_EQUAL_UINT(48u, PACKET_FRAME_SIZE);
    TEST_ASSERT_EQUAL_UINT(5000u, PACKET_TX_INTERVAL_MS);
}

/**
 * @brief Assert the packet artifact example heartbeat frame.
 *
 * @param void No parameters.
 * @return void
 */
static void assert_artifact_example(void) {
    TEST_ASSERT_EQUAL_UINT(48u, (unsigned)sizeof(PACKET_EXAMPLE_FRAME));
    TEST_ASSERT_EQUAL_UINT8(0x7Bu, PACKET_EXAMPLE_FRAME[0]);
    TEST_ASSERT_EQUAL_UINT8(0x35u, PACKET_EXAMPLE_FRAME[5]);
    TEST_ASSERT_EQUAL_UINT8(0x30u, PACKET_EXAMPLE_FRAME[17]);
    TEST_ASSERT_EQUAL_UINT8(0x7Du, PACKET_EXAMPLE_FRAME[18]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, PACKET_EXAMPLE_FRAME[19]);
}

/**
 * @brief Parse the canonical comma-laden JSON +RCV line.
 *
 * @param void No parameters.
 * @return radio_result_t Parsed result code.
 */
static radio_result_t parse_json_rcv(void) {
    return radio_parse_rcv("+RCV=0002,10,{\"cmd\":91},-78,5", &s_rcv);
}

/**
 * @brief Assert +RCV parsing of a comma-laden JSON payload.
 *
 * @param void No parameters.
 * @return void
 */
static void assert_rcv_json(void) {
    TEST_ASSERT_EQUAL(RADIO_RESULT_OK, parse_json_rcv());
    TEST_ASSERT_EQUAL_HEX16(0x0002u, s_rcv.sender);
    TEST_ASSERT_EQUAL_UINT(10u, (unsigned)s_rcv.len);
    TEST_ASSERT_EQUAL_STRING("{\"cmd\":91}", s_rcv.payload);
    TEST_ASSERT_EQUAL_INT(-78, s_rcv.rssi);
    TEST_ASSERT_EQUAL_INT(5, s_rcv.snr);
}

/**
 * @brief Assert +RCV parsing of a short comma-bearing payload.
 *
 * @param void No parameters.
 * @return void
 */
static void assert_rcv_comma(void) {
    TEST_ASSERT_EQUAL(RADIO_RESULT_OK, radio_parse_rcv("+RCV=0008,9,{\"a\",\"b\"},-60,3", &s_rcv));
    TEST_ASSERT_EQUAL_HEX16(0x0008u, s_rcv.sender);
    TEST_ASSERT_EQUAL_STRING("{\"a\",\"b\"}", s_rcv.payload);
}

/**
 * @brief Assert +RCV parsing of a frame with no RSSI/SNR tail.
 *
 * @param void No parameters.
 * @return void
 */
static void assert_rcv_no_tail(void) {
    TEST_ASSERT_EQUAL(RADIO_RESULT_OK, radio_parse_rcv("+RCV=0002,2,ok", &s_rcv));
    TEST_ASSERT_EQUAL_INT(0, s_rcv.rssi);
    TEST_ASSERT_EQUAL_INT(0, s_rcv.snr);
}

/**
 * @brief Assert +RCV rejection of malformed, oversized, and null lines.
 *
 * @param void No parameters.
 * @return void
 */
static void assert_rcv_rejects(void) {
    TEST_ASSERT_EQUAL(RADIO_RESULT_PARSE_ERROR, radio_parse_rcv("AT+SEND=0001,3,abc", &s_rcv));
    TEST_ASSERT_EQUAL(RADIO_RESULT_OVERSIZE, radio_parse_rcv("+RCV=0001,300,abcdef", &s_rcv));
    TEST_ASSERT_EQUAL(RADIO_RESULT_PARSE_ERROR, radio_parse_rcv("+RCV=0001,5,abc", &s_rcv));
    TEST_ASSERT_EQUAL(RADIO_RESULT_PARSE_ERROR, radio_parse_rcv(NULL, &s_rcv));
    TEST_ASSERT_EQUAL(RADIO_RESULT_PARSE_ERROR, radio_parse_rcv("+RCV=0001,1,a", NULL));
}

/**
 * @brief Assert the inbound line pump consumes two CRLF-terminated lines.
 *
 * @param line Pointer to line buffer.
 * @param len Pointer to accumulated length.
 * @return void
 */
static void assert_pump_lines(char *line, size_t *len) {
    TEST_ASSERT_TRUE(radio_line_pump(uart0, line, len));
    TEST_ASSERT_EQUAL_STRING("ab", line);
    TEST_ASSERT_TRUE(radio_line_pump(uart0, line, len));
    TEST_ASSERT_EQUAL_STRING("cd", line);
    TEST_ASSERT_FALSE(radio_line_pump(uart0, line, len));
}

/**
 * @brief Locate the hex payload after the AT+SEND address and length fields.
 *
 * @param void No parameters.
 * @return char* Pointer to the hex payload inside the captured frame.
 */
static char *tx_hex_start(void) {
    char *send = strstr(s_tx, "AT+SEND=");
    char *first = strchr(send, ',');
    char *second = strchr(first + 1, ',');
    return second + 1;
}

/**
 * @brief Trim the trailing CRLF from the captured hex payload.
 *
 * @param hex Pointer to the mutable hex payload.
 * @return size_t Number of remaining hex characters.
 */
static size_t tx_hex_len(char *hex) {
    size_t n = strlen(hex);
    while (n > 0u && (hex[n - 1u] == '\r' || hex[n - 1u] == '\n')) {
        n -= 1u;
    }
    hex[n] = '\0';
    return n;
}

/**
 * @brief Report whether every character is a lowercase hex digit.
 *
 * @param hex Pointer to the NUL-terminated candidate text.
 * @return bool true when the text is entirely lowercase hexadecimal.
 */
static bool tx_all_hex(const char *hex) {
    size_t i;
    for (i = 0u; hex[i] != '\0'; ++i) {
        if (hex_digit(hex[i]) < 0 || (hex[i] >= 'A' && hex[i] <= 'F')) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Assert the transmitted payload is an even-length lowercase hex envelope.
 *
 * @param void No parameters.
 * @return void
 */
static void assert_hex_payload(void) {
    char *hex = tx_hex_start();
    size_t hex_len = tx_hex_len(hex);
    TEST_ASSERT_TRUE(hex_len > 48u);
    TEST_ASSERT_TRUE((hex_len % 2u) == 0u);
    TEST_ASSERT_TRUE(tx_all_hex(hex));
}

void test_config_constants(void) {
    assert_pin_constants();
    assert_frame_constants();
}

void test_packet_artifact_constants(void) {
    assert_artifact_ids();
    assert_artifact_example();
}

void test_crc16_ccitt(void) {
    TEST_ASSERT_EQUAL_HEX16(0xFFFFu, crc16_ccitt((const uint8_t *)"", 0u));
    TEST_ASSERT_EQUAL_HEX16(0x29B1u, crc16_ccitt((const uint8_t *)"123456789", 9u));
}

void test_radio_build_send_cmd(void) {
    char cmd[64];
    TEST_ASSERT_EQUAL(RADIO_RESULT_OK, radio_build_send_cmd(0x0001u, (const uint8_t *)"abc", 3u, cmd, sizeof(cmd)));
    TEST_ASSERT_EQUAL_STRING("AT+SEND=0001,3,abc\r\n", cmd);
    TEST_ASSERT_EQUAL(RADIO_RESULT_OVERSIZE, radio_build_send_cmd(0x0001u, (const uint8_t *)"abc", 257u, cmd, sizeof(cmd)));
    TEST_ASSERT_EQUAL(RADIO_RESULT_PARSE_ERROR, radio_build_send_cmd(0x0001u, NULL, 3u, cmd, sizeof(cmd)));
}

void test_radio_build_send_cmd_oversize_cmd(void) {
    char tiny[16];
    TEST_ASSERT_EQUAL(RADIO_RESULT_OVERSIZE, radio_build_send_cmd(0x0001u, (const uint8_t *)"abcdefghijklmnopqrst", 20u, tiny, sizeof(tiny)));
}

void test_radio_send_frame_oversize(void) {
    TEST_ASSERT_EQUAL(RADIO_RESULT_OVERSIZE, radio_send_frame(uart0, (const uint8_t *)"x", 257u));
}

void test_radio_parse_rcv(void) {
    assert_rcv_json();
    assert_rcv_comma();
    assert_rcv_no_tail();
}

void test_radio_parse_rcv_rejects(void) {
    assert_rcv_rejects();
}

void test_radio_line_pump(void) {
    char line[32];
    size_t len = 0u;
    mock_uart_reset();
    mock_uart_set_rx("ab\r\ncd\r\n", 8u);
    assert_pump_lines(line, &len);
}

void test_radio_hex_digits(void) {
    TEST_ASSERT_EQUAL(RADIO_RESULT_OK, radio_parse_rcv("+RCV=00a7,2,ok,-3,2", &s_rcv));
    TEST_ASSERT_EQUAL_HEX16(0x00A7u, s_rcv.sender);
    TEST_ASSERT_EQUAL(RADIO_RESULT_OK, radio_parse_rcv("+RCV=00FE,2,ok,-3,2", &s_rcv));
    TEST_ASSERT_EQUAL_HEX16(0x00FEu, s_rcv.sender);
}

void test_radio_parse_missing_commas(void) {
    TEST_ASSERT_EQUAL(RADIO_RESULT_PARSE_ERROR, radio_parse_rcv("+RCV=007ZX,3,hi,-1,1", &s_rcv));
    TEST_ASSERT_EQUAL(RADIO_RESULT_PARSE_ERROR, radio_parse_rcv("+RCV=0002,9Z,hi,-1,1", &s_rcv));
}

void test_radio_spoofed_sender_attribution(void) {
    TEST_ASSERT_EQUAL(RADIO_RESULT_OK, radio_parse_rcv("+RCV=0002,7,{\"a\",1},-90,3", &s_rcv));
    TEST_ASSERT_TRUE(radio_frame_is_from(&s_rcv, 0x0002u));
    TEST_ASSERT_FALSE(radio_frame_is_from(&s_rcv, 0x0008u));
}

void test_monitor_bus_fail(void) {
    reset_all();
    mock_i2c_reset();
    mock_i2c_set_write_fail(true);
    monitor_bus_init();
    TEST_ASSERT_EQUAL_UINT(0u, (unsigned)mock_i2c_log_count());
    mock_i2c_set_write_fail(false);
}

void test_monitor_build_frame_json(void) {
    char frame[PICOKIT_05_LCD_LIVE_FRAME_SIZE];
    g_seq = 12u;
    g_count = 7u;
    monitor_build_frame(frame, sizeof(frame));
    TEST_ASSERT_EQUAL_STRING("{\"n\":5,\"s\":12,\"c\":7}", frame);
}

void test_monitor_init(void) {
    reset_all();
    TEST_ASSERT_TRUE(monitor_init());
    TEST_ASSERT_EQUAL_UINT(115200u, s_mock_uart_baud);
    TEST_ASSERT_TRUE(s_mock_gpio_dirs[PICOKIT_05_LCD_LIVE_LED_PIN]);
}

void test_monitor_refresh(void) {
    uint64_t base;
    init_monitor();
    base = mock_timer_now_us();
    step_at(base);
    TEST_ASSERT_EQUAL_UINT(1u, (unsigned)g_count);
    step_at(base + 1000000u);
    TEST_ASSERT_EQUAL_UINT(2u, (unsigned)g_count);
}

void test_monitor_transmit_frame(void) {
    size_t tx_len;
    init_monitor();
    step_at(mock_timer_now_us() + 5000000u);
    tx_len = mock_uart_get_tx(s_tx, sizeof(s_tx) - 1u);
    s_tx[tx_len] = '\0';
    TEST_ASSERT_TRUE(strstr(s_tx, "AT+SEND=0001,") != NULL);
    assert_hex_payload();
}

void test_monitor_transmit_no_key(void) {
    init_monitor();
    g_key_ready = false;
    mock_uart_reset();
    monitor_transmit();
    TEST_ASSERT_EQUAL_UINT(0u, (unsigned)mock_uart_get_tx(s_tx, sizeof(s_tx) - 1u));
    g_key_ready = true;
}

void test_monitor_not_ready(void) {
    monitor_deinit();
    TEST_ASSERT_FALSE(monitor_step());
}

void test_monitor_step_rx(void) {
    init_monitor();
    mock_uart_set_rx("+RCV=0002,10,{\"cmd\":91},-58,4\r\n", sizeof("+RCV=0002,10,{\"cmd\":91},-58,4\r\n") - 1u);
    step_at(mock_timer_now_us());
    TEST_ASSERT_EQUAL_UINT((unsigned)s_mock_rx_len, (unsigned)s_mock_rx_pos);
}

void setUp(void) {
    reset_all();
}

void tearDown(void) {
}

/**
 * @brief Run the provisioning, artifact, and CRC tests.
 *
 * @param void No parameters.
 * @return void
 */
static void run_basic_tests(void) {
    RUN_TEST(test_config_constants);
    RUN_TEST(test_packet_artifact_constants);
    RUN_TEST(test_crc16_ccitt);
}

/**
 * @brief Run the radio protocol tests.
 *
 * @param void No parameters.
 * @return void
 */
static void run_radio_tests(void) {
    RUN_TEST(test_radio_build_send_cmd);
    RUN_TEST(test_radio_build_send_cmd_oversize_cmd);
    RUN_TEST(test_radio_send_frame_oversize);
    RUN_TEST(test_radio_parse_rcv);
    RUN_TEST(test_radio_parse_rcv_rejects);
}

/**
 * @brief Run the inbound line and spoof-surface radio tests.
 *
 * @param void No parameters.
 * @return void
 */
static void run_radio_edge_tests(void) {
    RUN_TEST(test_radio_line_pump);
    RUN_TEST(test_radio_hex_digits);
    RUN_TEST(test_radio_parse_missing_commas);
    RUN_TEST(test_radio_spoofed_sender_attribution);
}

/**
 * @brief Run the traffic light monitor state-machine tests.
 *
 * @param void No parameters.
 * @return void
 */
static void run_monitor_tests(void) {
    RUN_TEST(test_monitor_build_frame_json);
    RUN_TEST(test_monitor_init);
    RUN_TEST(test_monitor_refresh);
    RUN_TEST(test_monitor_transmit_frame);
    RUN_TEST(test_monitor_transmit_no_key);
    RUN_TEST(test_monitor_not_ready);
    RUN_TEST(test_monitor_step_rx);
}

/**
 * @brief Run the LCD bus failure test group.
 *
 * @param void No parameters.
 * @return void
 */
static void run_lcd_bus_tests(void) {
    RUN_TEST(test_monitor_bus_fail);
}

/**
 * @brief Run the peripheral and security module test groups.
 *
 * @param void No parameters.
 * @return void
 */
extern void run_peripheral_and_crypto_tests(void);

int main(void) {
    TEST_BEGIN();
    run_basic_tests();
    run_radio_tests();
    run_radio_edge_tests();
    run_monitor_tests();
    run_lcd_bus_tests();
    run_peripheral_and_crypto_tests();
    return TEST_END();
}
