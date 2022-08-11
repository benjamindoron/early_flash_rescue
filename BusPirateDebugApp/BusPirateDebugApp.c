/** @file
  Early SPI flash rescue protocol - board implementation

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

#define SIZE_BLOCK    4096
#define MS_IN_SECOND  1000
#define NS_IN_SECOND  (1000 * 1000 * 1000)

#define EARLY_FLASH_RESCUE_PROTOCOL_VERSION 0.1
#define EARLY_FLASH_RESCUE_COMMAND_HELLO    0x10
#define EARLY_FLASH_RESCUE_COMMAND_CHECKSUM 0x11
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

VOID
EFIAPI
InternalPrintData (
  IN UINT8   *Data8,
  IN UINTN   DataSize
  )
{
  UINTN      Index;

  for (Index = 0; Index < DataSize; Index++) {
    if (Index % 0x10 == 0) {
      Print (L"\n%08X:", Index);
    }
    Print (L" %02X", *Data8++);
  }
  Print (L"\n");
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

  // `BlockNumber` starting in BIOS region
  Address = BlockNumber * SIZE_BLOCK;
  BlockData = AllocatePool (SIZE_BLOCK);

  Print (L"Writing address 0x%x\n", Address);

  // Acknowledge userspace command and retrieve block
  ResponsePacket.Acknowledge = 1;
  SerialPortWrite ((UINT8 *)&ResponsePacket, sizeof(ResponsePacket));

  // Start streaming block
  XferBlock = BlockData;
  UINTN TODO_readbytes;
  for (Index = 0; Index < SIZE_BLOCK; Index += XferBlockSize) {
    TODO_readbytes = SerialPortRead (XferBlock, XferBlockSize);
    XferBlock += XferBlockSize;
  }

  // TODO: Debugging
  InternalPrintData (BlockData, SIZE_BLOCK);
  Print (L"Yeah, read bytes from SerialPort is %d\n", TODO_readbytes);

End:
  FreePool (BlockData);
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
      SerialPortRead ((UINT8 *)&CommandPacket, sizeof(CommandPacket));
      switch (CommandPacket.Command) {
        case EARLY_FLASH_RESCUE_COMMAND_CHECKSUM:
          SendBlockChecksum (CommandPacket.BlockNumber);
          break;
        case EARLY_FLASH_RESCUE_COMMAND_WRITE:
          WriteBlock (CommandPacket.BlockNumber);
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

  Print (L"Userspace acknowledged HELLO!\n");

  // Step 2
  Print (L"Entering flash loop...\n");
  Status = PerformFlash ();
  if (EFI_ERROR (Status)) {
    Print (L"Flash operation failed!\n");
    goto End;
  }

  Print (L"Flash operation complete.");

End:
  SpiServiceDeInit ();

  Print (L"BusPirateDebugAppEntryPoint() End\n");

  return EFI_SUCCESS;
}
