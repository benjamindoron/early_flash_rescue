/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

#define SIZE_BLOCK    4096
#define SIZE_MB       (1024 * 1024)
#define MS_IN_SECOND  1000

#define EARLY_FLASH_RESCUE_PROTOCOL_VERSION 0.25
#define EARLY_FLASH_RESCUE_COMMAND_HELLO    0x10
#define EARLY_FLASH_RESCUE_COMMAND_CHECKSUM 0x11
#define EARLY_FLASH_RESCUE_COMMAND_READ     0x12
#define EARLY_FLASH_RESCUE_COMMAND_WRITE    0x13
#define EARLY_FLASH_RESCUE_COMMAND_RESET    0x14
#define EARLY_FLASH_RESCUE_COMMAND_EXIT     0x15

#pragma pack(push, 1)
typedef struct {
	uint8_t  Command;
	uint16_t BlockNumber; // This 4K block in BIOS region
} EARLY_FLASH_RESCUE_COMMAND;

typedef struct {
	uint8_t  Acknowledge; // Usually, ACK == 0x01
	uint16_t Size;        // OPTIONAL?
} EARLY_FLASH_RESCUE_RESPONSE;
#pragma pack(pop)

static FILE *bios_fp;
static int serial_dev;
static uint8_t implementation = 0xFF;
static uint16_t xfer_block_size = SIZE_BLOCK;


/* Written with help from
   https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/ */
int serial_open(char *dev)
{
	int serial_port;
	struct termios tty;

	if ((serial_port = open(dev, O_RDWR | O_NOCTTY)) == -1)
		goto fail;
	if (tcgetattr(serial_port, &tty) != 0)
		goto fail;

	// 8N1: START, 8*DATA, STOP (no parity)
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;

#if 1
	// I think so?
	tty.c_cflag &= ~CRTSCTS;
	tty.c_cflag |= (CREAD | CLOCAL);
	tty.c_iflag = (IGNPAR | BRKINT);
	tty.c_oflag = 0;
	tty.c_lflag = 0;
#endif

	if (cfsetspeed(&tty, B115200) != 0)
		goto fail;
	if (tcsetattr(serial_port, TCSANOW, &tty) != 0)
		goto fail;

	return serial_port;
fail:
	if (serial_port)
		close(serial_port);
	return -1;
}

// Cleanup open handles
void sig_handler(int sig_num)
{
	if (bios_fp)
		fclose(bios_fp);
	if (serial_dev)
		close(serial_dev);
	_exit(sig_num);
}

