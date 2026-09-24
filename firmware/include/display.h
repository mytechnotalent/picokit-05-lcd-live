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
// File:    display.h
// Desc:    Declares the 1602 I2C LCD two line text driver.
// Created: 2026

#ifndef DISPLAY_H
#define DISPLAY_H

#include "hardware/i2c.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Number of character columns on the 1602 display.
 */
#define DISPLAY_COLS 16u

/**
 * @brief Number of character rows on the 1602 display.
 */
#define DISPLAY_ROWS 2u

/**
 * @brief Capacity of one rendered display line including the terminator.
 */
#define DISPLAY_LINE_LEN (DISPLAY_COLS + 1u)

/**
 * @brief DRAM row offset of the second 1602 line.
 */
#define DISPLAY_LINE2_ADDR 0x40u

/**
 * @brief Initialize the 1602 LCD over the I2C backpack.
 *
 * Runs the HD44780 4-bit initialization sequence, turns the display on
 * with the cursor hidden, and clears both lines.
 *
 * @param i2c Pointer to the I2C peripheral the backpack is wired to.
 * @param addr The 7-bit I2C address of the backpack.
 * @return bool true when initialization completed.
 */
bool display_init(i2c_inst_t *i2c, uint8_t addr);

/**
 * @brief Push two rendered lines to the LCD frame buffer.
 *
 * Writes line one and line two to their DRAM row addresses over I2C,
 * padding each row to the full sixteen columns.
 *
 * @param i2c Pointer to the I2C peripheral the backpack is wired to.
 * @param addr The 7-bit I2C address of the backpack.
 * @param line1 Pointer to the first-line text.
 * @param line2 Pointer to the second-line text.
 * @return void
 */
void display_show_lines(i2c_inst_t *i2c, uint8_t addr, const char *line1,
                        const char *line2);

#endif // DISPLAY_H
