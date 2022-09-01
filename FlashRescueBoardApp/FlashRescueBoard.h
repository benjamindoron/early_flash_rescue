/** @file
  Early SPI flash rescue protocol - board implementation

  Copyright (c) 2022, Baruch Binyamin Doron<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef FLASH_RESCUE_BOARD_H
#define FLASH_RESCUE_BOARD_H

#include <Base.h>

#define SIZE_BLOCK    4096
#define MS_IN_SECOND  1000
#define NS_IN_SECOND  (1000 * 1000 * 1000)

#define EARLY_FLASH_RESCUE_PROTOCOL_VERSION 0.25
#define EARLY_FLASH_RESCUE_COMMAND_HELLO    0x10
#define EARLY_FLASH_RESCUE_COMMAND_CHECKSUM 0x11
#define EARLY_FLASH_RESCUE_COMMAND_READ     0x12
#define EARLY_FLASH_RESCUE_COMMAND_WRITE    0x13
#define EARLY_FLASH_RESCUE_COMMAND_RESET    0x14
#define EARLY_FLASH_RESCUE_COMMAND_EXIT     0x15

#pragma pack(push, 1)
typedef struct {
	UINT8   Command;
	UINT16  BlockNumber;  // This 4K block in BIOS region
} EARLY_FLASH_RESCUE_COMMAND;

typedef struct {
	UINT8   Acknowledge;  // Usually, ACK == 0x01
	UINT16  Size;         // OPTIONAL?
} EARLY_FLASH_RESCUE_RESPONSE;
#pragma pack(pop)

#endif
