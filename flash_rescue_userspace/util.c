/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "flash_rescue_userspace.h"
#include "util.h"

// Wait for `ACK` response helper
void wait_for_ack_on(char *progress_string)
{
	EARLY_FLASH_RESCUE_RESPONSE response_packet;

	serial_fifo_read(&response_packet, sizeof(response_packet));
	while (response_packet.Acknowledge != 1) {
		printf("\b\r%c[2K\r%s NACK'd. Serial port busy...\n", 0x1B, progress_string);
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
