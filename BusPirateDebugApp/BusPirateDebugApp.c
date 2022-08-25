/** @file
  Early SPI flash rescue protocol - board implementation

  Copyright (c) 2022, Baruch Binyamin Doron<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/SerialPortLib.h>
#include <Library/SpiLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/Spi2.h>

#include <Library/PchSpiCommonLib.h>
#include "FlashRescueBoard.h"

extern SPI_INSTANCE  *mSpiInstance;
// TODO: Use PCD; appropriate size
static UINT16 XferBlockSize = 64;


/**
 * Send HELLO command to an awaiting userspace.
 * Permit 15s for response.
 *
 * @return EFI_SUCCESS  Command acknowledged.
 * @return EFI_TIMEOUT  Command timed-out.
 */
EFI_STATUS
EFIAPI
SendHelloPacket (
  VOID
  )
{
  EARLY_FLASH_RESCUE_COMMAND   CommandPacket;
  UINTN                        TimeCounter;
  EARLY_FLASH_RESCUE_RESPONSE  ResponsePacket;

  // TODO: Consider sending a total `BlockNumber`?
  CommandPacket.Command = EARLY_FLASH_RESCUE_COMMAND_HELLO;

  for (TimeCounter = 0; TimeCounter < 15 * MS_IN_SECOND; TimeCounter += 250) {
    // Maybe packet was not in FIFO
    SerialPortWrite ((UINT8 *)&CommandPacket, sizeof(CommandPacket));

    SerialPortRead ((UINT8 *)&ResponsePacket, sizeof(ResponsePacket));
    if (ResponsePacket.Acknowledge == 1) {
      return EFI_SUCCESS;
    }

    MicroSecondDelay (250 * MS_IN_SECOND);
  }

  return EFI_TIMEOUT;
}

/**
 * Send the requested block CRC to an awaiting userspace.
 */
VOID
EFIAPI
SendBlockChecksum (
  UINTN  BlockNumber
  )
{
  UINTN                        Address;
  VOID                         *BlockData;
  EFI_STATUS                   Status;
  UINT32                       Crc;
  EARLY_FLASH_RESCUE_RESPONSE  ResponsePacket;

  // `BlockNumber` starting in BIOS region
  Address = BlockNumber * SIZE_BLOCK;
  BlockData = AllocatePool (SIZE_BLOCK);

  Print (L"Checksumming address 0x%x\n", Address);

  Status = SpiProtocolFlashRead (
             &(mSpiInstance->SpiProtocol),
             &gFlashRegionBiosGuid,
             Address,
             SIZE_BLOCK,
             BlockData
             );
  if (EFI_ERROR (Status)) {
    // TODO: NACK the block
    Print (L"Failed to read block 0x%x!\n", BlockNumber);
    goto End;
  }

  Crc = CalculateCrc32 (BlockData, SIZE_BLOCK);
  Print (L"The CRC32 for block 0x%x is 0x%x\n", BlockNumber, Crc);

  // Now, acknowledge userspace request and send block CRC
  ResponsePacket.Acknowledge = 1;
  SerialPortWrite ((UINT8 *)&ResponsePacket, sizeof(ResponsePacket));
  SerialPortWrite ((UINT8 *)&Crc, sizeof(Crc));

End:
  FreePool (BlockData);
}

/**
 * Write the requested SPI flash block.
 */
VOID
EFIAPI
WriteBlock (
  UINTN  BlockNumber
  )
{
  UINTN                        Address;
  VOID                         *BlockData;
  EARLY_FLASH_RESCUE_RESPONSE  ResponsePacket;
  VOID                         *XferBlock;
  UINTN                        Index;
  EFI_STATUS                   Status;

  // `BlockNumber` starting in BIOS region
  Address = BlockNumber * SIZE_BLOCK;
  BlockData = AllocatePool (SIZE_BLOCK);

  Print (L"Writing address 0x%x\n", Address);

  // Acknowledge userspace command and retrieve block
  ResponsePacket.Acknowledge = 1;
  SerialPortWrite ((UINT8 *)&ResponsePacket, sizeof(ResponsePacket));

  // Start streaming block
  XferBlock = BlockData;
  for (Index = 0; Index < SIZE_BLOCK; Index += XferBlockSize) {
    // FIXME: This will incur some penalty, but we must wait
    // - Microchip PIC <-> FTDI at baud rate limit?
    MicroSecondDelay (25 * MS_IN_SECOND);
    SerialPortRead (XferBlock, XferBlockSize);
    XferBlock += XferBlockSize;
    // FIXME: This will incur some penalty, but userspace must wait
    ResponsePacket.Acknowledge = 1;
    SerialPortWrite ((UINT8 *)&ResponsePacket, sizeof(ResponsePacket));
  }

  //InternalPrintData (BlockData, SIZE_BLOCK);
#if 1
  // TODO: SPI flash is is fairly durable, but determine when erase is necessary.
  Status = SpiProtocolFlashErase (
             &(mSpiInstance->SpiProtocol),
             &gFlashRegionBiosGuid,
             Address,
             SIZE_BLOCK
             );
  if (EFI_ERROR (Status)) {
    // TODO: NACK the block
    Print (L"Failed to erase block 0x%x!\n", BlockNumber);
    goto End;
  }

  Status = SpiProtocolFlashWrite (
             &(mSpiInstance->SpiProtocol),
             &gFlashRegionBiosGuid,
             Address,
             SIZE_BLOCK,
             BlockData
             );
  if (EFI_ERROR (Status)) {
    // TODO: NACK the block
    Print (L"Failed to write block 0x%x!\n", BlockNumber);
  }
#endif

End:
  FreePool (BlockData);
}

