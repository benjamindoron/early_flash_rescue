/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef UTIL_H
#define UTIL_H

#define TO_PERCENTAGE(val, total) (100 - (((total - val) * 100) / total))

int serial_open(char *dev);
void sig_handler(int sig_num);
void serial_fifo_write(void *data, int number_of_bytes);
void serial_fifo_read(void *data, int number_of_bytes);
void wait_for_ack_on(char *progress_string);
void draw_progress_bar(uint8_t percent);

#endif
