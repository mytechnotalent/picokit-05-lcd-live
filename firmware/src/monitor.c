// MIT License
//
// Copyright (c) 2026 Kevin Thomas
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Author:  Kevin Thomas
// Email:   kevin@mytechnotalent.com
// GitHub:  https://github.com/mytechnotalent/picokit-05-lcd-live
// File:    monitor.c
// Desc:    Implements the 1602 live LCD state machine that pairs the
//          display refresh with an authenticated LoRa heartbeat.
// Created: 2026

#include "picokit_05_lcd_live.h"
#include "monitor.h"
#include "display.h"
#include "radio.h"
#include "status_led.h"
#include "crypto_aead.h"
#include "crypto_kdf.h"
#include "envelope.h"
#include "field_secrets.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/time.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Module-ready flag.
 *
 * Set to true by monitor_init() once the peripherals are configured.
 * monitor_step() returns false while this flag is clear.
 */
static bool g_ready;

/**
 * @brief Number of completed LCD refresh steps.
 */
static uint16_t g_count;

/**
 * @brief First live LCD render line buffer.
 */
static char g_line1[DISPLAY_LINE_LEN];

/**
 * @brief Second live LCD render line buffer.
 */
static char g_line2[DISPLAY_LINE_LEN];

/**
 * @brief Monotonic transmit sequence number.
 */
static uint16_t g_seq;

/**
 * @brief Absolute time in microseconds of the next LCD refresh.
 */
static uint64_t g_next_step_us;

/**
 * @brief Absolute time in microseconds of the next authenticated transmit.
 */
static uint64_t g_next_tx_us;

/**
 * @brief Inbound radio line accumulator.
 */
static char g_rx_line[RADIO_LINE_BUF_LEN];

/**
 * @brief Number of bytes currently held in the inbound line accumulator.
 */
static size_t g_rx_len;

/**
 * @brief Derived XChaCha20-Poly1305 session key for telemetry.
 */
static uint8_t g_key[CRYPTO_AEAD_KEY_LEN];

/**
 * @brief True once the telemetry session key has been derived.
 */
static bool g_key_ready;

/**
 * @brief Probe one I2C address and report whether it acknowledges.
 *
 * @param i2c Pointer to the I2C peripheral to probe.
 * @param addr The 7-bit address to probe.
 * @return bool true when the address acknowledged.
 */
static bool i2c_probe(i2c_inst_t *i2c, uint8_t addr) {
    uint8_t dummy = 0u;
    if (i2c_write_blocking(i2c, addr, &dummy, 1u, false) < 0) {
        return false;
    }
    printf("  found 0x%02X\n", (unsigned)addr);
    return true;
}

/**
 * @brief Probe the I2C bus and print every device that acknowledges.
 *
 * @param i2c Pointer to the I2C peripheral to scan.
 * @return void
 */
static void i2c_bus_scan(i2c_inst_t *i2c) {
    uint8_t addr;
    uint8_t found = 0u;
    printf("I2C scan:\n");
    for (addr = 0x08u; addr < 0x78u; ++addr) {
        found += i2c_probe(i2c, addr) ? 1u : 0u;
    }
    if (found == 0u) {
        printf("  no devices\n");
    }
}

/**
 * @brief Initialize the I2C bus pins and scan the bus.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_bus_init(void) {
    i2c_init(PICOKIT_05_LCD_LIVE_I2C, PICOKIT_05_LCD_LIVE_I2C_BAUD);
    gpio_set_function(PICOKIT_05_LCD_LIVE_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PICOKIT_05_LCD_LIVE_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PICOKIT_05_LCD_LIVE_I2C_SDA);
    gpio_pull_up(PICOKIT_05_LCD_LIVE_I2C_SCL);
    i2c_bus_scan(PICOKIT_05_LCD_LIVE_I2C);
}

/**
 * @brief Configure the onboard heartbeat LED as a dark output.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_state_init_io(void) {
    gpio_init(PICOKIT_05_LCD_LIVE_LED_PIN);
    gpio_set_dir(PICOKIT_05_LCD_LIVE_LED_PIN, GPIO_OUT);
    gpio_put(PICOKIT_05_LCD_LIVE_LED_PIN, 0);
}

/**
 * @brief Reset the count, sequence, and transmit timing.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_state_init(void) {
    uint64_t now_us = time_us_64();
    g_count = 0u;
    g_seq = 0u;
    g_next_step_us = now_us;
    g_next_tx_us = now_us + (uint64_t)PICOKIT_05_LCD_LIVE_TX_INTERVAL_MS * 1000u;
    g_ready = true;
}

/**
 * @brief Derive the telemetry session key from the field secret.
 *
 * LAB-ONLY: production must provision the session key through OTP rather
 * than deriving it from a committed passphrase and salt.
 *
 * @param void No parameters.
 * @return bool true when the session key was derived.
 */
static bool monitor_derive_key(void) {
    bool ok = crypto_kdf_argon2id((const uint8_t *)FIELD_SECRET_PASSPHRASE, strlen(FIELD_SECRET_PASSPHRASE), FIELD_SECRET_SALT, 16u, g_key);
    g_key_ready = ok;
    return ok;
}

/**
 * @brief Print the boot banner for the LCD hello lesson.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_banner(void) {
    printf("=== PICOKIT-05 LCD LIVE // UPTIME + AUTHENTICATED HEARTBEAT ===\n");
}

/**
 * @brief Derive the field key and announce a ready monitor.
 *
 * @param void No parameters.
 * @return bool true when the field key was derived and installed.
 */
static bool monitor_finish(void) {
    bool ok = monitor_derive_key();
    if (ok) {
        monitor_banner();
    }
    return ok;
}

