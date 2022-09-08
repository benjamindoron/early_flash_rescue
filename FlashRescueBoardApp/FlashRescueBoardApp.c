/** @file
  Early SPI flash rescue protocol - board implementation.

  Copyright (c) 2022, Baruch Binyamin Doron.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/Spi2.h>
#include <Library/PchSpiCommonLib.h>

#include "FlashRescueBoard.h"

extern SPI_INSTANCE  *mSpiInstance;


/**
  Returns a pointer to the PCH SPI PPI.

  @return Pointer to PCH_SPI2_PPI   If an instance of the PCH SPI PPI is found
  @return NULL                      If an instance of the PCH SPI PPI is not found

**/
PCH_SPI2_PROTOCOL *
GetSpiPpi (
  VOID
  )
{
  ASSERT (mSpiInstance != NULL);
  return &(mSpiInstance->SpiProtocol);
}

/**
 * Perform system reset to start this firmware.
**/
VOID
EFIAPI
PerformSystemReset (
  VOID
  )
{
  DEBUG ((DEBUG_ERROR, "FIXME: Refusing to restart!\n"));
  DEBUG ((DEBUG_INFO, "Optionally verify the region with FPT\n"));
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
FlashRescueBoardAppEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Print (L"FlashRescueBoardAppEntryPoint() Start\n");

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

  Print (L"FlashRescueBoardAppEntryPoint() End\n");

  return EFI_SUCCESS;
}
