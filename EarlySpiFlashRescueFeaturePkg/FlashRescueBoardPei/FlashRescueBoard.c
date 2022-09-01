/** @file
  Early SPI flash rescue protocol - board implementation

  Copyright (c) 2022, Baruch Binyamin Doron.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/PeCoffLib.h>
#include <Ppi/FeatureInMemory.h>

STATIC CONST EFI_PEI_PPI_DESCRIPTOR  mFlashRescueReadyInMemoryPpiList = {
  EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
  &gPeiFlashRescueReadyInMemoryPpiGuid,
  NULL
};

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
  return EFI_SUCCESS;
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
  DEBUG ((DEBUG_INFO, "Hello from %a()!\n", __FUNCTION__));

  return EFI_SUCCESS;
}

/**
  Entry Point function
  TODO: Stop using DebugLib.

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

  DEBUG ((DEBUG_INFO, "%a() Start\n", __FUNCTION__));

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

    goto End;
  }

  //
  // First entry: Establish communication with board or don't reload
  //
  Status = SendHelloPacket ();
  if (EFI_ERROR (Status)) {
    goto End;
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

  PeimEntryPoint = (EFI_PEIM_ENTRY_POINT2)(UINTN)ImageContext.EntryPoint;
  Status = PeimEntryPoint (FileHandle, PeiServices);
  ASSERT_EFI_ERROR (Status);

  //
  // Cleanup
  //
  Status = PeiServicesFreePages (
             PeimCopy,
             EFI_SIZE_TO_PAGES ((UINT32)ImageContext.ImageSize)
             );
  ASSERT_EFI_ERROR (Status);

End:
  DEBUG ((DEBUG_INFO, "%a() End\n", __FUNCTION__));

  return EFI_SUCCESS;
}