/**
 * @brief Blink the onboard heartbeat LED exactly once.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_heartbeat(void) {
    gpio_put(PICOKIT_05_LCD_LIVE_LED_PIN, 1);
    sleep_us(MONITOR_HEARTBEAT_BLINK_US);
    gpio_put(PICOKIT_05_LCD_LIVE_LED_PIN, 0);
    sleep_us(MONITOR_HEARTBEAT_BLINK_US);
}

/**
 * @brief Print one console line for the current LCD refresh step.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_log_step(void) {
    printf("LCD step=%u seq=%u\n", (unsigned)g_count, (unsigned)g_seq);
}

/**
 * @brief Render the live uptime and step counter display lines.
 *
 * @param now_us Current monotonic time in microseconds.
 * @return void
 */
static void monitor_render(uint64_t now_us) {
    snprintf(g_line1, DISPLAY_LINE_LEN, "PICOKIT-05 LCD");
    snprintf(g_line2, DISPLAY_LINE_LEN, "UP %us C %u", (unsigned)(now_us / 1000000u), (unsigned)g_count);
    display_show_lines(PICOKIT_05_LCD_LIVE_I2C, PICOKIT_05_LCD_LIVE_LCD_ADDR, g_line1, g_line2);
}

/**
 * @brief Refresh the live LCD and schedule the next refresh.
 *
 * @param now_us Current monotonic time in microseconds.
 * @return void
 */
static void monitor_step_tick(uint64_t now_us) {
    monitor_render(now_us);
    monitor_heartbeat();
    monitor_log_step();
    g_count += 1u;
    g_next_step_us = now_us + (uint64_t)MONITOR_STEP_INTERVAL_MS * 1000u;
}

/**
 * @brief Format the heartbeat JSON body for the current counter.
 *
 * @param frame Pointer to the mutable frame output buffer.
 * @param frame_len Capacity of the frame output buffer in bytes.
 * @return size_t Number of JSON bytes written, or zero on overflow.
 */
static size_t monitor_build_frame(char *frame, size_t frame_len) {
    int written = snprintf(frame, frame_len, "{\"n\":%u,\"s\":%u,\"c\":%u}", (unsigned)PACKET_NODE_ID, (unsigned)g_seq, (unsigned)g_count);
    return (written > 0 && (size_t)written < frame_len) ? (size_t)written : 0u;
}

/**
 * @brief Seal the current heartbeat body into a hex envelope.
 *
 * @param hex Pointer to the NUL-terminated hex output buffer.
 * @param hex_len Capacity of the hex output buffer in bytes.
 * @return bool true when the heartbeat was sealed and encoded.
 */
static bool monitor_seal_frame(char *hex, size_t hex_len) {
    char frame[PICOKIT_05_LCD_LIVE_FRAME_SIZE];
    uint8_t nonce[ENVELOPE_NONCE_LEN];
    uint8_t ad = (uint8_t)PACKET_NODE_ID;
    size_t frame_len = monitor_build_frame(frame, sizeof(frame));
    envelope_fill_nonce(nonce);
    return envelope_seal_hex(g_key, nonce, &ad, 1u, (const uint8_t *)frame, frame_len, hex, hex_len);
}

/**
 * @brief Build and transmit the authenticated heartbeat frame.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_transmit(void) {
    char hex[ENVELOPE_MAX_HEX_LEN];
    if (!g_key_ready) {
        return;
    }
    if (monitor_seal_frame(hex, sizeof(hex))) {
        radio_send_frame(PICOKIT_05_LCD_LIVE_UART, (const uint8_t *)hex, strlen(hex));
        g_seq += 1u;
    }
}

/**
 * @brief Transmit one heartbeat and schedule the next transmit.
 *
 * @param now_us Current monotonic time in microseconds.
 * @return void
 */
static void monitor_tx_tick(uint64_t now_us) {
    monitor_transmit();
    g_next_tx_us = now_us + (uint64_t)PICOKIT_05_LCD_LIVE_TX_INTERVAL_MS * 1000u;
}

/**
 * @brief Drain inbound radio lines and log every valid +RCV report.
 *
 * @param void No parameters.
 * @return void
 */
static void monitor_rx_tick(void) {
    radio_rcv_t rcv;
    while (radio_line_pump(PICOKIT_05_LCD_LIVE_UART, g_rx_line, &g_rx_len)) {
        if (radio_parse_rcv(g_rx_line, &rcv) == RADIO_RESULT_OK) {
            printf("RX from 0x%04X, %u bytes\n", (unsigned)rcv.sender, (unsigned)rcv.len);
        }
    }
}

/**
 * @brief Service the LCD refresh and heartbeat transmit timers.
 *
 * @param now_us Current monotonic time in microseconds.
 * @return void
 */
static void monitor_service_timers(uint64_t now_us) {
    if (now_us >= g_next_step_us) {
        monitor_step_tick(now_us);
    }
    if (now_us >= g_next_tx_us) {
        monitor_tx_tick(now_us);
    }
}

bool monitor_init(void) {
    bool ok;
    monitor_bus_init();
    ok = status_led_init() && radio_init(PICOKIT_05_LCD_LIVE_UART);
    monitor_state_init_io();
    monitor_state_init();
    return ok && display_init(PICOKIT_05_LCD_LIVE_I2C, PICOKIT_05_LCD_LIVE_LCD_ADDR) && monitor_finish();
}

void monitor_deinit(void) {
    g_ready = false;
}

bool monitor_step(void) {
    uint64_t now_us;
    if (!g_ready) {
        return false;
    }
    now_us = time_us_64();
    monitor_service_timers(now_us);
    monitor_rx_tick();
    return true;
}
