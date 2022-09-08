/** @file
  Early SPI flash rescue protocol - board implementation.

  Copyright (c) 2022, Baruch Binyamin Doron.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/SerialPortLib.h>
#include <Library/SpiLib.h>
#include <Library/TimerLib.h>
#include <Protocol/Spi2.h>
#include "FlashRescueBoard.h"

// TODO: Appropriate size
static UINT16 XferBlockSize = FixedPcdGet16 (PcdDataXferPacketSize);

/**
 * Send HELLO command to an awaiting userspace.
 *
 * @return EFI_SUCCESS  Command acknowledged.
 * @return EFI_TIMEOUT  Command timed-out.
**/
EFI_STATUS
EFIAPI
SendHelloPacket (
  VOID
  )
{
  UINT32                       WaitTimeout;
  EARLY_FLASH_RESCUE_COMMAND   CommandPacket;
  UINTN                        TimeCounter;
  EARLY_FLASH_RESCUE_RESPONSE  ResponsePacket;

  WaitTimeout = FixedPcdGet32 (PcdUserspaceHostWaitTimeout);

  // TODO: Consider sending a total `BlockNumber`?
  CommandPacket.Command = EARLY_FLASH_RESCUE_COMMAND_HELLO;

  for (TimeCounter = 0; TimeCounter < WaitTimeout; TimeCounter += 250) {
    // Maybe packet was not in FIFO
    SerialPortWrite ((UINT8 *)&CommandPacket, sizeof (CommandPacket));

    SerialPortRead ((UINT8 *)&ResponsePacket, sizeof (ResponsePacket));
    if (ResponsePacket.Acknowledge == 1) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (250 * MS_IN_SECOND);
  }

  return EFI_TIMEOUT;
}

/**
 * Send the requested block CRC to an awaiting userspace.
 * - TODO: NACK blocks as necessary
**/
VOID
EFIAPI
SendBlockChecksum (
  UINTN  BlockNumber
  )
{
  PCH_SPI2_PROTOCOL            *Spi2Ppi;
  UINTN                        Address;
  UINT8                        BlockData[SIZE_BLOCK];
  EFI_STATUS                   Status;
  UINT32                       Crc;
  EARLY_FLASH_RESCUE_RESPONSE  ResponsePacket;

  Spi2Ppi = GetSpiPpi ();
  if (Spi2Ppi == NULL) {
    return;
  }

  // `BlockNumber` starting in BIOS region
  Address = BlockNumber * SIZE_BLOCK;

  Status = Spi2Ppi->FlashRead (
             Spi2Ppi,
             &gFlashRegionBiosGuid,
             Address,
             SIZE_BLOCK,
             BlockData
             );
  if (EFI_ERROR (Status)) {
    return;
  }

  Crc = CalculateCrc32 (BlockData, SIZE_BLOCK);

  // Now, acknowledge userspace request and send block CRC
  ResponsePacket.Acknowledge = 1;
  SerialPortWrite ((UINT8 *)&ResponsePacket, sizeof (ResponsePacket));
  SerialPortWrite ((UINT8 *)&Crc, sizeof (Crc));
}

/**
 * Write the requested SPI flash block.
 * - TODO: NACK blocks as necessary
**/
VOID
EFIAPI
WriteBlock (
  UINTN  BlockNumber
  )
{
  PCH_SPI2_PROTOCOL            *Spi2Ppi;
  UINTN                        Address;
  UINT8                        BlockData[SIZE_BLOCK];
  EARLY_FLASH_RESCUE_RESPONSE  ResponsePacket;
  VOID                         *XferBlock;
  UINTN                        Index;
  EFI_STATUS                   Status;

  Spi2Ppi = GetSpiPpi ();
  if (Spi2Ppi == NULL) {
    return;
  }

  // `BlockNumber` starting in BIOS region
  Address = BlockNumber * SIZE_BLOCK;

  // Acknowledge userspace command and retrieve block
  ResponsePacket.Acknowledge = 1;
  SerialPortWrite ((UINT8 *)&ResponsePacket, sizeof (ResponsePacket));

  // Start streaming block
  XferBlock = BlockData;
  for (Index = 0; Index < SIZE_BLOCK; Index += XferBlockSize) {
    // FIXME: This will incur some penalty, but we must wait
    // - Still debugging timing parameters, especially at higher baudrate
    MicroSecondDelay (33 * MS_IN_SECOND);

    SerialPortRead (XferBlock, XferBlockSize);
    XferBlock += XferBlockSize;

    // FIXME: This will incur some penalty, but userspace must wait
    ResponsePacket.Acknowledge = 1;
    SerialPortWrite ((UINT8 *)&ResponsePacket, sizeof (ResponsePacket));
  }

  // SPI flash is is fairly durable, but determine when erase is necessary.
  Status = Spi2Ppi->FlashErase (
             Spi2Ppi,
             &gFlashRegionBiosGuid,
             Address,
             SIZE_BLOCK
             );
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = Spi2Ppi->FlashWrite (
             Spi2Ppi,
             &gFlashRegionBiosGuid,
             Address,
             SIZE_BLOCK,
             BlockData
             );
}

/**
 * Perform flash.
 *
 * @return EFI_SUCCESS       Successful flash.
 * @return EFI_DEVICE_ERROR  Initialise SPI service failed.
 * @return EFI_TIMEOUT       Await command timed-out.
**/
EFI_STATUS
EFIAPI
PerformFlash (
  VOID
  )
{
  EFI_STATUS                  Status;
  UINT8                       NoUserspaceExit;
  UINT64                      LastServicedTimeNs;
  EARLY_FLASH_RESCUE_COMMAND  CommandPacket;

  //
  // TODO: Library must reinstall its PPI, backed by NEM/DRAM
  //
  Status = SpiServiceInit ();
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  // Userspace-side orchestrates procedure, so no looping over blocks
  NoUserspaceExit = 1;

  LastServicedTimeNs = GetTimeInNanoSecond (GetPerformanceCounter ());
  while (NoUserspaceExit) {
    // Check if there is command waiting for us
    if (SerialPortPoll ()) {
      // Stall a tiny bit, in-case the remainder of the packet is flushing
      MicroSecondDelay (10 * MS_IN_SECOND);

      SerialPortRead ((UINT8 *)&CommandPacket, sizeof (CommandPacket));
      switch (CommandPacket.Command) {
        case EARLY_FLASH_RESCUE_COMMAND_CHECKSUM:
          SendBlockChecksum (CommandPacket.BlockNumber);
          break;
        case EARLY_FLASH_RESCUE_COMMAND_WRITE:
          WriteBlock (CommandPacket.BlockNumber);
          break;
        case EARLY_FLASH_RESCUE_COMMAND_RESET:
          PerformSystemReset ();
          // Permit fallthrough
          NoUserspaceExit = 0;
          break;
        case EARLY_FLASH_RESCUE_COMMAND_EXIT:
          NoUserspaceExit = 0;
          break;
        default:
          DEBUG ((DEBUG_ERROR, "Cannot understand command 0x%x!\n", CommandPacket.Command));
          break;
      }

      LastServicedTimeNs = GetTimeInNanoSecond (GetPerformanceCounter ());
    }

    if ((GetTimeInNanoSecond (GetPerformanceCounter ()) - LastServicedTimeNs) >=
        (10ULL * NS_IN_SECOND)) {
      // This is very bad. SPI flash could be inconsistent
      // - In CAR there's likely too little memory to stash a backup
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}
