/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <termios.h>

#define TO_PERCENTAGE(val, total) (100 - (((total - val) * 100) / total))

int serial_open(char *dev, speed_t baud);
void serial_fifo_write(void *data, size_t number_of_bytes);
void serial_fifo_read(void *data, size_t number_of_bytes);
void bp_switch_baudrate_generator(bool to_high_speed);
void bp_exit(void);
void sig_handler(int sig_num);
void wait_for_ack_on(char *progress_string, uint32_t address);
void draw_progress_bar(uint8_t percent);

#endif
