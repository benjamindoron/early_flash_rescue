/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include "flash_rescue_userspace.h"
#include "util.h"

/* Written with help from
   https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/
 */
int serial_open(char *dev, speed_t baud)
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

	if (cfsetspeed(&tty, baud) != 0)
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
	if (implementation == 1)
		bp_exit();
	if (serial_dev)
		close(serial_dev);
	_exit(sig_num);
}

// Can push into buffer while board handles its pulled data
void serial_fifo_write(void *data, size_t number_of_bytes)
{
	size_t status = write(serial_dev, data, number_of_bytes);
	assert(status > 0);
	tcdrain(serial_dev);
}
// Can block while awaiting a busy board
void serial_fifo_read(void *data, size_t number_of_bytes)
{
	// Do not flush, maintain following FIFO bytes
	size_t status = read(serial_dev, data, number_of_bytes);
	assert(status > 0);
}