// Initialise userspace
int initialise_userspace(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "f:d:m:")) != -1) {
		// Required parameter is in global "optarg"
		switch (opt) {
		case 'f':
			bios_fp = fopen(optarg, "r");
			break;
		case 'd':
			serial_dev = serial_open(optarg);
			break;
		case 'm':
			implementation = atoi(optarg);
			break;
		}
	}

	if (bios_fp == NULL || serial_dev < 0 || implementation == 0xFF) {
		printf("Usage: %s -f <BIOS image> -d <serial port> -m [init]\n",
			argv[0]);
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

// Can push into buffer while board handles its pulled data
void serial_fifo_write(void *data, int number_of_bytes)
{
	write(serial_dev, data, number_of_bytes);
	tcdrain(serial_dev);
}
// Can block while awaiting a busy board
void serial_fifo_read(void *data, int number_of_bytes)
{
	read(serial_dev, data, number_of_bytes);
}

// Implementation-specific methods to bring-up underlying layer
void initialise_debug_port(void)
{
	char bp_rst_sequence[] = { '\n', '#', '\n' };
	char bp_i2c_sequence[] = { 'm', '\n', '4', '\n', '2', '\n' };
	char bp_debug_port[] = { '(', '5', ')', '\n' };

	if (implementation == 1) {
		// TODO: Appropriate size
		xfer_block_size = 64;

		// usleep() to allow interactive console to keep up
		serial_fifo_write(bp_rst_sequence, sizeof(bp_rst_sequence));
		usleep(100 * MS_IN_SECOND);
		serial_fifo_write(bp_i2c_sequence, sizeof(bp_i2c_sequence));
		usleep(100 * MS_IN_SECOND);
		serial_fifo_write(bp_debug_port, sizeof(bp_debug_port));
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
		printf("\b\r%c[2K\rStill awaiting a COMMAND_HELLO. Serial port busy...\n", 0x1B);
		serial_fifo_read(&hello_packet, sizeof(hello_packet));
	}

	printf("Board is present! Acknowledging its COMMAND_HELLO...\n");
	response_packet.Acknowledge = 1;
	serial_fifo_write(&response_packet, sizeof(response_packet));

	// Flush spurious `HELLO`s
	tcflush(serial_dev, TCIOFLUSH);
}

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

// By requesting checksums, we attempt optimising the flash procedure
uint32_t request_block_checksum(uint32_t address)
{
	EARLY_FLASH_RESCUE_COMMAND command_packet;
	uint32_t response_crc = 0;

	command_packet.Command = EARLY_FLASH_RESCUE_COMMAND_CHECKSUM;
	command_packet.BlockNumber = (address / SIZE_BLOCK);
	serial_fifo_write(&command_packet, sizeof(command_packet));

	// Board acknowledges when it's ready
	wait_for_ack_on("COMMAND_CHECKSUM");

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
	wait_for_ack_on("COMMAND_WRITE");

	// Start streaming block
	void *xfer_block = block;
	for (int i = 0; i < SIZE_BLOCK; i += xfer_block_size) {
		serial_fifo_write(xfer_block, xfer_block_size);
		xfer_block += xfer_block_size;
		// FIXME: This will incur significant penalty
		// - However, low baud rate here means that CRC write does not hold
		// - Alternatively, raise FTDI baud rate?
		wait_for_ack_on("WRITE_DATA");
	}
}

/* Written with help from
   https://gist.github.com/amullins83/24b5ef48657c08c4005a8fab837b7499/ */
#define TO_PERCENTAGE(val, total) (100 - (((total - val) * 100) / total))
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

// Orchestrate flash operations
void perform_flash(void)
{
	struct stat bios_fp_stats;
	uint8_t region_modified;
	void *bios_block;
	time_t start_time, stop_time, diff_time;
	uint32_t crc;
	EARLY_FLASH_RESCUE_COMMAND command_packet;

	// Determine size
	// - TODO: Check that region matches image by stashing total
	fstat(fileno(bios_fp), &bios_fp_stats);
	printf("BIOS image is %.2f MiB (%d blocks)\n",
		(float)bios_fp_stats.st_size / SIZE_MB,
		(int)bios_fp_stats.st_size / SIZE_BLOCK);
	if (bios_fp_stats.st_size % SIZE_BLOCK != 0) {
		printf("BIOS image is not a multiple of %d!", SIZE_BLOCK);
		return;
	}

	region_modified = 0;

	// Write modified blocks
	printf("Writing...\n");
	bios_block = malloc(SIZE_BLOCK);
	time(&start_time);
	for (int i = 0; i < bios_fp_stats.st_size; i += SIZE_BLOCK) {
		draw_progress_bar(TO_PERCENTAGE(i, bios_fp_stats.st_size));

		// Read this block
		fseek(bios_fp, i, SEEK_SET);
		fread(bios_block, SIZE_BLOCK, 1, bios_fp);

		// Independent checksums
		crc = crc32(0, bios_block, SIZE_BLOCK);
		// TODO: Handle NACKs
		if (request_block_checksum(i) != crc) {
			write_block(i, bios_block);
			region_modified = 1;
		}
	}
	printf("\n");

	if (region_modified == 0)
		goto end;

	// Perform verification
	printf("Verifying...\n");
	for (int i = 0; i < bios_fp_stats.st_size; i += SIZE_BLOCK) {
		draw_progress_bar(TO_PERCENTAGE(i, bios_fp_stats.st_size));

		// Read this block
		fseek(bios_fp, i, SEEK_SET);
		fread(bios_block, SIZE_BLOCK, 1, bios_fp);

		// Independent checksums
		crc = crc32(0, bios_block, SIZE_BLOCK);
		// TODO: Handle NACKs
		if (request_block_checksum(i) != crc) {
			printf("Verification FAILURE at 0x%x!\n", i);
		}
	}
	time(&stop_time);
	diff_time = stop_time - start_time;
	printf("\nWrite operation took %ldm%lds\n",
		diff_time / 60, diff_time % 60);

	// Finalise
	if (region_modified == 1)
		command_packet.Command = EARLY_FLASH_RESCUE_COMMAND_RESET;
	else
end:
		command_packet.Command = EARLY_FLASH_RESCUE_COMMAND_EXIT;

	serial_fifo_write(&command_packet, sizeof(command_packet));

	printf("Flash operations completed successfully.\n");
	free(bios_block);
}

// TODO: Win32 support; implement read to complete interface
int main(int argc, char *argv[])
{
	int return_value;

	// Print hello text
	printf("Early BIOS flash rescue v%.2f (Userspace side)\n",
		EARLY_FLASH_RESCUE_PROTOCOL_VERSION);
	printf("NB: At this time, don't use with console - risks read() races\n\n");

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
	if (serial_dev)
		close(serial_dev);
	return return_value;
}
