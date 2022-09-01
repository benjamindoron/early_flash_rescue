/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <zlib.h>
#include "flash_rescue_userspace.h"
#include "util.h"

FILE *bios_fp;
int serial_dev;
char *p_dev;
uint8_t implementation = 0xFF;
bool implementation_high_speed = false;
static uint16_t xfer_block_size = SIZE_BLOCK;


// Initialise userspace
int initialise_userspace(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "f:d:m:s")) != -1) {
		// Required parameter is in global "optarg"
		switch (opt) {
		case 'f':
			bios_fp = fopen(optarg, "r");
			break;
		case 'd':
			serial_dev = serial_open(optarg, B115200);
			p_dev = optarg;
			break;
		case 'm':
			implementation = atoi(optarg);
			break;
		case 's':
			implementation_high_speed = true;
			break;
		}
	}

	if (bios_fp == NULL || serial_dev < 0 || implementation == 0xFF) {
		printf("Usage: %s [OPTIONS]", argv[0]);
		printf("\n");
		printf("  -f <BIOS image>\n");
		printf("  -d <serial port>\n");
		printf("  -m [mode]\n");
		printf("  -s [high speed; OPTIONAL]\n");
		printf("\n");
		printf("Implementation modes:\n");
		printf("  1: Bus Pirate\n");
		printf("  254: (No initialisation or quirks required)\n");
		printf("  255: (Reserved - MAX)\n");
		return 1;
	}

	// Might wait infinitely. Be polite and clean up if the user escapes
	signal(SIGINT, sig_handler);

	return 0;
}

// Implementation-specific methods to bring-up underlying layer
void initialise_debug_port(void)
{
	uint8_t bp_debug_port_exit[] = {0x1B, 0x5B, 0x32, 0x34, 0x7E};
	char *bp_not_exits = "\n";
	char *bp_rst_sequence = "\n#\n";
	char *bp_i2c_sequence = "m\n4\n2\n";
	char *bp_debug_port = "(5)\n";

	if (implementation == 1) {
		// TODO: Appropriate size
		xfer_block_size = 64;

		// usleep() to allow interactive console to keep up
		serial_fifo_write(bp_debug_port_exit, sizeof(bp_debug_port_exit));
		usleep(100 * MS_IN_SECOND);
		serial_fifo_write(bp_not_exits, strlen(bp_not_exits));
		usleep(100 * MS_IN_SECOND);
		serial_fifo_write(bp_rst_sequence, strlen(bp_rst_sequence));
		usleep(100 * MS_IN_SECOND);

		// TODO: Debugging
		if (implementation_high_speed)
			bp_switch_baudrate_generator(true);

		serial_fifo_write(bp_i2c_sequence, strlen(bp_i2c_sequence));
		usleep(100 * MS_IN_SECOND);
		serial_fifo_write(bp_debug_port, strlen(bp_debug_port));
		usleep(100 * MS_IN_SECOND);
	}

	// Don't care what debug port responded
	tcflush(serial_dev, TCIOFLUSH);
}

// Wait for `HELLO` command packet
void wait_for_hello(void)
{
	EARLY_FLASH_RESCUE_COMMAND hello_packet;
	EARLY_FLASH_RESCUE_RESPONSE response_packet;

	printf("Awaiting a COMMAND_HELLO...\n");
	serial_fifo_read(&hello_packet, sizeof(hello_packet));
	while (hello_packet.Command != EARLY_FLASH_RESCUE_COMMAND_HELLO) {
		fprintf(stderr, "Still awaiting a COMMAND_HELLO. Serial port busy...\n");
		serial_fifo_read(&hello_packet, sizeof(hello_packet));
	}

	printf("Board is present! Acknowledging its COMMAND_HELLO...\n");
	response_packet.Acknowledge = 1;
	serial_fifo_write(&response_packet, sizeof(response_packet));

	// Flush spurious `HELLO`s
	tcflush(serial_dev, TCIOFLUSH);
}

// By requesting checksums, we attempt optimising the flash procedure
uint32_t request_block_checksum(uint32_t address)
{
	EARLY_FLASH_RESCUE_COMMAND command_packet;
	uint32_t response_crc = 0;

	command_packet.Command = EARLY_FLASH_RESCUE_COMMAND_CHECKSUM;
	command_packet.BlockNumber = (address / SIZE_BLOCK);
	serial_fifo_write(&command_packet, sizeof(command_packet));

	// Board acknowledges when it's ready
	wait_for_ack_on("COMMAND_CHECKSUM", address);

	// Retrieve packet with requested data
	serial_fifo_read(&response_crc, sizeof(response_crc));
	return response_crc;
}