/**
 * Perform system reset to start this firmware.
 */
VOID
EFIAPI
PerformSystemReset (
  VOID
  )
{
  Print (L"FIXME: Refusing to restart!\n");
  Print (L"Optionally verify the region with FPT\n");
}

/**
 * Perform flash.
 *
 * @return EFI_SUCCESS  Successful flash.
 * @return EFI_TIMEOUT  Await command timed-out.
 */
EFI_STATUS
EFIAPI
PerformFlash (
  VOID
  )
{
  UINT8                       NoUserspaceExit;
  UINT64                      LastServicedTimeNs;
  EARLY_FLASH_RESCUE_COMMAND  CommandPacket;

  // Userspace-side orchestrates procedure, so no looping over blocks
  NoUserspaceExit = 1;

  LastServicedTimeNs = GetTimeInNanoSecond (GetPerformanceCounter ());
  while (NoUserspaceExit) {
    // Check if there is command waiting for us
    if (SerialPortPoll ()) {
      // Stall a tiny bit, in-case the remainder of the packet is flushing
      MicroSecondDelay (10 * MS_IN_SECOND);

      SerialPortRead ((UINT8 *)&CommandPacket, sizeof(CommandPacket));
      switch (CommandPacket.Command) {
        case EARLY_FLASH_RESCUE_COMMAND_CHECKSUM:
          SendBlockChecksum (CommandPacket.BlockNumber);
          break;
        case EARLY_FLASH_RESCUE_COMMAND_WRITE:
          WriteBlock (CommandPacket.BlockNumber);
          break;
        case EARLY_FLASH_RESCUE_COMMAND_RESET:
          PerformSystemReset ();
          // TODO: Fallthrough?
          NoUserspaceExit = 0;
          break;
        case EARLY_FLASH_RESCUE_COMMAND_EXIT:
          NoUserspaceExit = 0;
          break;
        default:
          Print (L"Cannot understand command 0x%x!\n", CommandPacket.Command);
          break;
      }

      LastServicedTimeNs = GetTimeInNanoSecond (GetPerformanceCounter ());
    }

    if ((GetTimeInNanoSecond (GetPerformanceCounter ()) - LastServicedTimeNs) >=
        (10ULL * NS_IN_SECOND)) {
      // This is very bad. SPI flash could be inconsistent
      // - In CAR there's likely too little memory to stash a backup
      Print (L"Fatal error! Userspace has failed to answer for 10s!\n");
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

VOID
EFIAPI
SpiServiceDeInit (
  VOID
  );

/**
  Entry Point function

  @param[in] ImageHandle  Handle to the Image (NULL if internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if internal).
**/
EFI_STATUS
EFIAPI
BusPirateDebugAppEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_TPL     PreviousTpl;
  EFI_STATUS  Status;

  Print (L"BusPirateDebugAppEntryPoint() Start\n");

  Status = SpiServiceInit ();
  if (EFI_ERROR (Status)) {
    Print (L"Failed to init our private SPI service!\n");
    goto End;
  }

  // Step 1
  Print (L"Sending HELLO to userspace...\n");
  Status = SendHelloPacket ();
  if (EFI_ERROR (Status)) {
    Print (L"Userspace failed to acknowledge HELLO!\n");
    goto End;
  }

  Print (L"Userspace acknowledged HELLO.\n");

  // Step 2
  Print (L"Entering flash operations loop...\n");
  Status = PerformFlash ();
  if (EFI_ERROR (Status)) {
    Print (L"Flash operation failed!\n");
    goto End;
  }

  Print (L"Flash operation complete.\n");

End:
  SpiServiceDeInit ();

  Print (L"BusPirateDebugAppEntryPoint() End\n");

  return EFI_SUCCESS;
}
