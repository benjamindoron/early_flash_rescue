/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "flash_rescue_userspace.h"
#include "util.h"

// Bus Pirate toggle baudrate generator
void bp_switch_baudrate_generator(bool to_high_speed)
{
	char *bp_normal_speed = "b\n9\n";
	char *bp_high_speed = "b\n10\n3\n";
	char *bp_speed_ack = " \n";

	char *bp_this_speed = (to_high_speed == 1) ?
				bp_high_speed : bp_normal_speed;
	speed_t sys_this_speed = (to_high_speed == 1) ? B1000000 : B115200;

	serial_fifo_write(bp_this_speed, strlen(bp_this_speed));
	usleep(100 * MS_IN_SECOND);

	close(serial_dev);
	serial_open(p_dev, sys_this_speed);

	serial_fifo_write(bp_speed_ack, strlen(bp_speed_ack));
	usleep(100 * MS_IN_SECOND);
}

// Bus Pirate exit helper
void bp_exit(void)
{
	uint8_t bp_debug_port_exit[] = { 0x1B, 0x5B, 0x32, 0x34, 0x7E };
	char *bp_not_exits = "\n";

	serial_fifo_write(bp_debug_port_exit, sizeof(bp_debug_port_exit));
	usleep(100 * MS_IN_SECOND);
	serial_fifo_write(bp_not_exits, strlen(bp_not_exits));
	usleep(100 * MS_IN_SECOND);

	if (implementation_high_speed)
		bp_switch_baudrate_generator(false);
	tcflush(serial_dev, TCIOFLUSH);
}

// Wait for `ACK` response helper
void wait_for_ack_on(char *progress_string, uint32_t address)
{
	EARLY_FLASH_RESCUE_RESPONSE response_packet;

	serial_fifo_read(&response_packet, sizeof(response_packet));
	while (response_packet.Acknowledge != 1) {
		printf("\b\r%c[2K\r%s (0x%x) NACK'd. Serial port busy...\n", 0x1B, progress_string, address);
		serial_fifo_read(&response_packet, sizeof(response_packet));
	}
}

/* Written with help from
   https://gist.github.com/amullins83/24b5ef48657c08c4005a8fab837b7499/ */
void draw_progress_bar(uint8_t percent)
{
	#define BAR_LENGTH 25
	#define PERCENT_TO_CHAR (100 / BAR_LENGTH)
	// Allocate space for bar, as well as brackets and NULL terminator
	char progress_string[(BAR_LENGTH + 3)];

	// Build a progress bar
	memset(progress_string, ' ', (BAR_LENGTH + 2));
	progress_string[0] = '[';
	progress_string[(BAR_LENGTH + 1)] = ']';
	progress_string[(BAR_LENGTH + 2)] = 0;

	// Copy in this percentage as chars
	if (percent > 100) percent = 100;
	memset(progress_string + 1, '#', percent / PERCENT_TO_CHAR);

	printf("\b\r%c[2K\r%s", 0x1B, progress_string);
	fflush(stdout);
}