// Write one block
void write_block(uint32_t address, void *block)
{
	EARLY_FLASH_RESCUE_COMMAND command_packet;

	command_packet.Command = EARLY_FLASH_RESCUE_COMMAND_WRITE;
	command_packet.BlockNumber = (address / SIZE_BLOCK);
	serial_fifo_write(&command_packet, sizeof(command_packet));

	// Board acknowledges when it's ready
	wait_for_ack_on("COMMAND_WRITE", address);

	// Start streaming block
	void *xfer_block = block;
	for (int i = 0; i < SIZE_BLOCK; i += xfer_block_size) {
		serial_fifo_write(xfer_block, xfer_block_size);
		xfer_block += xfer_block_size;
		// FIXME: This will incur significant penalty
		// - However, low baud rate here means that CRC write does not hold
		// - Alternatively, raise FTDI baud rate?
		wait_for_ack_on("WRITE_DATA", address);
	}
}

// Orchestrate flash operations
void perform_flash(void)
{
	struct stat bios_fp_stats;
	bool region_modified;
	void *bios_block;
	time_t start_time, stop_time, diff_time;
	size_t status;
	uint32_t crc;
	EARLY_FLASH_RESCUE_COMMAND command_packet;

	// Determine size
	// - TODO: Check that region matches image by stashing total
	fstat(fileno(bios_fp), &bios_fp_stats);
	printf("BIOS image is %.2f MiB (%d blocks)\n", (float)bios_fp_stats.st_size / SIZE_MB,
	       (int)bios_fp_stats.st_size / SIZE_BLOCK);
	if (bios_fp_stats.st_size % SIZE_BLOCK != 0) {
		printf("BIOS image is not a multiple of %d!", SIZE_BLOCK);
		return;
	}

	// Write modified blocks
	printf("Writing...\n");
	region_modified = false;
	bios_block = malloc(SIZE_BLOCK);
	time(&start_time);
	for (int i = 0; i < bios_fp_stats.st_size; i += SIZE_BLOCK) {
		draw_progress_bar(TO_PERCENTAGE(i, bios_fp_stats.st_size));

		// Read this block
		fseek(bios_fp, i, SEEK_SET);
		status = fread(bios_block, SIZE_BLOCK, 1, bios_fp);
		assert(status > 0);

		// Independent checksums
		crc = crc32(0, bios_block, SIZE_BLOCK);
		// TODO: Handle NACKs
		if (request_block_checksum(i) != crc) {
			write_block(i, bios_block);
			region_modified = true;
		}
	}
	printf("\n");

	if (!region_modified) {
		command_packet.Command = EARLY_FLASH_RESCUE_COMMAND_EXIT;
		goto end;
	}

	// Perform verification
	printf("Verifying...\n");
	region_modified = false;
	for (int i = 0; i < bios_fp_stats.st_size; i += SIZE_BLOCK) {
		draw_progress_bar(TO_PERCENTAGE(i, bios_fp_stats.st_size));

		// Read this block
		fseek(bios_fp, i, SEEK_SET);
		status = fread(bios_block, SIZE_BLOCK, 1, bios_fp);
		assert(status > 0);

		// Independent checksums
		crc = crc32(0, bios_block, SIZE_BLOCK);
		// TODO: Handle NACKs
		if (request_block_checksum(i) != crc) {
			fprintf(stderr, "Verification FAILURE at 0x%x!\n", i);
			region_modified = true;
		}
	}
	time(&stop_time);
	diff_time = stop_time - start_time;
	printf("\nWrite operation took %ldm%lds\n", diff_time / 60, diff_time % 60);

	// Finalise
	command_packet.Command = EARLY_FLASH_RESCUE_COMMAND_RESET;

end:
	serial_fifo_write(&command_packet, sizeof(command_packet));
	free(bios_block);

	if (!region_modified)
		printf("Flash operations completed successfully.\n");
	else
		fprintf(stderr, "Flash operations failed!\n");
}

// TODO: Win32 support; implement read and complete interface
int main(int argc, char *argv[])
{
	int return_value;

	// Print hello text
	printf("Early BIOS flash rescue v%.2f (Userspace side)\n",
	       EARLY_FLASH_RESCUE_PROTOCOL_VERSION);
	printf("NB: Cannot open console - serial read() is racey\n\n");

	// Step 1
	return_value = initialise_userspace(argc, argv);
	if (return_value != 0)
		goto cleanup;

	// Step 2
	initialise_debug_port();

	// Step 3
	wait_for_hello();

	// Step 4
	perform_flash();

cleanup:
	// Step 5
	if (bios_fp)
		fclose(bios_fp);
	if (implementation == 1)
		bp_exit();
	if (serial_dev)
		close(serial_dev);
	return return_value;
}
