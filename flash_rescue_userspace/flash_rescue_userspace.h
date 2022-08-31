/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FLASH_RESCUE_USERSPACE_H
#define FLASH_RESCUE_USERSPACE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>

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

extern FILE *bios_fp;
extern int serial_dev;
extern char *p_dev;
extern uint8_t implementation;
extern bool implementation_high_speed;

#endif
