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
// File:    monitor.h
// Desc:    Declares the 1602 live LCD state machine that pairs the display
//          refresh with an authenticated LoRa heartbeat.
// Created: 2026

#ifndef MONITOR_H
#define MONITOR_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Spacing between two LCD refresh steps in milliseconds.
 */
#define MONITOR_STEP_INTERVAL_MS 1000u

/**
 * @brief Onboard heartbeat LED on and off time in microseconds.
 */
#define MONITOR_HEARTBEAT_BLINK_US 50000u

/**
 * @brief Initialize the 1602 live LCD monitor state machine.
 *
 * Configures the I2C bus, the onboard heartbeat LED, the 1602 LCD, and the
 * RYLR998 UART, derives the field key, and resets the sequence counter.
 *
 * @param void No parameters.
 * @return bool true when all submodules initialized.
 */
bool monitor_init(void);

/**
 * @brief Clear the monitor-ready flag.
 *
 * Test and recovery hook that returns the state machine to the
 * uninitialized policy state.
 *
 * @param void No parameters.
 * @return void
 */
void monitor_deinit(void);

/**
 * @brief Execute one monitor state-machine tick.
 *
 * Refreshes the 1602 LCD on the step interval, blinks the onboard LED as a
 * heartbeat, transmits the authenticated heartbeat frame on the telemetry
 * interval, and pumps inbound +RCV lines.
 *
 * @param void No parameters.
 * @return bool true when the tick completed without a policy error.
 */
bool monitor_step(void);

#endif // MONITOR_H
