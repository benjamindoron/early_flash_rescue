/** @file
  Early SPI flash rescue protocol - board implementation.

  Copyright (c) 2022, Baruch Binyamin Doron.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PeCoffLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/SpiLib.h>
#include <Library/TimerLib.h>
#include <Ppi/FeatureInMemory.h>
#include <Ppi/Spi2.h>
#include "FlashRescueBoard.h"

STATIC CONST EFI_PEI_PPI_DESCRIPTOR  mFlashRescueReadyInMemoryPpiList = {
  EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
  &gPeiFlashRescueReadyInMemoryPpiGuid,
  NULL
};

/**
  Returns a pointer to the PCH SPI PPI.

  @return Pointer to PCH_SPI2_PPI   If an instance of the PCH SPI PPI is found
  @return NULL                      If an instance of the PCH SPI PPI is not found

**/
PCH_SPI2_PPI *
GetSpiPpi (
  VOID
  )
{
  EFI_STATUS    Status;
  PCH_SPI2_PPI  *PchSpi2Ppi;

  Status =  PeiServicesLocatePpi (
              &gPchSpi2PpiGuid,
              0,
              NULL,
              (VOID **) &PchSpi2Ppi
              );
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return PchSpi2Ppi;
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
  //
  // PPI may be unavailable, but do not risk UAF. Must use silicon variant
  //
  ResetCold ();
}

/**
  Entry Point function

  @param[in] FileHandle  Handle of the file being invoked.
  @param[in] PeiServices Describes the list of possible PEI Services.

  @retval EFI_SUCCESS          if it completed successfully.
  @retval EFI_OUT_OF_RESOURCES if PEIM cannot be reloaded.
**/
EFI_STATUS
EFIAPI
FlashRescueBoardPeiEntryPoint (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                    Status;
  VOID                          *FlashRescueReady;
  VOID                          *ThisPeimData;
  PE_COFF_LOADER_IMAGE_CONTEXT  ImageContext;
  EFI_PHYSICAL_ADDRESS          PeimCopy;
  EFI_PEIM_ENTRY_POINT2         PeimEntryPoint;

  //
  // Second entry: Enter flash loop
  //
  Status = PeiServicesLocatePpi (
             &gPeiFlashRescueReadyInMemoryPpiGuid,
             0,
             NULL,
             (VOID **)&FlashRescueReady
             );
  if (!EFI_ERROR (Status)) {
    Status = PerformFlash ();
    ASSERT_EFI_ERROR (Status);

    return EFI_SUCCESS;
  }

  //
  // First entry: Establish communication with board or don't reload
  //
  DEBUG ((DEBUG_INFO, "HELLO begins. Re-connect with userspace-side now\n"));
  MicroSecondDelay (3000 * MS_IN_SECOND);

  Status = SendHelloPacket ();
  if (EFI_ERROR (Status)) {
    return EFI_SUCCESS;
  }

  //
  // Find this PEIM, then obtain a PE context with its data handle
  //
  Status = PeiServicesFfsFindSectionData (
             EFI_SECTION_PE32,
             FileHandle,
             &ThisPeimData
             );
  ASSERT_EFI_ERROR (Status);

  ZeroMem (&ImageContext, sizeof (ImageContext));

  ImageContext.Handle    = ThisPeimData;
  ImageContext.ImageRead = PeCoffLoaderImageReadFromMemory;

  Status = PeCoffLoaderGetImageInfo (&ImageContext);
  ASSERT_EFI_ERROR (Status);

  //
  // Allocate memory from NEM or DRAM
  // - RegisterForShadow() is simpler for DRAM, but simplify code paths
  //
  Status = PeiServicesAllocatePages (
               EfiBootServicesCode,
               EFI_SIZE_TO_PAGES ((UINT32)ImageContext.ImageSize),
               &PeimCopy
               );
  if (EFI_ERROR (Status)) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Load and relocate into the new buffer
  //
  ImageContext.ImageAddress = PeimCopy;
  Status = PeCoffLoaderLoadImage (&ImageContext);
  ASSERT_EFI_ERROR (Status);

  Status = PeCoffLoaderRelocateImage (&ImageContext);
  ASSERT_EFI_ERROR (Status);

  //
  // Install flag PPI and call entrypoint
  //
  PeiServicesInstallPpi (&mFlashRescueReadyInMemoryPpiList);
  ASSERT_EFI_ERROR (Status);

  DEBUG ((DEBUG_INFO, "ATTN: This PEIM copied to 0x%x\n", ImageContext.ImageAddress));

  PeimEntryPoint = (EFI_PEIM_ENTRY_POINT2)(UINTN)ImageContext.EntryPoint;
  Status = PeimEntryPoint (FileHandle, PeiServices);
  ASSERT_EFI_ERROR (Status);

  //
  // Cleanup. It is important that PPIs do not point here
  //
  Status = SpiServiceInit ();
  ASSERT_EFI_ERROR (Status);

  Status = PeiServicesFreePages (
             PeimCopy,
             EFI_SIZE_TO_PAGES ((UINT32)ImageContext.ImageSize)
             );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
